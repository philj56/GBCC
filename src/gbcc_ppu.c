#include "gbcc.h"
#include "gbcc_bit_utils.h"
#include "gbcc_colour.h"
#include "gbcc_debug.h"
#include "gbcc_hdma.h"
#include "gbcc_memory.h"
#include "gbcc_palettes.h"
#include "gbcc_ppu.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BACKGROUND_MAP_BANK_1 0x9800u
#define BACKGROUND_MAP_BANK_2 0x9C00u
#define NUM_SPRITES 40

/* 
 * Guide to line buffer attribute bits:
 * 0 - is this pixel being drawn?
 * 1 - is this colour 0?
 * 2 - priority bit
 */

#define ATTR_DRAWN (bit(0))
#define ATTR_COLOUR0 (bit(1))
#define ATTR_PRIORITY (bit(2))

enum palette_flag { BACKGROUND, SPRITE_1, SPRITE_2 };

static void draw_line(struct gbc *gbc);
static void draw_background_line(struct gbc *gbc);
static void draw_window_line(struct gbc *gbc);
static void draw_sprite_line(struct gbc *gbc);
static void composite_line(struct gbc *gbc);
static uint8_t get_video_mode(struct gbc *gbc);
static void set_video_mode(struct gbc *gbc, uint8_t mode);
static uint32_t get_palette_colour(struct gbc *gbc, uint8_t palette, uint8_t n, enum palette_flag pf);

void gbcc_disable_lcd(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	ppu->lcd_disable = true;
}

void gbcc_enable_lcd(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	if (!ppu->lcd_disable) {
		return;
	}
	ppu->lcd_disable = false;
	ppu->clock = 4;
	set_video_mode(gbc, GBC_LCD_MODE_OAM_READ);
	gbcc_memory_write(gbc, LY, 0, true);
}

void gbcc_ppu_clock(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	if (ppu->lcd_disable) {
		return;
	}
	uint8_t mode = get_video_mode(gbc);
	uint8_t ly = gbcc_memory_read(gbc, LY, true);
	uint8_t stat = gbcc_memory_read(gbc, STAT, true);
	ppu->clock++;
	ppu->clock %= 456;

	/* Line-drawing timings */
	if (mode != GBC_LCD_MODE_VBLANK) {
		if (ppu->clock == 0) {
			set_video_mode(gbc, GBC_LCD_MODE_OAM_READ);
			if (check_bit(stat, 5)) {
				gbcc_memory_set_bit(gbc, IF, 1, true);
			}
		} else if (ppu->clock == GBC_LCD_MODE_PERIOD) {
			set_video_mode(gbc, GBC_LCD_MODE_OAM_VRAM_READ);
		} else if (ppu->clock == (3 * GBC_LCD_MODE_PERIOD)) {
			draw_line(gbc);
			set_video_mode(gbc, GBC_LCD_MODE_HBLANK);
			gbcc_hdma_copy_block(gbc);
			if (check_bit(stat, 3)) {
				gbcc_memory_set_bit(gbc, IF, 1, true);
			}
		}
	}
	/* LCD STAT Interrupt */
	if (ly != 0 && ly == gbcc_memory_read(gbc, LYC, true)) {
		if (ppu->clock == 4) {
			gbcc_memory_set_bit(gbc, STAT, 2, true);
			if (check_bit(stat, 6)) {
				gbcc_memory_set_bit(gbc, IF, 1, true);
			}
		}
	} else {
		gbcc_memory_clear_bit(gbc, STAT, 2, true);
	}
	if (ppu->clock == 0) {
		ly++;
	}
	/* VBLANK interrupt flag */
	if (ly == 144) {
		if (ppu->clock == 4) {
			gbcc_memory_set_bit(gbc, IF, 0, true);
			set_video_mode(gbc, GBC_LCD_MODE_VBLANK);
			if (check_bit(stat, 4)) {
				gbcc_memory_set_bit(gbc, IF, 1, true);
			}
			
			uint32_t *tmp = ppu->screen.gbc;
			ppu->screen.gbc = ppu->screen.sdl;
			ppu->screen.sdl = tmp;
		}
	} else if (ly == 154) {
		ppu->frame++;
		ly = 0;
		set_video_mode(gbc, GBC_LCD_MODE_OAM_READ);
	}
	gbcc_memory_write(gbc, LY, ly, true);
}

