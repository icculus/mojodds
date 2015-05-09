/**
 * MojoDDS; tools for dealing with DDS files.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

// Specs on DDS format: http://msdn.microsoft.com/en-us/library/bb943991.aspx/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifdef _MSC_VER
typedef unsigned __int8 uint8;
typedef unsigned __int32 uint32;
#else
#include <stdint.h>
typedef uint8_t uint8;
typedef uint32_t uint32;
#endif

#include "mojodds.h"

#define STATICARRAYLEN(x) ( (sizeof ((x))) / (sizeof ((x)[0])) )

#define DDS_MAGIC 0x20534444  // 'DDS ' in littleendian.
#define DDS_HEADERSIZE 124
#define DDS_PIXFMTSIZE 32
#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PITCH 0x8
#define DDSD_FMT 0x1000
#define DDSD_MIPMAPCOUNT 0x20000
#define DDSD_LINEARSIZE 0x80000
#define DDSD_DEPTH 0x800000
#define DDSD_REQ (DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_FMT)
#define DDSCAPS_ALPHA 0x2
#define DDSCAPS_COMPLEX 0x8
#define DDSCAPS_MIPMAP 0x400000
#define DDSCAPS_TEXTURE 0x1000
#define DDSCAPS2_CUBEMAP 0x200
#define DDSCAPS2_CUBEMAP_POSITIVEX 0x400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x1000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x2000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x4000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x8000
#define DDSCAPS2_VOLUME 0x200000
#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000

#define FOURCC_DXT1 0x31545844
#define FOURCC_DXT2 0x32545844
#define FOURCC_DXT3 0x33545844
#define FOURCC_DXT4 0x34545844
#define FOURCC_DXT5 0x35545844
#define FOURCC_DX10 0x30315844

#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1
#define GL_LUMINANCE_ALPHA 0x190A

#define MAX( a, b ) ((a) > (b) ? (a) : (b))

typedef struct
{
    uint32 dwSize;
    uint32 dwFlags;
    uint32 dwFourCC;
    uint32 dwRGBBitCount;
    uint32 dwRBitMask;
    uint32 dwGBitMask;
    uint32 dwBBitMask;
    uint32 dwABitMask;
} MOJODDS_PixelFormat;

typedef struct
{
    uint32 dwSize;
    uint32 dwFlags;
    uint32 dwHeight;
    uint32 dwWidth;
    uint32 dwPitchOrLinearSize;
    uint32 dwDepth;
    uint32 dwMipMapCount;
    uint32 dwReserved1[11];
    MOJODDS_PixelFormat ddspf;
    uint32 dwCaps;
    uint32 dwCaps2;
    uint32 dwCaps3;
    uint32 dwCaps4;
    uint32 dwReserved2;
} MOJODDS_Header;


//http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
static const uint32_t MultiplyDeBruijnBitPosition[32] =
{
  0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
  8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
};


static uint32_t uintLog2(uint32_t v) {
    v |= v >> 1; // first round down to one less than a power of 2
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;

    return MultiplyDeBruijnBitPosition[(uint32_t)(v * 0x07C4ACDDU) >> 27];
}


static uint32 readui32(const uint8 **_ptr, size_t *_len)
{
    uint32 retval = 0;
    if (*_len < sizeof (retval))
        *_len = 0;
    else
    {
        const uint8 *ptr = *_ptr;
        retval = (((uint32) ptr[0]) <<  0) | (((uint32) ptr[1]) <<  8) |
                 (((uint32) ptr[2]) << 16) | (((uint32) ptr[3]) << 24) ;
        *_ptr += sizeof (retval);
        *_len -= sizeof (retval);
    } // else
    return retval;
} // readui32

static int parse_dds(MOJODDS_Header *header, const uint8 **ptr, size_t *len,
                     unsigned int *_glfmt, unsigned int *_miplevels,
                     unsigned int *_cubemapfacelen,
                     MOJODDS_textureType *_textureType)
{
    const uint32 pitchAndLinear = (DDSD_PITCH | DDSD_LINEARSIZE);
    uint32 width = 0;
    uint32 height = 0;
    uint32 calcSize = 0;
    uint32 calcSizeFlag = DDSD_LINEARSIZE;
    uint32 blockDim = 1;
    uint32 blockSize = 0;
    int i;

    // Files start with magic value...
    if (readui32(ptr, len) != DDS_MAGIC)
        return 0;  // not a DDS file.

    // Then comes the DDS header...
    if (*len < DDS_HEADERSIZE)
        return 0;

    header->dwSize = readui32(ptr, len);
    header->dwFlags = readui32(ptr, len);
    header->dwHeight = readui32(ptr, len);
    header->dwWidth = readui32(ptr, len);
    header->dwPitchOrLinearSize = readui32(ptr, len);
    header->dwDepth = readui32(ptr, len);
    header->dwMipMapCount = readui32(ptr, len);
    for (i = 0; i < STATICARRAYLEN(header->dwReserved1); i++)
        header->dwReserved1[i] = readui32(ptr, len);
    header->ddspf.dwSize = readui32(ptr, len);
    header->ddspf.dwFlags = readui32(ptr, len);
    header->ddspf.dwFourCC = readui32(ptr, len);
    header->ddspf.dwRGBBitCount = readui32(ptr, len);
    header->ddspf.dwRBitMask = readui32(ptr, len);
    header->ddspf.dwGBitMask = readui32(ptr, len);
    header->ddspf.dwBBitMask = readui32(ptr, len);
    header->ddspf.dwABitMask = readui32(ptr, len);
    header->dwCaps = readui32(ptr, len);
    header->dwCaps2 = readui32(ptr, len);
    header->dwCaps3 = readui32(ptr, len);
    header->dwCaps4 = readui32(ptr, len);
    header->dwReserved2 = readui32(ptr, len);

    width = header->dwWidth;
    height = header->dwHeight;

    if (width == 0 || height == 0)
    {
        return 0;
    }

    // check for overflow in width * height
    if (height > 0xFFFFFFFFU / width)
    {
        return 0;
    }

    header->dwCaps &= ~DDSCAPS_ALPHA;  // we'll get this from the pixel format.

    if (header->dwSize != DDS_HEADERSIZE)   // header size must be 124.
        return 0;
    else if (header->ddspf.dwSize != DDS_PIXFMTSIZE)   // size must be 32.
        return 0;
    else if ((header->dwFlags & DDSD_REQ) != DDSD_REQ)  // must have these bits.
        return 0;
    else if ((header->dwCaps & DDSCAPS_TEXTURE) == 0)
        return 0;
    else if ((header->dwFlags & pitchAndLinear) == pitchAndLinear)
        return 0;  // can't specify both.

    *_miplevels = (header->dwCaps & DDSCAPS_MIPMAP) ? header->dwMipMapCount : 1;

    unsigned int calculatedMipLevels = uintLog2(MAX(width, height)) + 1;
    if (*_miplevels == 0)
    {
        // invalid, calculate it ourselves from size
        *_miplevels = calculatedMipLevels;
    }
    else if (*_miplevels > calculatedMipLevels)
    {
        // too many mip levels, several would be 1x1
        // file is corrupted
        return 0;
    }

    if (header->ddspf.dwFlags & DDPF_FOURCC)
    {
        switch (header->ddspf.dwFourCC)
        {
            case FOURCC_DXT1:
                *_glfmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                calcSize = ((width ? ((width + 3) / 4) : 1) * 8) *
                           (height ? ((height + 3) / 4) : 1);
                blockDim = 4;
                blockSize = 8;
                break;
            case FOURCC_DXT3:
                *_glfmt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                calcSize = ((width ? ((width + 3) / 4) : 1) * 16) *
                           (height ? ((height + 3) / 4) : 1);
                blockDim = 4;
                blockSize = 16;
                break;
            case FOURCC_DXT5:
                *_glfmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                calcSize = ((width ? ((width + 3) / 4) : 1) * 16) *
                           (height ? ((height + 3) / 4) : 1);
                blockDim = 4;
                blockSize = 16;
                break;

            // !!! FIXME: DX10 is an extended header, introduced by DirectX 10.
            //case FOURCC_DX10: do_something(); break;

            //case FOURCC_DXT2:  // premultiplied alpha unsupported.
            //case FOURCC_DXT4:  // premultiplied alpha unsupported.
            default:
                return 0;  // unsupported data format.
        } // switch
    } // if

    // no FourCC...uncompressed data.
    else if (header->ddspf.dwFlags & DDPF_RGB)
    {
        if ( (header->ddspf.dwRBitMask != 0x00FF0000) ||
             (header->ddspf.dwGBitMask != 0x0000FF00) ||
             (header->ddspf.dwBBitMask != 0x000000FF) )
            return 0;  // !!! FIXME: deal with this.

        if (header->ddspf.dwFlags & DDPF_ALPHAPIXELS)
        {
            if ( (header->ddspf.dwRGBBitCount != 32) ||
                 (header->ddspf.dwABitMask != 0xFF000000) )
                return 0;  // unsupported.
            *_glfmt = GL_BGRA;
            blockSize = 4;
        } // if
        else
        {
            if (header->ddspf.dwRGBBitCount != 24)
                return 0;  // unsupported.
            *_glfmt = GL_BGR;
            blockSize = 3;
        } // else

        calcSizeFlag = DDSD_PITCH;
        calcSize = ((width * header->ddspf.dwRGBBitCount) + 7) / 8;
    }
    else if (header->ddspf.dwFlags & (DDPF_LUMINANCE | DDPF_ALPHA) )
    {
        *_glfmt = GL_LUMINANCE_ALPHA;

        calcSizeFlag = DDSD_PITCH;
        blockSize = 2;
        calcSize = ((width * header->ddspf.dwRGBBitCount) + 7) / 8;
    }

    //else if (header->ddspf.dwFlags & DDPF_LUMINANCE)  // !!! FIXME
    //else if (header->ddspf.dwFlags & DDPF_YUV)  // !!! FIXME
    //else if (header->ddspf.dwFlags & DDPF_ALPHA)  // !!! FIXME
    else
    {
        return 0;  // unsupported data format.
    } // else if

    // no pitch or linear size? Calculate it.
    if ((header->dwFlags & pitchAndLinear) == 0)
    {
        if (!calcSizeFlag)
        {
            assert(0 && "should have caught this up above");
            return 0;  // uh oh.
        } // if

        header->dwPitchOrLinearSize = calcSize;
        header->dwFlags |= calcSizeFlag;
    } // if

    *_textureType = MOJODDS_TEXTURE_2D;
    { // figure out texture type.
        if (header->dwCaps & DDSCAPS_COMPLEX &&
            header->dwCaps2 & DDSCAPS2_CUBEMAP &&
            header->dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX &&
            header->dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX &&
            header->dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY &&
            header->dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY &&
            header->dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ &&
            header->dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ)
        {
            *_textureType = MOJODDS_TEXTURE_CUBE;
        }
        else if (header->dwCaps2 & DDSCAPS2_VOLUME)
        {
            *_textureType = MOJODDS_TEXTURE_VOLUME;
        }
    }

    // figure out how much memory makes up a single face mip chain.
    if (*_textureType == MOJODDS_TEXTURE_CUBE)
    {
        uint32 wd = header->dwWidth;
        uint32 ht = header->dwHeight;
        if (wd != ht)
        {
            // cube maps must be square
            return 0;
        }
        *_cubemapfacelen = 0;
        for (i = 0; i < (int)*_miplevels; i++)
        {
            uint32_t mipLen = MAX((wd + blockDim - 1) / blockDim, 1) * MAX((ht + blockDim - 1) / blockDim, 1) * blockSize;
            if (UINT32_MAX - mipLen < *_cubemapfacelen) {
                // data size would overflow 32-bit uint, invalid file
                return 0;
            }
            *_cubemapfacelen += mipLen;
            wd >>= 1;
            ht >>= 1;
        }

        // 6 because cube faces
        if (*len < (*_cubemapfacelen) * 6) {
            return 0;
        }
    }
    else if (*_textureType == MOJODDS_TEXTURE_2D)
    {
        // check that file contains enough data like the header says
        // TODO: also do this for other texture types
        uint32 wd = header->dwWidth;
        uint32 ht = header->dwHeight;
        uint32_t dataLen = 0;
        for (i = 0; i < (int)*_miplevels; i++)
        {
            uint32_t mipLen = MAX((wd + blockDim - 1) / blockDim, 1) * MAX((ht + blockDim - 1) / blockDim, 1) * blockSize;
            if (UINT32_MAX - mipLen < dataLen) {
                // data size would overflow 32-bit uint, invalid file
                return 0;
            }
            dataLen += mipLen;
            wd >>= 1;
            ht >>= 1;
        }

        if (*len < dataLen) {
            return 0;
        }
    }

    if (header->dwPitchOrLinearSize > *len) {
        // dwPitchOrLinearSize is incorrect
        return 0;
    }

    if (calcSize > *len) {
        // there's not enough data to contain the advertised images
        // trying to read mips would fail
        return 0;
    }

    return 1;
} // parse_dds


// !!! FIXME: improve the crap out of this API later.
int MOJODDS_isDDS(const void *_ptr, const unsigned long _len)
{
    size_t len = (size_t) _len;
    const uint8 *ptr = (const uint8 *) _ptr;
    return (readui32(&ptr, &len) == DDS_MAGIC);
} // MOJODDS_isDDS

int MOJODDS_getTexture(const void *_ptr, const unsigned long _len,
                       const void **_tex, unsigned long *_texlen,
                       unsigned int *_glfmt, unsigned int *_w,
                       unsigned int *_h, unsigned int *_miplevels,
                       unsigned int *_cubemapfacelen,
                       MOJODDS_textureType *_textureType)
{
    size_t len = (size_t) _len;
    const uint8 *ptr = (const uint8 *) _ptr;
    MOJODDS_Header header;
    if (!parse_dds(&header, &ptr, &len, _glfmt, _miplevels, _cubemapfacelen, _textureType))
        return 0;

    *_tex = (const void *) ptr;
    *_w = (unsigned int) header.dwWidth;
    *_h = (unsigned int) header.dwHeight;
    *_texlen = (unsigned long) header.dwPitchOrLinearSize;

    if (header.dwFlags & DDSD_PITCH)
        *_texlen *= header.dwHeight;

    return 1;
} // MOJODDS_getTexture

int MOJODDS_getMipMapTexture(unsigned int miplevel, unsigned int glfmt,
                             const void*_basetex,
                             unsigned int w, unsigned h,
                             const void **_tex, unsigned long *_texlen,
                             unsigned int *_texw, unsigned int *_texh)
{
    int i;
    const void* newtex;
    unsigned long newtexlen;
    unsigned int neww;
    unsigned int newh;
    uint32 blockDim = 1;
    uint32 blockSize = 0;

    switch (glfmt) {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        blockDim = 4;
        blockSize = 8;
        break;

    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        blockDim = 4;
        blockSize = 16;
        break;

    case GL_BGR:
        blockSize = 3;
        break;

    case GL_BGRA:
        blockSize = 4;
        break;

    case GL_LUMINANCE_ALPHA:
        blockSize = 2;
        break;

    default:
        assert(!"unsupported GL format");
        break;
    }

    assert(blockSize != 0);

    newtex = _basetex;
    neww = w;
    newh = h;
    newtexlen = ((neww + blockDim - 1) / blockDim) * ((newh + blockDim - 1) / blockDim) * blockSize;

    // Calculate size of miplevel
    for (i=0; i < miplevel; ++i)
    {
        // move position to next texture start
        newtex += newtexlen;
        // calculate texture size
        neww >>= 1;
        newh >>= 1;
        if (neww < 1) neww = 1;
        if (newh < 1) newh = 1;
        newtexlen = ((neww + blockDim - 1) / blockDim) * ((newh + blockDim - 1) / blockDim) * blockSize;
    } // for

    *_tex = newtex;
    if (_texlen) {
        *_texlen = newtexlen;
    }
    *_texw = neww;
    *_texh = newh;
    return 1;
} // MOJODDS_getMipMapTexture


int MOJODDS_getCubeFace(MOJODDS_cubeFace cubeFace, unsigned int miplevel,
                        unsigned int glfmt, const void *_basetex,
                        unsigned long _cubemapfacelen, unsigned int w, unsigned h,
                        const void **_tex, unsigned long *_texlen,
                        unsigned int *_texw, unsigned int *_texh)
{
    // pick correct face
    const char *faceBaseTex = (const char *) _basetex;
    faceBaseTex = faceBaseTex + cubeFace * _cubemapfacelen;


    // call MOJODDS_getMipMapTexture to get offset in that face
    return MOJODDS_getMipMapTexture(miplevel, glfmt, faceBaseTex, w, h, _tex, _texlen, _texw, _texh);
}


// end of mojodds.c ...

