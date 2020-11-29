/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "../gbcc.h"
#include "../debug.h"
#include "../input.h"
#include "../nelem.h"
#include "../time_diff.h"
#include "../window.h"
#include "sdl.h"
#include "vram_window.h"
#include <errno.h>
#include <png.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ICON_PATH
#define ICON_PATH "icons"
#endif

#define HEADER_BYTES 8

static const SDL_Scancode keymap[36] = {
	SDL_SCANCODE_Z,		/* A */
	SDL_SCANCODE_X, 	/* B */
	SDL_SCANCODE_RETURN,	/* Start */
	SDL_SCANCODE_SPACE,	/* Select */
	SDL_SCANCODE_UP,	/* DPAD up */
	SDL_SCANCODE_DOWN,	/* DPAD down */
	SDL_SCANCODE_LEFT,	/* DPAD left */
	SDL_SCANCODE_RIGHT,	/* DPAD right */
	SDL_SCANCODE_RSHIFT, 	/* Turbo */
	SDL_SCANCODE_S, 	/* Screenshot */
	SDL_SCANCODE_P, 	/* Pause */
	SDL_SCANCODE_F, 	/* FPS Counter / frame blending */
	SDL_SCANCODE_V, 	/* Vsync / VRAM display */
	SDL_SCANCODE_1, 	/* Toggle background */
	SDL_SCANCODE_2, 	/* Toggle window */
	SDL_SCANCODE_3, 	/* Toggle sprites */
	SDL_SCANCODE_L, 	/* Toggle link cable loop */
	SDL_SCANCODE_A, 	/* Toggle autosave */
	SDL_SCANCODE_B, 	/* Toggle background playback */
	SDL_SCANCODE_ESCAPE, 	/* Toggle menu */
	SDL_SCANCODE_I,         /* Toggle interlacing */
	SDL_SCANCODE_O,         /* Cycle through shaders */
	SDL_SCANCODE_C,         /* Toggle cheats */
	SDL_SCANCODE_KP_2, 	/* MBC7 accelerometer down */
	SDL_SCANCODE_KP_4, 	/* MBC7 accelerometer left */
	SDL_SCANCODE_KP_6, 	/* MBC7 accelerometer right */
	SDL_SCANCODE_KP_8, 	/* MBC7 accelerometer up */
	SDL_SCANCODE_F1,	/* State n */
	SDL_SCANCODE_F2,
	SDL_SCANCODE_F3,
	SDL_SCANCODE_F4,
	SDL_SCANCODE_F5,
	SDL_SCANCODE_F6,
	SDL_SCANCODE_F7,
	SDL_SCANCODE_F8,
	SDL_SCANCODE_F9
};

static const SDL_GameControllerButton buttonmap[8] = {
	SDL_CONTROLLER_BUTTON_B,
	SDL_CONTROLLER_BUTTON_A,
	SDL_CONTROLLER_BUTTON_START,
	SDL_CONTROLLER_BUTTON_BACK,
	SDL_CONTROLLER_BUTTON_DPAD_UP,
	SDL_CONTROLLER_BUTTON_DPAD_DOWN,
	SDL_CONTROLLER_BUTTON_DPAD_LEFT,
	SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};

// Returns key that changed, or -1 for a non-emulated key
static int process_input(struct gbcc_sdl *sdl, const SDL_Event *e);
static void process_game_controller(struct gbcc_sdl *sdl);
static void *init_input(void *_);
static void set_icon(SDL_Window *win, const char *filename);

void gbcc_sdl_initialise(struct gbcc_sdl *sdl)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		gbcc_log_error("Failed to initialize SDL: %s\n", SDL_GetError());
	}

	sdl->game_controller = NULL;

	{
		/*
		 * SDL_INIT_GAMECONTROLLER is very slow, so we spin it off into
		 * its own thread here.
		 */
		pthread_t init_thread;
		pthread_create(&init_thread, NULL, init_input, NULL);
		pthread_detach(init_thread);
	}
	
	/* Main OpenGL settings */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);


	sdl->window = SDL_CreateWindow(
			"GBCC",                    // window title
			SDL_WINDOWPOS_UNDEFINED,   // initial x position
			SDL_WINDOWPOS_UNDEFINED,   // initial y position
			GBC_SCREEN_WIDTH,      // width, in pixels
			GBC_SCREEN_HEIGHT,     // height, in pixels
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI// flags
			);

	if (sdl->window == NULL) {
		gbcc_log_error("Could not create window: %s\n", SDL_GetError());
		SDL_Quit();
		exit(EXIT_FAILURE);
	}

	SDL_SetWindowMinimumSize(sdl->window, GBC_SCREEN_WIDTH, GBC_SCREEN_HEIGHT);

	set_icon(sdl->window, ICON_PATH "icon-32x32.png");

	sdl->context = SDL_GL_CreateContext(sdl->window);
	SDL_GL_MakeCurrent(sdl->window, sdl->context);

	gbcc_window_initialise(&sdl->gbc);
	gbcc_menu_init(&sdl->gbc);
}

