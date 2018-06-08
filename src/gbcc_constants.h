#ifndef GBC_CONSTANTS_H
#define GBC_CONSTANTS_H

/* Memory map */
#define ROM0_START 0x0000u	/* Non-Switchable ROM Bank */
#define ROM0_SIZE 0x4000u
#define ROMX_START 0x4000u	/* Switchable ROM Bank */
#define ROMX_SIZE 0x4000u
#define VRAM_START 0x8000u	/* VRAM (Switchable in GBC mode) */
#define VRAM_SIZE 0x2000u
#define SRAM_START 0xA000u	/* Cartridge RAM */
#define SRAM_SIZE 0x2000u
#define WRAM0_START 0xC000u	/* Non-Switchable Work RAM */
#define WRAM0_SIZE 0x1000u
#define WRAMX_START 0xD000u	/* Work RAM (Switchable in GBC mode) */
#define WRAMX_SIZE 0x1000u
#define ECHO_START 0xE000u	/* Mirror of WRAM */
#define ECHO_SIZE 0x1E00u
#define OAM_START 0xFE00u	/* Object Attribute Table */
#define OAM_SIZE 0x00A0u
#define UNUSED_START 0xFEA0u	/* Strange Unused Area */
#define UNUSED_SIZE 0x0060u
#define IOREG_START 0xFF00u	/* I/O Registers */
#define IOREG_SIZE 0x0080u
#define HRAM_START 0xFF80u	/* Internal CPU RAM */
#define HRAM_SIZE 0x007Fu

/* Cartridge header map */
#define CART_HEADER_START 0x0100u
#define CART_HEADER_SIZE 0x004Fu
#define CART_LOGO_START 0x0104u
#define CART_LOGO_SIZE 0x0030u
#define CART_GBC_FLAG 0x0143u
#define CART_TYPE 0x0147u
#define CART_ROM_SIZE 0x0148u
#define CART_RAM_SIZE 0x0149u
#define CART_HEADER_CHECKSUM 0x014Du

/* Registers */
/* Joypad */
#define JOYP 0xFF00u	/* Joypad (0-3 R, 4-7 R/W) */

/* Link Cable */
#define SB 0xFF01u	/* Serial Transfer Data (R/W) */
#define SC 0xFF02u	/* Serial Transfer Control (R/W) */

/* Timers */
#define DIV 0xFF04u	/* Divider Register (R/W) */
#define TIMA 0xFF05u	/* Timer Counter (R/W) */
#define TMA 0xFF06u	/* Timer Modulo (R/W) */
#define TAC 0xFF07u	/* Timer Control (R/W) */

/* LCD */
#define LCDC 0xFF40u	/* LCD Control (R/W) */
#define STAT 0xFF41u	/* LCDC Status (0-2 R, 3+ R/W) */
#define SCY 0xFF42u	/* Scroll Y (R/W) */
#define SCX 0xFF43u	/* Scroll X (R/W) */
#define LY 0xFF44u	/* LCDC Y-Coordinate (R) */
#define LYC 0xFF45u	/* LY Compare (R) */
#define WY 0xFF4Au	/* Window Y Position (R/W) */
#define WX 0xFF4Bu	/* Window X Position minus 7 (R/W) */
#define BGP 0xFF47u	/* BG Palette data (DMG mode) (R/W) */
#define OBP0 0xFF48u	/* Object Palette 0 data (DMG mode) (R/W) */
#define OBP1 0xFF49u	/* Object Palette 1 data (DMG mode) (R/W) */
#define BGPI 0xFF68u	/* Background Palette Index (GBC mode) (R/W) */
#define BGPD 0xFF69u	/* Background Palette Data (GBC mode) (R/W) */
#define OBPI 0xFF6Au	/* Sprite Palette Index (GBC mode) (R/W) */
#define OBPD 0xFF6Bu	/* Sprite Palette Data (GBC mode) (R/W) */
#define VBK 0xFF4Fu	/* VRAM Bank (GBC mode) (R/W) */
#define DMA 0xFF46u	/* DMA Transfer and Start Address (W) */
#define HDMA1 0xFF51u	/* New DMA Source, High (GBC mode) (R/W) */
#define HDMA2 0xFF52u	/* New DMA Source, Low (GBC mode) (R/W) */
#define HDMA3 0xFF53u	/* New DMA Destination, High (GBC mode) (R/W) */
#define HDMA4 0xFF54u	/* New DMA Destination, Low (GBC mode) (R/W) */
#define HDMA5 0xFF55u	/* New DMA Length/Mode/Start (GBC mode) (R/W) */