void draw_line(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	uint8_t ly = gbcc_memory_read(gbc, LY, true);
	if (ly > GBC_SCREEN_HEIGHT) {
		return;
	}
	memset(ppu->bg_line.attr, 0, sizeof(ppu->bg_line.attr));
	memset(ppu->window_line.attr, 0, sizeof(ppu->window_line.attr));
	memset(ppu->sprite_line.attr, 0, sizeof(ppu->sprite_line.attr));

	if (!gbc->interlace || ((ppu->frame) % 2 == ly % 2)) {
		draw_background_line(gbc);
		draw_window_line(gbc);
		draw_sprite_line(gbc);
		composite_line(gbc);
	} else {
		for (uint8_t x = 0; x < GBC_SCREEN_WIDTH; x++) {
			uint32_t p = ppu->screen.sdl[ly * GBC_SCREEN_WIDTH + x]; 
			uint8_t r = (p & 0xFF0000u) >> 17u;
			uint8_t g = (p & 0x00FF00u) >> 9u;
			uint8_t b = (p & 0x0000FFu) >> 1u;
			p = (uint32_t)(r << 16u) | (uint32_t)(g << 8u) | b;
			ppu->screen.gbc[ly * GBC_SCREEN_WIDTH + x] = p;
		}
	}
}

/* TODO: GBC BG-to-OAM Priority */
void draw_background_line(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	uint8_t scy = gbcc_memory_read(gbc, SCY, true);
	uint8_t scx = gbcc_memory_read(gbc, SCX, true);
	uint8_t ly = gbcc_memory_read(gbc, LY, true);
	uint8_t palette = gbcc_memory_read(gbc, BGP, true);
	uint8_t lcdc = gbcc_memory_read(gbc, LCDC, true);

	uint8_t ty = ((scy + ly) / 8u) % 32u;
	uint16_t line_offset = 2 * ((scy + ly) % 8u); /* 2 bytes per line */
	uint16_t map;
	if (check_bit(lcdc, 3)) {
		map = BACKGROUND_MAP_BANK_2;
	} else {
		map = BACKGROUND_MAP_BANK_1;
	}

	if (!check_bit(lcdc, 0)) {
		uint32_t colour;
		if (gbc->mode == DMG) {
			colour = get_palette_colour(gbc, 0, 0, BACKGROUND);
		} else {
			colour = 0xFF;
		}
		for (size_t x = 0; x < GBC_SCREEN_WIDTH; x++) {
			ppu->bg_line.colour[x] = colour;
		}
		return;
	}

	if (gbc->mode == DMG) {
		for (size_t x = 0; x < GBC_SCREEN_WIDTH; x++) {
			uint8_t tx = ((scx + x) / 8u) % 32u;
			uint8_t xoff = (scx + x) % 8u;
			uint8_t tile = gbcc_memory_read(gbc, map + 32 * ty + tx, true);
			uint16_t tile_addr;
			if (check_bit(lcdc, 4)) {
				tile_addr = VRAM_START + 16 * tile;
			} else {
				tile_addr = (uint16_t)(0x9000 + 16 * (int8_t)tile);
			}
			uint8_t lo = gbcc_memory_read(gbc, tile_addr + line_offset, true);
			uint8_t hi = gbcc_memory_read(gbc, tile_addr + line_offset + 1, true);
			uint8_t colour = (uint8_t)(check_bit(hi, 7 - xoff) << 1u) | check_bit(lo, 7 - xoff);
			ppu->bg_line.colour[x] = get_palette_colour(gbc, palette, colour, BACKGROUND);
			ppu->bg_line.attr[x] |= ATTR_DRAWN;
			if (colour == 0) {
				ppu->bg_line.attr[x] |= ATTR_COLOUR0;
			}
		}
	} else {
		for (size_t x = 0; x < GBC_SCREEN_WIDTH; x++) {
			uint8_t tx = ((scx + x) / 8u) % 32u;
			uint8_t xoff = (scx + x) % 8u;
			uint8_t tile = gbc->memory.vram_bank[0][map + 32 * ty + tx - VRAM_START];
			uint8_t attr = gbc->memory.vram_bank[1][map + 32 * ty + tx - VRAM_START];
			uint8_t *vbk;
			uint16_t tile_addr;
			if (check_bit(attr, 3)) {
				vbk = gbc->memory.vram_bank[1];
			} else {
				vbk = gbc->memory.vram_bank[0];
			}
			/* TODO: Put this somewhere better */
			if (check_bit(lcdc, 4)) {
				tile_addr = 16 * tile;
			} else {
				tile_addr = (uint16_t)(0x1000 + 16 * (int8_t)tile);
			}
			uint8_t lo;
			uint8_t hi;
			/* Check for Y-flip */
			if (check_bit(attr, 6)) {
				lo = vbk[tile_addr + (14 - line_offset)];
				hi = vbk[tile_addr + (14 - line_offset) + 1];
			} else {
				lo = vbk[tile_addr + line_offset];
				hi = vbk[tile_addr + line_offset + 1];
			}
			uint8_t colour;
			/* Check for X-flip */
			if (check_bit(attr, 5)) {
				colour = (uint8_t)(check_bit(hi, xoff) << 1u) | check_bit(lo, xoff);
			} else {
				colour = (uint8_t)(check_bit(hi, 7 - xoff) << 1u) | check_bit(lo, 7 - xoff);
			}
			ppu->bg_line.colour[x] = get_palette_colour(gbc, attr & 0x07u, colour, BACKGROUND);
			ppu->bg_line.attr[x] |= ATTR_DRAWN;
			if (colour == 0) {
				ppu->bg_line.attr[x] |= ATTR_COLOUR0;
			}
			if (check_bit(attr, 7)) {
				ppu->bg_line.attr[x] |= ATTR_PRIORITY;
			}
		}
	}
}

