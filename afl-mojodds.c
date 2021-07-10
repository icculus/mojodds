#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mojodds.h"


// helper for fuzzing with AFL
// if you don't know what this is you don't need it


void __afl_manual_init(void);


int main(int argc, char *argv[]) {
	if (argc != 2) {
		return 0;
	}

#ifdef __AFL_HAVE_MANUAL_INIT
	__afl_manual_init();
#endif  // __AFL_HAVE_MANUAL_INIT

	const char *filename = argv[1];

	FILE *f = fopen(filename, "rb");
	if (!f) {
		return 1;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *contents = malloc(size);
	size_t readbytes = fread(contents, 1, size, f);
	if (readbytes != size) {
		free(contents);
		fclose(f);
		return 2;
	}

	fclose(f);

	int isDDS = MOJODDS_isDDS(contents, size);
	if (!isDDS) {
		return 3;
	}

	const void *tex = NULL;
	unsigned long texlen = 0;
	unsigned int glfmt = 0, w = 0, h = 0, miplevels = 0;
	unsigned int cubemapfacelen = 0;
	MOJODDS_textureType textureType = 0;
	int retval = MOJODDS_getTexture(contents, size, &tex, &texlen, &glfmt, &w, &h, &miplevels, &cubemapfacelen, &textureType);
	if (!retval) {
		free(contents);
		return 4;
	}

	uint32_t hash = 0x12345678;
	switch (textureType) {
	case MOJODDS_TEXTURE_2D:
	for (unsigned int miplevel = 0; miplevel < miplevels; miplevel++) {
		const void *miptex = NULL;
		unsigned long miptexlen = 0;
		unsigned int mipW = 0, mipH = 0;
		retval = MOJODDS_getMipMapTexture(miplevel, glfmt, tex, w, h, &miptex, &miptexlen, &mipW, &mipH);
		if (!retval) {
			continue;
		}

		// read every byte to make sure any buffer overflows actually overflow
		const char *miptex_ = (const char *) miptex;
		for (unsigned int i = 0; i < miptexlen; i++) {
			hash = (hash * 65537) ^ miptex_[i];
		}
	}
	break;

	case MOJODDS_TEXTURE_CUBE:
		for (MOJODDS_cubeFace cubeFace = MOJODDS_CUBEFACE_POSITIVE_X; cubeFace <= MOJODDS_CUBEFACE_NEGATIVE_Z; cubeFace++) {
			for (unsigned int miplevel = 0; miplevel < miplevels; miplevel++) {
				const void *miptex = NULL;
				unsigned long miptexlen = 0;
				unsigned int mipW = 0, mipH = 0;
				retval = MOJODDS_getCubeFace(cubeFace, miplevel, glfmt, tex, cubemapfacelen, w, h, &miptex, &miptexlen, &mipW, &mipH);
				if (!retval) {
					continue;
				}

				// read every byte to make sure any buffer overflows actually overflow
				const char *miptex_ = (const char *) miptex;
				for (unsigned int i = 0; i < miptexlen; i++) {
					hash = (hash * 65537) ^ miptex_[i];
				}
			}
		}
		break;

	case MOJODDS_TEXTURE_VOLUME:
		// TODO: do something with the data
		break;

	}
	// do something the optimizer is not allowed to remove
	printf("0x%08x\n", hash);

	free(contents);

	return 0;
}
