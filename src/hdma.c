/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "debug.h"
#include "core.h"
#include "hdma.h"
#include "memory.h"

void gbcc_hdma_copy_chunk(struct gbcc_core *gbc)
{
	if (gbc->hdma.to_copy <= 0) {
		gbcc_log_error("HDMA already finished.\n");
		return;
	}
	/* In single speed mode, hdma copies twice as much per clock */
	for (int i = 0; i < 4 * (1 + !gbc->cpu.double_speed) && gbc->hdma.to_copy > 0; i++) {
		gbcc_memory_copy(gbc, gbc->hdma.source, gbc->hdma.dest, true);
		gbc->hdma.source++;
		gbc->hdma.dest++;
		gbc->hdma.length--;
		gbc->hdma.to_copy--;
	}
	gbcc_memory_write(gbc, HDMA1, (gbc->hdma.source >> 8u), true);
	gbcc_memory_write(gbc, HDMA2, (gbc->hdma.source & 0xFFu), true);
	gbcc_memory_write(gbc, HDMA3, (gbc->hdma.dest >> 8u), true);
	gbcc_memory_write(gbc, HDMA4, (gbc->hdma.dest & 0xFFu), true);
	if (gbc->hdma.length == 0) {
		gbcc_memory_write(gbc, HDMA5, 0xFFu, true);
	} else {
		gbcc_memory_write(gbc, HDMA5, (uint8_t)((gbc->hdma.length >> 4u) - 1), true);
	}
}
