#ifndef GBCC_AUDIO_H
#define GBCC_AUDIO_H

#include "gbcc.h"
#include <SDL2/SDL.h>

#define GBCC_AUDIO_BUFSIZE 512 /* samples */
#define GBCC_AUDIO_FMT Uint16

struct gbcc_audio {
	struct gbc *gbc;
	SDL_Thread *thread;
	SDL_AudioSpec audiospec;
	SDL_AudioDeviceID device;
	uint64_t sample_clock;
	uint64_t sample;
	size_t index;
	bool quit;
	struct timespec cur_time;
	struct timespec start_time;
	GBCC_AUDIO_FMT mix_buffer[GBCC_AUDIO_BUFSIZE];
};

struct gbcc_audio *gbcc_audio_initialise(struct gbc *gbc);
void gbcc_audio_update(struct gbcc_audio *audio);

#endif /* GBCC_AUDIO_H */