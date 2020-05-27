/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_H
#define GBCC_H

#ifdef __ANDROID__
#define ANDROID_INLINE __attribute__((always_inline))
#else
#define ANDROID_INLINE
#endif

#include "audio.h"
#include "core.h"
#include "camera.h"
#include "menu.h"
#include "window.h"
#include "vram_window.h"

struct gbcc {
	struct gbcc_core core;
	struct gbcc_window window;
	struct gbcc_vram_window vram_window;
	struct gbcc_audio audio;
	struct gbcc_menu menu;
	struct gbcc_camera_platform camera;
	
	const char *save_directory;
	float turbo_speed;
	bool quit;
	bool pause;
	int8_t save_state;
	int8_t load_state;
	bool background_play;
	bool has_focus;
	bool autosave;
	bool fractional_scaling;
	bool frame_blending;
	bool interlacing;
	bool vram_display;
	bool show_fps;
};

void *gbcc_emulation_loop(void *_gbc);

#endif /* GBCC_H */