void draw_window_line(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	uint8_t wy = gbcc_memory_read(gbc, WY, true);
	uint8_t wx = gbcc_memory_read(gbc, WX, true);
	uint8_t ly = gbcc_memory_read(gbc, LY, true);
	uint8_t palette = gbcc_memory_read(gbc, BGP, true);
	uint8_t lcdc = gbcc_memory_read(gbc, LCDC, true);

	if (ly < wy || !check_bit(lcdc, 5) || (gbc->mode == DMG && !check_bit(lcdc, 0))) {
		return;
	}

	uint8_t ty = ((ly - wy) / 8u) % 32u;
	uint16_t line_offset = 2 * ((ly - wy) % 8);
	uint16_t map;
	if (check_bit(lcdc, 6)) {
		map = BACKGROUND_MAP_BANK_2;
	} else {
		map = BACKGROUND_MAP_BANK_1;
	}

	if (gbc->mode == DMG) {
		for (int x = 0; x < GBC_SCREEN_WIDTH; x++) {
			if (x < wx - 7) {
				continue;
			}
			uint8_t tx = ((x - wx + 7) / 8) % 32;
			uint8_t xoff = (x - wx + 7) % 8;
			uint8_t tile = gbcc_memory_read(gbc, map + 32 * ty + tx, true);
			uint16_t tile_addr;
			if (check_bit(lcdc, 4)) {
				tile_addr = VRAM_START + 16 * tile;
			} else {
				tile_addr = (uint16_t)(0x9000 + 16 * (int8_t)tile);
			}
			uint8_t lo = gbcc_memory_read(gbc, tile_addr + line_offset, true);
			uint8_t hi = gbcc_memory_read(gbc, tile_addr + line_offset + 1, true);
			uint8_t colour = (uint8_t)(check_bit(hi, 7 - xoff) << 1u) | check_bit(lo, 7 - xoff);
			ppu->window_line.colour[x] = get_palette_colour(gbc, palette, colour, BACKGROUND);
			ppu->window_line.attr[x] |= ATTR_DRAWN;
			if (colour == 0) {
				ppu->window_line.attr[x] |= ATTR_COLOUR0;
			}
		}
	} else {
		for (int x = 0; x < GBC_SCREEN_WIDTH; x++) {
			if (x < wx - 7) {
				continue;
			}
			uint8_t tx = ((x - wx + 7) / 8) % 32;
			uint8_t xoff = (x - wx + 7) % 8;
			uint8_t tile = gbc->memory.vram_bank[0][map + 32 * ty + tx - VRAM_START];
			uint8_t attr = gbc->memory.vram_bank[1][map + 32 * ty + tx - VRAM_START];
			uint8_t *vbk;
			uint16_t tile_addr;
			if (check_bit(attr, 3)) {
				vbk = gbc->memory.vram_bank[1];
			} else {
				vbk = gbc->memory.vram_bank[0];
			}
			/* TODO: Put this somewhere better */
			if (check_bit(lcdc, 4)) {
				tile_addr = 16 * tile;
			} else {
				tile_addr = (uint16_t)(0x1000 + 16 * (int8_t)tile);
			}
			uint8_t lo;
			uint8_t hi;
			/* Check for Y-flip */
			if (check_bit(attr, 6)) {
				lo = vbk[tile_addr + (14 - line_offset)];
				hi = vbk[tile_addr + (14 - line_offset) + 1];
			} else {
				lo = vbk[tile_addr + line_offset];
				hi = vbk[tile_addr + line_offset + 1];
			}
			uint8_t colour;
			/* Check for X-flip */
			if (check_bit(attr, 5)) {
				colour = (uint8_t)(check_bit(hi, xoff) << 1u) | check_bit(lo, xoff);
			} else {
				colour = (uint8_t)(check_bit(hi, 7 - xoff) << 1u) | check_bit(lo, 7 - xoff);
			}
			ppu->window_line.colour[x] = get_palette_colour(gbc, attr & 0x07u, colour, BACKGROUND);
			ppu->window_line.attr[x] |= ATTR_DRAWN;
			if (colour == 0) {
			    ppu->window_line.attr[x] |= ATTR_COLOUR0;
			}
			if (check_bit(attr, 7)) {
				ppu->window_line.attr[x] |= ATTR_PRIORITY;
			}
		}
	}
}

