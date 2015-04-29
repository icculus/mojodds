#ifndef _INCL_MOJODDS_H_
#define _INCL_MOJODDS_H_

/* !!! FIXME: flesh this file out. */
#ifdef __cplusplus
extern "C" {
#endif

int MOJODDS_isDDS(const void *_ptr, const unsigned long _len);
int MOJODDS_getTexture(const void *_ptr, const unsigned long _len,
                       const void **_tex, unsigned long *_texlen,
                       unsigned int *_glfmt, unsigned int *_w,
                       unsigned int *_h, unsigned int *_miplevels);
int MOJODDS_getMipMapTexture(unsigned int miplevel, unsigned int glfmt,
                             const void*_basetex, const unsigned long _basetexlen,
                             unsigned int w, unsigned h,
                             const void **_tex, unsigned long *_texlen,
                             unsigned int *_texw, unsigned int *_texh);

#ifdef __cplusplus
}
#endif

#endif

/* end of mojodds.h ... */

