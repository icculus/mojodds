#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mojodds.h"


// from http://graphics.stanford.edu/~seander/bithacks.html
static bool isPow2(unsigned int v) {
	return v && !(v & (v - 1));
}


int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s DDS-file\n", argv[0]);
		return 0;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		printf("Error opening %s: %s (%d)\n", argv[1], strerror(errno), errno);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *contents = malloc(size);
	size_t readbytes = fread(contents, 1, size, f);
	if (readbytes != size) {
		printf("Only got %u of %ld bytes: %s\n", (unsigned int) readbytes, size, strerror(errno));
		free(contents);
		fclose(f);
		return 2;
	}

	fclose(f);

	int isDDS = MOJODDS_isDDS(contents, size);
	printf("isDDS: %d\n", isDDS);
	if (isDDS) {
		const void *tex = NULL;
		unsigned long texlen = 0;
		unsigned int glfmt = 0, w = 0, h = 0, miplevels = 0;
		unsigned int cubemapfacelen = 0;
		MOJODDS_textureType textureType = MOJODDS_TEXTURE_NONE;
		int retval = MOJODDS_getTexture(contents, size, &tex, &texlen, &glfmt, &w, &h, &miplevels, &cubemapfacelen, &textureType);

		uintptr_t texoffset = ((const char *)(tex)) - contents;
		printf("MOJODDS_getTexture retval: %d\n", retval);
		printf("texoffset: %u\n", (unsigned int)(texoffset));
		printf("texlen: %lu\n", texlen);
		printf("glfmt: 0x%x\n", glfmt);
		printf("width x height: %d x %d\n", w, h);
		printf("miplevels: %d\n", miplevels);
		printf("\n");

		for (unsigned int miplevel = 0; miplevel < miplevels; miplevel++) {
			const void *miptex = NULL;
			unsigned long miptexlen = 0;
			unsigned int mipW = 0, mipH = 0;
			retval = MOJODDS_getMipMapTexture(miplevel, glfmt, tex, texlen, w, h, &miptex, &miptexlen, &mipW, &mipH);
			if (!retval) {
				printf("MOJODDS_getMipMapTexture(%u) error: %d\n", miplevel, retval);
				continue;
			}

			uintptr_t miptexoffset = ((const char *)(miptex)) - ((const char *)(tex));
			bool npot = !(isPow2(mipW) || isPow2(mipH));
			printf("%4d x %4d  %s", mipW, mipH, npot ? "NPOT  " : "      ");
			printf("miptexoffset: %8u  ", (unsigned int)(miptexoffset));
			printf("miptexlen: %8lu\n", miptexlen);
		}
	}

	free(contents);

	return 0;
}