void draw_sprite_line(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	uint8_t ly = gbcc_memory_read(gbc, LY, true);
	uint8_t lcdc = gbcc_memory_read(gbc, LCDC, true);
	enum palette_flag pf;
	if (!check_bit(lcdc, 1)) {
		return;
	}
	bool double_size = check_bit(lcdc, 2); /* 1 or 2 8x8 tiles */
	uint8_t n_drawn = 0;
	for (size_t s = 0; s < NUM_SPRITES && n_drawn < 10; s++) {
		/* 
		 * Together SY & SX define the bottom right corner of the
		 * sprite. X=1, Y=1 is the top left corner, so each of these
		 * are offset by 1 for array values.
		 * TODO: Is this off-by-one true?
		 */
		uint8_t sy = gbcc_memory_read(gbc, OAM_START + 4 * s, true);
		uint8_t sx = gbcc_memory_read(gbc, OAM_START + 4 * s + 1, true);
		uint8_t tile = gbcc_memory_read(gbc, OAM_START + 4 * s + 2, true);
		uint8_t attr = gbcc_memory_read(gbc, OAM_START + 4 * s + 3, true);
		bool priority = check_bit(attr, 7);
		bool yflip = check_bit(attr, 6);
		bool xflip = check_bit(attr, 5);
		uint8_t sprite_line = sy - ly;
		uint8_t *vram_bank;
		if (gbc->mode == DMG) {
			vram_bank = gbc->memory.vram_bank[0];
		} else {
			vram_bank = gbc->memory.vram_bank[check_bit(attr, 3)];
		}
		if (ly >= sy || ly + 16 < sy || (!double_size && ly + 8 >= sy)) {
			/* Sprite isn't on this line */
			continue;
		}
		if (double_size) {
			/* 
			 * In 8x16 mode, the lower bit of the tile number is
			 * ignored, and the sprite is stored as upper tile then
			 * lower tile.
			 */
			if (ly + 8 < sy) {
				/* We are drawing the top tile */
				tile = cond_bit(tile, 0, yflip);
			} else {
				/* We are drawing the bottom tile */
				tile = cond_bit(tile, 0, !yflip);
				sprite_line += 8;
			}
		}
		if (yflip) {
			sprite_line -= 9;
		} else {
			sprite_line = 16 - sprite_line;
		}
		uint8_t palette;
		if (gbc->mode == DMG) {
			if (check_bit(attr, 4)) {
				palette = gbcc_memory_read(gbc, OBP1, true);
				pf = SPRITE_1;
			} else {
				palette = gbcc_memory_read(gbc, OBP0, true);
				pf = SPRITE_2;
			}
		} else {
			palette = attr & 0x07u;
			pf = SPRITE_1;
		}
		uint8_t	lo = vram_bank[16 * tile + 2 * sprite_line];
		uint8_t	hi = vram_bank[16 * tile + 2 * sprite_line + 1];
		for (uint8_t x = 0; x < 8; x++) {
			uint8_t colour = (uint8_t)(check_bit(hi, 7 - x) << 1u) | check_bit(lo, 7 - x);
			/* Colour 0 is transparent */
			if (!colour) {
				continue;
			}
			uint8_t screen_x;
			if (xflip) {
				screen_x = sx - x - 1;
			} else {
				screen_x = sx + x - 8;
			}
			if (screen_x < GBC_SCREEN_WIDTH) {
				ppu->sprite_line.colour[screen_x] = get_palette_colour(gbc, palette, colour, pf);
				ppu->sprite_line.attr[screen_x] |= ATTR_DRAWN;
				if (priority) {
					ppu->sprite_line.attr[screen_x] |= ATTR_PRIORITY;
				}
			}
		}
		n_drawn++;
	}
}