void gbcc_sdl_destroy(struct gbcc_sdl *sdl)
{
	if (sdl->game_controller != NULL) {
		SDL_GameControllerClose(sdl->game_controller);
		sdl->game_controller = NULL;
	}
	SDL_GL_MakeCurrent(sdl->window, sdl->context);
	gbcc_window_deinitialise(&sdl->gbc);
	SDL_GL_DeleteContext(sdl->context);
	SDL_DestroyWindow(sdl->window);
}

void gbcc_sdl_update(struct gbcc_sdl *sdl)
{
	struct gbcc *gbc = &sdl->gbc;
	struct gbcc_window *win = &gbc->window;

	/* Hide the cursor after 2 seconds of inactivity */
	struct timespec cur_time;
	clock_gettime(CLOCK_REALTIME, &cur_time);
	if (gbcc_time_diff(&cur_time, &sdl->last_cursor_move) > 2 * SECOND) {
		SDL_ShowCursor(SDL_DISABLE);
	}

	if (gbc->vram_display) {
		if (!gbc->vram_window.initialised) {
			gbcc_sdl_vram_window_initialise(sdl);

			/* 
			 * Workaround to allow input on vram window.
			 * The VRAM window immediately grabs input focus,
			 * causing an SDL_KEYDOWN to fire, and closing the
			 * window instantly :). To stop that, we make sure that
			 * the main window has focus, and then discard any
			 * SDL_KEYDOWN events.
			 */
			SDL_RaiseWindow(sdl->window);
			SDL_PumpEvents();
			SDL_FlushEvent(SDL_KEYDOWN);
		}
		gbcc_sdl_vram_window_update(sdl);
	} else if (gbc->vram_window.initialised) {
		gbcc_sdl_vram_window_destroy(sdl);
	}

	SDL_GL_MakeCurrent(sdl->window, sdl->context);
	SDL_GL_GetDrawableSize(sdl->window, &win->width, &win->height);
	gbcc_window_update(gbc);
	SDL_GL_SwapWindow(sdl->window);
}

