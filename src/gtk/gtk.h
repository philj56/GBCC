/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_GTK_H
#define GBCC_GTK_H

#include "../gbcc.h"
#include "../palettes.h"
#include <gtk/gtk.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <time.h>

struct gbcc_gtk {
	struct gbcc gbc;
	bool fullscreen;
	pthread_t emulation_thread;
	struct timespec last_cursor_move;
	GdkCursor* blank_cursor;
	GdkCursor* default_cursor;
	SDL_GameController *game_controller;
	SDL_Haptic *haptic;
	GtkWindow *window;
	GtkDialog *turbo_dialog;
	GtkWidget *gl_area;
	GtkWidget *vram_gl_area;
	GList *icons;
	guint keymap[51];
	struct {
		GtkWidget *bar;
		GtkWidget *stop;
		GtkWidget *autosave;
		GtkWidget *background_playback;
		GtkWidget *fractional_scaling;
		GtkWidget *frame_blending;
		GtkWidget *vram_display;
		GtkWidget *turbo_speed;
		GtkWidget *vsync;
		GtkWidget *interlacing;
		GtkCheckMenuItem *turbo_custom;
		GtkSpinButton *custom_turbo_speed;
		struct {
			GtkWidget *submenu;
			GtkWidget *state[9];
		} save_state;
		struct {
			GtkWidget *submenu;
			GtkWidget *state[9];
		} load_state;
		struct {
			GtkWidget *menuitem;
			GtkWidget *submenu;
			GSList *group;
			GtkWidget *shader[4];
		} shader;
		struct {
			GtkWidget *menuitem;
			GtkWidget *submenu;
			GSList *group;
			GtkWidget *palette[GBCC_NUM_PALETTES];
		} palette;
	} menu;
	char *filename;
};

void gbcc_gtk_initialise(struct gbcc_gtk *gtk, int *argc, char ***argv);

#endif /* GBCC_SDL_H */
