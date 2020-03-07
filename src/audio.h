/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_AUDIO_H
#define GBCC_AUDIO_H

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <stdint.h>
#include <time.h>

#define GBCC_AUDIO_BUFSIZE_SAMPLES 4096
#define GBCC_AUDIO_BUFSIZE (GBCC_AUDIO_BUFSIZE_SAMPLES*2) /* samples * channels */
#define GBCC_AUDIO_FMT uint16_t

struct gbcc;

struct gbcc_audio {
	struct {
		ALCdevice *device;
		ALCcontext *context;
		ALuint source;
		ALuint buffers[5];
		ALuint vsync_buffers[5];
	} al;
	uint64_t sample_clock;
	uint64_t clock;
	size_t index;
	struct timespec cur_time;
	struct timespec start_time;
	GBCC_AUDIO_FMT mix_buffer[GBCC_AUDIO_BUFSIZE];
	struct timespec t_buffer[GBCC_AUDIO_BUFSIZE_SAMPLES];
};

void gbcc_audio_initialise(struct gbcc *gbc);
void gbcc_audio_destroy(struct gbcc *gbc);
void gbcc_audio_update(struct gbcc *gbc);
int gbcc_check_openal_error(const char *msg);

#endif /* GBCC_AUDIO_H */