/* Sound */
#define NR10 0xFF10u	/* Channel 1 Sweep (R/W) */
#define NR11 0xFF11u	/* Channel 1 Sound Length / Wave Pattern Duty (R/W) */
#define NR12 0xFF12u	/* Channel 1 Volume Envelope (R/W) */
#define NR13 0xFF13u	/* Channel 1 Frequency Low (W) */
#define NR14 0xFF14u	/* Channel 1 Frequency High (0-2 W, 6 R/W, 7 W) */
#define NR21 0xFF16u	/* Channel 2 Sound Length / Wave Pattern Duty (R/W) */
#define NR22 0xFF17u	/* Channel 2 Volume Envelope (R/W) */
#define NR23 0xFF18u	/* Channel 2 Frequency Low (W) */
#define NR24 0xFF19u	/* Channel 2 Frequency High (0-2 W, 6 R/W, 7 W) */
#define NR30 0xFF1Au	/* Channel 3 Sound On/Off (R/W) */
#define NR31 0xFF1Bu	/* Channel 3 Sound Length (R/W) */
#define NR32 0xFF1Cu	/* Channel 3 Select Output Level (R/W) */
#define NR33 0xFF1Du	/* Channel 3 Frequency Low (W) */
#define NR34 0xFF1Eu	/* Channel 3 Frequency High (0-2 W, 6 R/W, 7 W) */
#define NR41 0xFF20u	/* Channel 4 Sound Length (R/W) */
#define NR42 0xFF21u	/* Channel 4 Volume Envelope (R/W) */
#define NR43 0xFF22u	/* Channel 4 Polynomial Counter (R/W) */
#define NR44 0xFF23u	/* Channel 4 Counter/Consecutive; Initial (6 R/W, 7 W) */
#define NR50 0xFF24u	/* Channel Control / On/Off / Volume (R/W) */
#define NR51 0xFF25u	/* Selection of Sound Output Terminal (R/W) */
#define NR52 0xFF26u	/* Sound On / Off (0-3 R, 7 R/W) */

/* Interrupts */
#define IE 0xFFFFu	/* Interrupt Enable (0-4 R/W, 5-7 R) */
#define IF 0xFF0Fu	/* Interrupt Flag (0-4 R/W, 5-7 R) */

/* GBC Miscellaneous */
#define KEY1 0xFF4Du	/* Prepare Speed Switch (GBC mode) (0 R/W, 7 R) */
#define RP 0xFF56u	/* IR Port (GBC mode) (0 R/W, 1 R, 6-7 R) */
#define SVBK 0xFF70u	/* WRAM Bank (GBC mode) (0-2 R/W) */

/* Interrupt Vectors */
#define INT_VBLANK 0x0040u	/* V-Blank */
#define INT_LCDSTAT 0x0048u	/* LCD Status */
#define INT_TIMER 0x0050u	/* TIMA Overflow */
#define INT_SERIAL 0x0058u	/* Byte Transferred */
#define INT_JOYPAD 0x0060u	/* Button Press */

/* System Information */
#define GBC_CLOCK_FREQ 4194304	/* Clock speed in Hz */
#define GBC_CLOCK_PERIOD 238	/* Clock period in ns */
#define GBC_SCREEN_WIDTH 160	/* Screen width in pixels */
#define GBC_SCREEN_HEIGHT 144	/* Screen height in pixels */
#define GBC_SCREEN_SIZE (GBC_SCREEN_WIDTH * GBC_SCREEN_HEIGHT)	/* Total number of pixels */


enum MBC { NONE, MBC1, MBC2, MBC3, MBC4, MBC5, MMM01 };
enum CART_MODE { GBC, DMG };

#endif /* GBC_CONSTANTS_H */