void composite_line(struct gbc *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	uint8_t ly = gbcc_memory_read(gbc, LY, true);
	uint32_t *line = &ppu->screen.gbc[ly * GBC_SCREEN_WIDTH];

	memcpy(line, ppu->bg_line.colour, GBC_SCREEN_WIDTH * sizeof(line[0]));
	for (uint8_t x = 0; x < GBC_SCREEN_WIDTH; x++) {
		uint8_t bg_attr = ppu->bg_line.attr[x];
		uint8_t win_attr = ppu->window_line.attr[x];
		uint8_t ob_attr = ppu->sprite_line.attr[x];

		if (win_attr & ATTR_DRAWN) {
			line[x] = ppu->window_line.colour[x];
		}

		if (ob_attr & ATTR_DRAWN) {
			if ((win_attr & ATTR_DRAWN)) {
				if (win_attr & ATTR_PRIORITY) {
					continue;
				}
				if (ob_attr & ATTR_PRIORITY) {
					continue;
				}
			}
			if ((bg_attr & ATTR_DRAWN)) {
				if (bg_attr & ATTR_PRIORITY && !(bg_attr & ATTR_COLOUR0)) {
					continue;
				}
				if (!(bg_attr & ATTR_COLOUR0) && (ob_attr & ATTR_PRIORITY)) {
					continue;
				}
			}
			line[x] = ppu->sprite_line.colour[x];
		}
	}
}

uint8_t get_video_mode(struct gbc *gbc)
{
	return gbcc_memory_read(gbc, STAT, true) & 0x03u;
}

void set_video_mode(struct gbc *gbc, uint8_t mode)
{
	uint8_t stat = gbcc_memory_read(gbc, STAT, true);
	stat &= 0xFCu;
	stat |= mode;
	gbcc_memory_write(gbc, STAT, stat, true);
}

uint32_t get_palette_colour(struct gbc *gbc, uint8_t palette, uint8_t n, enum palette_flag pf)
{
	struct ppu *ppu = &gbc->ppu;
	if (gbc->mode == DMG) {
		uint8_t colours[4] = {
			(palette & 0x03u) >> 0u,
			(palette & 0x0Cu) >> 2u,
			(palette & 0x30u) >> 4u,
			(palette & 0xC0u) >> 6u
		};
		switch (pf) {
			case BACKGROUND:
				return ppu->palette.background[colours[n]];
			case SPRITE_1:
				return ppu->palette.sprite1[colours[n]];
			case SPRITE_2:
				return ppu->palette.sprite2[colours[n]];
		}
	}
	uint8_t index = palette * 8;
	uint8_t lo;
	uint8_t hi;
	if (pf != BACKGROUND) {
		lo = ppu->obp[index + 2 * n];
		hi = ppu->obp[index + 2 * n + 1];
	} else {
		lo = ppu->bgp[index + 2 * n];
		hi = ppu->bgp[index + 2 * n + 1];
	} 
	uint8_t r = lo & 0x1Fu;
	uint8_t g = ((lo & 0xE0u) >> 5u) | (uint8_t)((hi & 0x03u) << 3u);
	uint8_t b = (hi & 0x7Cu) >> 2u;
	uint32_t res = 0;
	res |= (uint32_t)(r << 27u);
	res |= (uint32_t)(g << 19u);
	res |= (uint32_t)(b << 11u);
	res |= 0xFFu;

	return res;
}