void gbcc_sdl_process_input(struct gbcc_sdl *sdl)
{
	struct gbcc *gbc = &sdl->gbc;
	int jx = SDL_GameControllerGetAxis(sdl->game_controller, SDL_CONTROLLER_AXIS_LEFTX);
	int jy = SDL_GameControllerGetAxis(sdl->game_controller, SDL_CONTROLLER_AXIS_LEFTY);
	SDL_Event e;
	const uint8_t *state = SDL_GetKeyboardState(NULL);
	while (SDL_PollEvent(&e) != 0) {
		int key = process_input(sdl, &e);
		enum gbcc_key emulator_key;
		bool val;
		if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) {
			val = true;
		} else if (e.type == SDL_KEYUP || e.type == SDL_CONTROLLERBUTTONUP) {
			val = false;
		} else if (e.type == SDL_CONTROLLERAXISMOTION) {
			if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
				val = (abs(e.caxis.value) > abs(jx));
			} else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
				val = (abs(e.caxis.value) > abs(jy));
			} else {
				continue;
			}
		} else if (e.type == SDL_MOUSEMOTION) {
			clock_gettime(CLOCK_REALTIME, &sdl->last_cursor_move);
			SDL_ShowCursor(SDL_ENABLE);
			continue;
		} else {
			continue;
		}
		if (val && e.key.keysym.scancode == SDL_SCANCODE_F11) {
			sdl->fullscreen = !sdl->fullscreen;
			if (sdl->fullscreen) {
				SDL_SetWindowFullscreen(sdl->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			} else {
				SDL_SetWindowFullscreen(sdl->window, 0);
			}
		}

		switch(key) {
			case -2:
				gbc->quit = true;
				gbcc_sdl_destroy(sdl);
				return;
			case 0:
				emulator_key = GBCC_KEY_A;
				break;
			case 1:
				emulator_key = GBCC_KEY_B;
				break;
			case 2:
				emulator_key = GBCC_KEY_START;
				break;
			case 3:
				emulator_key = GBCC_KEY_SELECT;
				break;
			case 4:
				emulator_key = GBCC_KEY_UP;
				break;
			case 5:
				emulator_key = GBCC_KEY_DOWN;
				break;
			case 6:
				emulator_key = GBCC_KEY_LEFT;
				break;
			case 7:
				emulator_key = GBCC_KEY_RIGHT;
				break;
			case 8:
				emulator_key = GBCC_KEY_TURBO;
				break;
			case 9:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_RAW_SCREENSHOT;
				} else {
					emulator_key = GBCC_KEY_SCREENSHOT;
				}
				break;
			case 10:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_PRINTER;
				} else {
					emulator_key = GBCC_KEY_PAUSE;
				}
				break;
			case 11:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_FRAME_BLENDING;
				} else {
					emulator_key = GBCC_KEY_FPS;
				}
				break;
			case 12:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_VRAM;
				} else {
					emulator_key = GBCC_KEY_VSYNC;
				}
				break;
			case 13:
				emulator_key = GBCC_KEY_DISPLAY_BACKGROUND;
				break;
			case 14:
				emulator_key = GBCC_KEY_DISPLAY_WINDOW;
				break;
			case 15:
				emulator_key = GBCC_KEY_DISPLAY_SPRITES;
				break;
			case 16:
				emulator_key = GBCC_KEY_LINK_CABLE;
				break;
			case 17:
				emulator_key = GBCC_KEY_AUTOSAVE;
				break;
			case 18:
				emulator_key = GBCC_KEY_BACKGROUND_PLAY;
				break;
			case 19:
				emulator_key = GBCC_KEY_MENU;
				break;
			case 20:
				emulator_key = GBCC_KEY_INTERLACE;
				break;
			case 21:
				emulator_key = GBCC_KEY_SHADER;
				break;
			case 22:
				emulator_key = GBCC_KEY_CHEATS;
				break;
			case 23:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_ACCELEROMETER_MAX_DOWN;
				} else {
					continue;
				}
				break;
			case 24:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_ACCELEROMETER_MAX_LEFT;
				} else {
					continue;
				}
				break;
			case 25:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_ACCELEROMETER_MAX_RIGHT;
				} else {
					continue;
				}
				break;
			case 26:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_ACCELEROMETER_MAX_UP;
				} else {
					continue;
				}
				break;
			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
			case 32:
			case 33:
			case 34:
			case 35:
			case 36:
				if (state[SDL_SCANCODE_LSHIFT]) {
					emulator_key = GBCC_KEY_SAVE_STATE_1 + (uint8_t)(key - 27);
				} else {
					emulator_key = GBCC_KEY_LOAD_STATE_1 + (uint8_t)(key - 27);
				}
				break;
			default:
				continue;
		}
		if (e.type == SDL_CONTROLLERAXISMOTION) {
			if (e.caxis.value == 0) {
				if (emulator_key == GBCC_KEY_DOWN) {
					gbcc_input_process_key(gbc, GBCC_KEY_UP, false);
				} else if (emulator_key == GBCC_KEY_RIGHT) {
					gbcc_input_process_key(gbc, GBCC_KEY_LEFT, false);
				}
			}
		}
		gbcc_input_process_key(gbc, emulator_key, val);
	}

	if (state[SDL_SCANCODE_KP_6]) {
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_RIGHT, true);
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_LEFT, false);
	} else if (state[SDL_SCANCODE_KP_4]) {
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_RIGHT, false);
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_LEFT, true);
	} else {
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_RIGHT, false);
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_LEFT, false);
	}
	if (state[SDL_SCANCODE_KP_8]) {
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_UP, true);
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_DOWN, false);
	} else if (state[SDL_SCANCODE_KP_2]) {
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_UP, false);
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_DOWN, true);
	} else {
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_UP, false);
		gbcc_input_process_key(gbc, GBCC_KEY_ACCELEROMETER_DOWN, false);
	}
	process_game_controller(sdl);
	gbcc_input_accelerometer_update(&gbc->core.cart.mbc.accelerometer);
}

