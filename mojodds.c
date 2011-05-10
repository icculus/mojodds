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

static int parse_dds(MOJODDS_Header *header, const uint8 **ptr, size_t *len)
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
    else if (header->dwCaps != DDSCAPS_TEXTURE)  // !!! FIXME
        return 0;
    else if (header->dwCaps2 != 0)  // !!! FIXME (non-zero with other bits in dwCaps set)
        return 0;
    else if ((header->dwFlags & pitchAndLinear) == pitchAndLinear)
        return 0;  // can't specify both.

    if ((header->ddspf.dwFlags & DDPF_FOURCC) == 0)  // !!! FIXME
        return 0;

    if (header->ddspf.dwFlags & DDPF_FOURCC)
    {
        switch (header->ddspf.dwFourCC)
        {
            case FOURCC_DXT1:
                calcSize = ((width ? ((width + 3) / 4) : 1) * 8) * height;
                break;
            case FOURCC_DXT2:
            case FOURCC_DXT3:
            case FOURCC_DXT4:
            case FOURCC_DXT5:
                calcSize = ((width ? ((width + 3) / 4) : 1) * 16) * height;
                break;
            // !!! FIXME: DX10 is an extended header, introduced by DirectX 10.
            //case FOURCC_DX10:
            default:
                return 0;  // unsupported data format.
        } // switch
    } // if

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
                       const void **_tex, unsigned long *_texlen, int *_dxtver,
                       unsigned int *_w, unsigned int *_h)
{
    size_t len = (size_t) _len;
    const uint8 *ptr = (const uint8 *) _ptr;
    MOJODDS_Header header;
    if (!parse_dds(&header, &ptr, &len))
        return 0;

    *_tex = (const void *) ptr;
    *_texlen = (unsigned long) header.dwPitchOrLinearSize;
    *_w = (unsigned int) header.dwWidth;
    *_h = (unsigned int) header.dwHeight;

    switch (header.ddspf.dwFourCC)
    {
        case FOURCC_DXT1: *_dxtver = 1; break;
        case FOURCC_DXT2: *_dxtver = 2; break;
        case FOURCC_DXT3: *_dxtver = 3; break;
        case FOURCC_DXT4: *_dxtver = 4; break;
        case FOURCC_DXT5: *_dxtver = 5; break;
        default: *_dxtver = 0; return 0;
    } // switch

    return 1;
} // MOJODDS_getTexture

// end of mojodds.c ...

