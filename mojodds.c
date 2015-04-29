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
                     unsigned int *_glfmt, unsigned int *_miplevels)
{
    const uint32 pitchAndLinear = (DDSD_PITCH | DDSD_LINEARSIZE);
    uint32 width = 0;
    uint32 height = 0;
    uint32 calcSize = 0;
    uint32 calcSizeFlag = DDSD_LINEARSIZE;
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

    header->dwCaps &= ~DDSCAPS_ALPHA;  // we'll get this from the pixel format.

    if (header->dwSize != DDS_HEADERSIZE)   // header size must be 124.
        return 0;
    else if (header->ddspf.dwSize != DDS_PIXFMTSIZE)   // size must be 32.
        return 0;
    else if ((header->dwFlags & DDSD_REQ) != DDSD_REQ)  // must have these bits.
        return 0;
    else if ((header->dwCaps & DDSCAPS_TEXTURE) == 0)
        return 0;
    else if (header->dwCaps2 != 0)  // !!! FIXME (non-zero with other bits in dwCaps set)
        return 0;
    else if ((header->dwFlags & pitchAndLinear) == pitchAndLinear)
        return 0;  // can't specify both.

    *_miplevels = (header->dwCaps & DDSCAPS_MIPMAP) ? header->dwMipMapCount : 1;

    if (header->ddspf.dwFlags & DDPF_FOURCC)
    {
        switch (header->ddspf.dwFourCC)
        {
            case FOURCC_DXT1:
                *_glfmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                calcSize = ((width ? ((width + 3) / 4) : 1) * 8) *
                           (height ? ((height + 3) / 4) : 1);
                break;
            case FOURCC_DXT3:
                *_glfmt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                calcSize = ((width ? ((width + 3) / 4) : 1) * 16) *
                           (height ? ((height + 3) / 4) : 1);
                break;
            case FOURCC_DXT5:
                *_glfmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                calcSize = ((width ? ((width + 3) / 4) : 1) * 16) *
                           (height ? ((height + 3) / 4) : 1);
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
        } // if
        else
        {
            if (header->ddspf.dwRGBBitCount != 24)
                return 0;  // unsupported.
            *_glfmt = GL_BGR;
        } // else

        calcSizeFlag = DDSD_PITCH;
        calcSize = ((width * header->ddspf.dwRGBBitCount) + 7) / 8;
    }
    else if (header->ddspf.dwFlags & (DDPF_LUMINANCE | DDPF_ALPHA) )
    {
        *_glfmt = GL_LUMINANCE_ALPHA;

        calcSizeFlag = DDSD_PITCH;
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
                       unsigned int *_h, unsigned int *_miplevels)
{
    size_t len = (size_t) _len;
    const uint8 *ptr = (const uint8 *) _ptr;
    MOJODDS_Header header;
    if (!parse_dds(&header, &ptr, &len, _glfmt, _miplevels))
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
                             const void*_basetex, const unsigned long _basetexlen,
                             unsigned int w, unsigned h,
                             const void **_tex, unsigned long *_texlen,
                             unsigned int *_texw, unsigned int *_texh)
{
    int i;
    const void* newtex;
    unsigned long newtexlen;
    unsigned int neww;
    unsigned int newh;

    newtex = _basetex;
    newtexlen = _basetexlen;
    neww = w;
    newh = h;

    // Calculate size of miplevel
    for (i=0; i < miplevel; ++i)
    {
        // move position to next texture start
        newtex += newtexlen;
        // calculate texture size
        newtexlen >>= 2;
        neww >>= 1;
        newh >>= 1;
        if (neww < 1) neww = 1;
        if (newh < 1) newh = 1;
        switch (glfmt) {
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                if (newtexlen < 8) newtexlen = 8;
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                if (newtexlen < 16) newtexlen = 16;
                break;
        } // switch
    } // for

    *_tex = newtex;
    if (_texlen) {
        *_texlen = newtexlen;
    }
    *_texw = neww;
    *_texh = newh;
    return 1;
} // MOJODDS_getMipMapTexture


// end of mojodds.c ...