int process_input(struct gbcc_sdl *sdl, const SDL_Event *e)
{
	struct gbcc *gbc = &sdl->gbc;
	if (e->type == SDL_QUIT) {
		gbc->quit = true;
	} else if (e->type == SDL_WINDOWEVENT) {
		if (e->window.event == SDL_WINDOWEVENT_CLOSE) {
			uint32_t id = e->window.windowID;
			if (id == SDL_GetWindowID(sdl->window)) {
				return -2;
			} else if (id == SDL_GetWindowID(sdl->vram_window)) {
				gbc->vram_display = false;
			} else {
				gbcc_log_error("Unknown window ID %u\n", id);
			}
		} else if (e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			gbc->has_focus = true;
			SDL_PumpEvents();
			SDL_FlushEvent(SDL_KEYDOWN);
		} else if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			gbc->has_focus = false;
		}
	} else if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
		for (size_t i = 0; i < N_ELEM(keymap); i++) {
			if (e->key.keysym.scancode == keymap[i]) {
				return (int)i;
			}
		}
	} else if (e->type == SDL_CONTROLLERBUTTONDOWN || e->type == SDL_CONTROLLERBUTTONUP) {
		for (size_t i = 0; i < N_ELEM(buttonmap); i++) {
			if (e->cbutton.button == buttonmap[i]) {
				return (int)i;
			}
		}
	} else if (e->type == SDL_CONTROLLERAXISMOTION) {
		if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
			if (e->caxis.value < 0) {
				return 4;
			} else {
				return 5;
			}
		} else if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
			if (e->caxis.value < 0) {
				return 6;
			} else {
				return 7;
			}
		}
	}
	return -1;
}

void process_game_controller(struct gbcc_sdl *sdl)
{
	if (sdl->game_controller != NULL && !SDL_GameControllerGetAttached(sdl->game_controller)) {
		gbcc_log_info("Controller \"%s\" disconnected.\n", SDL_GameControllerName(sdl->game_controller));
		SDL_GameControllerClose(sdl->game_controller);
		sdl->game_controller = NULL;
	} 
	if (sdl->game_controller == NULL) {
		for (int i = 0; i < SDL_NumJoysticks(); i++) {
			if (SDL_IsGameController(i)) {
				sdl->game_controller = SDL_GameControllerOpen(i);
				gbcc_log_info("Controller \"%s\" connected.\n", SDL_GameControllerName(sdl->game_controller));
				break;
			}
		}
		sdl->haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(sdl->game_controller));
		if (sdl->game_controller == NULL || SDL_NumHaptics() == 0) {
			return;
		}
		if (SDL_HapticRumbleInit(sdl->haptic) != 0) {
			printf("%s\n", SDL_GetError());
			return;
		}
	}

	if (sdl->haptic == NULL) {
		return;
	}
	if (sdl->gbc.core.cart.rumble_state) {
		if (SDL_HapticRumblePlay(sdl->haptic, 0.1f, 100) != 0) {
			printf("%s\n", SDL_GetError());
		}
	} else {
		if (SDL_HapticRumbleStop(sdl->haptic) != 0) {
			printf("%s\n", SDL_GetError());
		}
	}
}

void *init_input(void *_)
{
	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0) {
		gbcc_log_error("Failed to initialize controller support: %s\n", SDL_GetError());
	}
	return NULL;
}

void set_icon(SDL_Window *win, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	uint8_t header[HEADER_BYTES];
	if (!fp) {
		gbcc_log_error("Couldn't open %s: %s\n", filename, strerror(errno));
		return;
	}
	if (fread(header, 1, HEADER_BYTES, fp) != HEADER_BYTES) {
		gbcc_log_error("Failed to read icon data: %s\n", filename);
		fclose(fp);
		return;
	}
	if (png_sig_cmp(header, 0, HEADER_BYTES)) {
		gbcc_log_error("Not a PNG file: %s\n", filename);
		fclose(fp);
		return;
	}

	png_structp png_ptr = png_create_read_struct(
			PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	if (!png_ptr) {
		gbcc_log_error("Couldn't create PNG read struct.\n");
		fclose(fp);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(fp);
		gbcc_log_error("Couldn't create PNG info struct.\n");
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr)) != 0) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		gbcc_log_error("Couldn't setjmp for libpng.\n");
		return;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, HEADER_BYTES);
	png_read_info(png_ptr, info_ptr);

	uint32_t width = png_get_image_width(png_ptr, info_ptr);
	uint32_t height = png_get_image_height(png_ptr, info_ptr);
	png_get_channels(png_ptr, info_ptr);

	uint32_t *bitmap = calloc(width * height, sizeof(*bitmap));

	png_bytepp row_pointers = calloc(height, sizeof(png_bytep));
	for (uint32_t y = 0; y < height; y++) {
		row_pointers[y] = (uint8_t *)&bitmap[y * width];
	}

	if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE) {
		png_set_expand(png_ptr);
	}
	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, NULL);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(row_pointers);
	fclose(fp);

	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
			bitmap,
			(int)width,
			(int)height,
			32,
			(int)width * 4,
			SDL_PIXELFORMAT_RGBA32);

	if (surface == NULL) {
		gbcc_log_error("Failed to create surface: %s\n", SDL_GetError());
		free(bitmap);
		return;
	}

	SDL_SetWindowIcon(win, surface);

	SDL_free(surface);
	free(bitmap);
	return;
}
