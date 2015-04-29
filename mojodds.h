#ifndef _INCL_MOJODDS_H_
#define _INCL_MOJODDS_H_

/* !!! FIXME: flesh this file out. */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum MOJODDS_textureType
{
    MOJODDS_TEXTURE_NONE,
    MOJODDS_TEXTURE_2D,
    MOJODDS_TEXTURE_CUBE,
    MOJODDS_TEXTURE_VOLUME
} MOJODDS_textureType;

int MOJODDS_isDDS(const void *_ptr, const unsigned long _len);
int MOJODDS_getTexture(const void *_ptr, const unsigned long _len,
                       const void **_tex, unsigned long *_texlen,
                       unsigned int *_glfmt, unsigned int *_w,
                       unsigned int *_h, unsigned int *_miplevels,
                       unsigned int *_cubemapfacelen,
                       MOJODDS_textureType *_textureType);

#ifdef __cplusplus
}
#endif

#endif

/* end of mojodds.h ... */

