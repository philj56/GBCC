#include <stdlib.h>
#include <stdio.h>
#include "gbc.h"

int main(int argc, char **argv)
{
	if (argc == 1) {
		printf("Usage: %s rom_file\n", argv[0]);
		exit(1);
	}
	struct gbc gbc;

	gbcc_initialise(&gbc, argv[1]);

	return 0;
}
