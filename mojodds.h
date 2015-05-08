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


/* order and values for these matter, they are used for calculating offsets
   lucky for us both DDS and OpengGL order matches */
typedef enum MOJODDS_cubeFace {
    MOJODDS_CUBEFACE_POSITIVE_X,
    MOJODDS_CUBEFACE_NEGATIVE_X,
    MOJODDS_CUBEFACE_POSITIVE_Y,
    MOJODDS_CUBEFACE_NEGATIVE_Y,
    MOJODDS_CUBEFACE_POSITIVE_Z,
    MOJODDS_CUBEFACE_NEGATIVE_Z
} MOJODDS_cubeFace;


int MOJODDS_isDDS(const void *_ptr, const unsigned long _len);
int MOJODDS_getTexture(const void *_ptr, const unsigned long _len,
                       const void **_tex, unsigned long *_texlen,
                       unsigned int *_glfmt, unsigned int *_w,
                       unsigned int *_h, unsigned int *_miplevels,
                       unsigned int *_cubemapfacelen,
                       MOJODDS_textureType *_textureType);
int MOJODDS_getMipMapTexture(unsigned int miplevel, unsigned int glfmt,
                             const void*_basetex, const unsigned long _basetexlen,
                             unsigned int w, unsigned h,
                             const void **_tex, unsigned long *_texlen,
                             unsigned int *_texw, unsigned int *_texh);

int MOJODDS_getCubeFace(MOJODDS_cubeFace cubeFace, unsigned int miplevel,
                        unsigned int glfmt, const void*_basetex,
                        unsigned long _basetexlen, unsigned int w, unsigned h,
                        const void **_tex, unsigned long *_texlen,
                        unsigned int *_texw, unsigned int *_texh);


#ifdef __cplusplus
}
#endif

#endif

/* end of mojodds.h ... */

