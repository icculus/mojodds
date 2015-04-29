#define _GNU_SOURCE

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <GL/glew.h>

#include "mojodds.h"


// example how to use mojodds with OpenGL
// this program doesn't actually draw anything
// use apitrace, vogl or similar tool to see what it does


static void pumpGLErrors(const char *format, ...) {
	va_list args;

	va_start(args, format);

	char *tempStr = NULL;
	vasprintf(&tempStr, format, args);

	va_end(args);

	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		switch (err) {
		case GL_INVALID_ENUM:
			printf("GL error GL_INVALID_ENUM in \"%s\"\n", tempStr);
			break;

		case GL_INVALID_VALUE:
			printf("GL error GL_INVALID_VALUE in \"%s\"\n", tempStr);
			break;

		default:
            printf("Unknown GL error %04x in \"%s\"\n", err, tempStr);
			break;
		}
	}

	free(tempStr);
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
        SDL_Init(SDL_INIT_VIDEO);
		SDL_Window *window = SDL_CreateWindow("OpenGL DDS test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_OPENGL);
		if (window == NULL) {
			printf("Could not create window: %s\n", SDL_GetError());
			free(contents);
			return 3;
		}

		SDL_GLContext context = SDL_GL_CreateContext(window);
		if (context == NULL) {
			printf("Could not create GL context: %s\n", SDL_GetError());
			SDL_DestroyWindow(window);
			free(contents);
			return 4;
		}

		// clear and swap to get better traces
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		SDL_GL_SwapWindow(window);

		glewInit();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		SDL_GL_SwapWindow(window);

		// glewInit might raise errors if we got an OpenGL 3.0 context
		// ignore them
		// in this case glGetError is simpler than fooling around with callbacks
		GLenum err = GL_NO_ERROR;
		while ((err = glGetError()) != GL_NO_ERROR) { }

		const void *tex = NULL;
		unsigned long texlen = 0;
		unsigned int glfmt = 0, w = 0, h = 0, miplevels = 0;
		int retval = MOJODDS_getTexture(contents, size, &tex, &texlen, &glfmt, &w, &h, &miplevels);

		bool isCompressed = true;
		GLenum internalFormat = glfmt;
		if (glfmt == GL_BGRA || glfmt == GL_BGR) {
			isCompressed = false;
			if (glfmt == GL_BGR) {
				internalFormat = GL_RGB8;
			} else {
				internalFormat = GL_RGBA8;
			}
		}

		GLuint texId = 0;
		// we leak this but don't care
		glGenTextures(1, &texId);
		glBindTexture(GL_TEXTURE_2D, texId);

		for (unsigned int miplevel = 0; miplevel < miplevels; miplevel++) {
			const void *miptex = NULL;
			unsigned long miptexlen = 0;
			unsigned int mipW = 0, mipH = 0;
			retval = MOJODDS_getMipMapTexture(miplevel, glfmt, tex, texlen, w, h, &miptex, &miptexlen, &mipW, &mipH);
			if (!retval) {
				printf("MOJODDS_getMipMapTexture(%u) error: %d\n", miplevel, retval);
				continue;
			}

			if (isCompressed) {
				glCompressedTexImage2D(GL_TEXTURE_2D, miplevel, glfmt, mipW, mipH, 0, miptexlen, miptex);
				pumpGLErrors("glCompressedTexImage2D %u 0x%04x %ux%u %u", miplevel, glfmt, mipW, mipH, miptexlen);
			} else {
				glTexImage2D(GL_TEXTURE_2D, miplevel, internalFormat, mipW, mipH, 0, glfmt, GL_UNSIGNED_BYTE, miptex);
				pumpGLErrors("glTexImage2D %u 0x%04x %ux%u 0x%04x", miplevel, internalFormat, mipW, mipH, glfmt);
			}
		}

		// and now the same with ARB_texture_storage if it's available
		if (GLEW_ARB_texture_storage) {
			glGenTextures(1, &texId);
			glBindTexture(GL_TEXTURE_2D, texId);
			glTexStorage2D(GL_TEXTURE_2D, miplevels, internalFormat, w, h);
			pumpGLErrors("glTexStorage2D %u 0x%04x %ux%u", miplevels, internalFormat, w, h);

			for (unsigned int miplevel = 0; miplevel < miplevels; miplevel++) {
				const void *miptex = NULL;
				unsigned long miptexlen = 0;
				unsigned int mipW = 0, mipH = 0;
				retval = MOJODDS_getMipMapTexture(miplevel, glfmt, tex, texlen, w, h, &miptex, &miptexlen, &mipW, &mipH);
				if (!retval) {
					printf("MOJODDS_getMipMapTexture(%u) error: %d\n", miplevel, retval);
					continue;
				}

				if (isCompressed) {
					glCompressedTexSubImage2D(GL_TEXTURE_2D, miplevel, 0, 0, mipW, mipH, glfmt, miptexlen, miptex);
					pumpGLErrors("glCompressedTexSubImage2D %u %ux%u 0x%04x %u", miplevel, mipW, mipH, glfmt, miptexlen);
				} else {
					glTexSubImage2D(GL_TEXTURE_2D, miplevel, 0, 0, mipW, mipH, glfmt, GL_UNSIGNED_BYTE, miptex);
					pumpGLErrors("glTexSubImage2D %u %ux%u 0x%04x", miplevel, mipW, mipH, glfmt);
				}
			}
		}

		// one last clear and swap for clean trace end
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		SDL_GL_SwapWindow(window);

		SDL_GL_DeleteContext(context);
		SDL_DestroyWindow(window);
	}

	free(contents);

	return 0;
}
