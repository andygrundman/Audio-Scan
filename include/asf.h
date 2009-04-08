/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#ifndef __APPLE__
#  include <endian.h>
#else
#  include <machine/endian.h>
#  define __LITTLE_ENDIAN  LITTLE_ENDIAN
#  define __BIG_ENDIAN     BIG_ENDIAN
#  define __BYTE_ORDER     BYTE_ORDER
#endif
 
#define ASF_BLOCK_SIZE 8192

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
# define _PACKED __attribute((packed))
#else
# define _PACKED
#endif

 typedef struct _GUID {
   uint32_t l;
   uint16_t w[2];
   uint8_t  b[8];
 } _PACKED GUID;

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
  GUID name = {l, {w1, w2}, {b1, b2, b3, b4, b5, b6, b7, b8}}
  
#define IsEqualGUID(rguid1, rguid2) (!memcmp(rguid1, rguid2, sizeof(GUID)))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SWAP32(l) (l)
#define SWAP16(w) (w)
#else
#define SWAP32(l) ( (((l)>>24)&0x000000ff) | (((l)>>8)&0x0000ff00) | (((l)<<8)&0x00ff0000) | (((l)<<24)&0xff000000) )
#define SWAP16(w) ( (((w)>>8)&0x00ff) | (((w)<<8)&0xff00) )
#endif

DEFINE_GUID(ASF_Header_Object, SWAP32(0x75b22630), SWAP16(0x668e), SWAP16(0x11cf),
      0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_File_Properties, SWAP32(0x8cabdca1), SWAP16(0xa947), SWAP16(0x11cf),
	    0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_Stream_Properties, SWAP32(0xb7dc0791), SWAP16(0xa9b7), SWAP16(0x11cf),
	    0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_ContentDescription, SWAP32(0x75b22633), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_ExtendedContentDescription, SWAP32(0xd2d0a440), SWAP16(0xe307), SWAP16(0x11d2),
	    0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50);

DEFINE_GUID(ASF_ClientGuid, SWAP32(0x8d262e32), SWAP16(0xfc28), SWAP16(0x11d7),
	    0xa9, 0xea, 0x00, 0x04, 0x5a, 0x6b, 0x76, 0xc2);

DEFINE_GUID(ASF_HeaderExtension, SWAP32(0x5fbf03b5), SWAP16(0xa92e), SWAP16(0x11cf),
	    0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_CodecList, SWAP32(0x86d15240), SWAP16(0x311d), SWAP16(0x11d0),
	    0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6);

DEFINE_GUID(ASF_DataObject, SWAP32(0x75b22636), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_PaddingObject, SWAP32(0x1806d474), SWAP16(0xcadf), SWAP16(0x4509),
	    0xa4, 0xba, 0x9a, 0xab, 0xcb, 0x96, 0xaa, 0xe8);

DEFINE_GUID(ASF_SimpleIndexObject, SWAP32(0x33000890), SWAP16(0xe5b1), SWAP16(0x11cf),
	    0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb);

DEFINE_GUID(ASF_NoErrorCorrection, SWAP32(0x20fb5700), SWAP16(0x5b55), SWAP16(0x11cf),
	    0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_AudioSpread, SWAP32(0xbfc3cd50), SWAP16(0x618f), SWAP16(0x11cf),
	    0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20);

DEFINE_GUID(ASF_Reserved1, SWAP32(0xabd3d211), SWAP16(0xa9ba), SWAP16(0x11cf),
	    0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_Reserved2, SWAP32(0x86d15241), SWAP16(0x311d), SWAP16(0x11d0),
	    0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6);

DEFINE_GUID(ASF_ContentEncryptionObject, SWAP32(0x2211B3FB), SWAP16(0xBD23), SWAP16(0x11D2),
	    0xB4, 0xB7, 0x00, 0xA0, 0xC9, 0x55, 0xFC, 0x6E);

DEFINE_GUID(ASF_ExtendedContentEncryptionObject, SWAP32(0x298AE614), SWAP16(0x2622), SWAP16(0x4C17),
	    0xB9, 0x35, 0xDA, 0xE0, 0x7E, 0xE9, 0x28, 0x9C);

DEFINE_GUID(ASF_ExtendedStreamPropertiesObject, SWAP32(0x14E6A5CB), SWAP16(0xC672), SWAP16(0x4332),
	    0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A);

DEFINE_GUID(ASF_MediaTypeAudio, SWAP32(0x31178C9D), SWAP16(0x03E1), SWAP16(0x4528),
	    0xB5, 0x82, 0x3D, 0xF9, 0xDB, 0x22, 0xF5, 0x03);

DEFINE_GUID(ASF_FormatTypeWave, SWAP32(0xC4C4C4D1), SWAP16(0x0049), SWAP16(0x4E2B),
	    0x98, 0xFB, 0x95, 0x37, 0xF6, 0xCE, 0x51, 0x6D);

DEFINE_GUID(ASF_StreamBufferStream, SWAP32(0x3AFB65E2), SWAP16(0x47EF), SWAP16(0x40F2),
	    0xAC, 0x2C, 0x70, 0xA9, 0x0D, 0x71, 0xD3, 0x43);
	    
// Stream types found in Stream Properties
DEFINE_GUID(ASF_Audio_Media, SWAP32(0xf8699e40), SWAP16(0x5b4d), SWAP16(0x11cf),
	    0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_Video_Media, SWAP32(0xbc19efc0), SWAP16(0x5b4d), SWAP16(0x11cf),
	    0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_Command_Media, SWAP32(0x59dacfc0), SWAP16(0x59e6), SWAP16(0x11d0),
	    0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6);

DEFINE_GUID(ASF_JFIF_Media, SWAP32(0xb61be100), SWAP16(0x5b4e), SWAP16(0x11cf),
	    0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_Degradable_JPEG_Media, SWAP32(0x35907de0), SWAP16(0xe415), SWAP16(0x11cf),
      0xa9, 0x17, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_File_Transfer_Media, SWAP32(0x91bd222c), SWAP16(0xf21c), SWAP16(0x497a),
      0x8b, 0x6d, 0x5a, 0xa8, 0x6b, 0xfc, 0x01, 0x85);

DEFINE_GUID(ASF_Binary_Media, SWAP32(0x3afb65e2), SWAP16(0x47ef), SWAP16(0x40f2),
      0xac, 0x2c, 0x70, 0xa9, 0x0d, 0x71, 0xd3, 0x43);

// Error correction types found in Stream Properties
DEFINE_GUID(ASF_No_Error_Correction, SWAP32(0x20fb5700), SWAP16(0x5b55), SWAP16(0x11cf),
      0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b);

DEFINE_GUID(ASF_Audio_Spread, SWAP32(0xbfc3cd50), SWAP16(0x618f), SWAP16(0x11cf),
      0x8b, 0xb2, 0x00, 0xaa, 0x00, 0xb4, 0xe2, 0x20);
      
typedef struct _asf_object_t {
  GUID     ID;
  uint64_t size;
  uint32_t num_objects;
  uint8_t  reserved1;
  uint8_t  reserved2;
} _PACKED ASF_Object;

static int get_asf_metadata(char *file, HV *info, HV *tags);
void _parse_file_properties(Buffer *buf, uint64_t len, HV *info, HV *tags);
void _parse_stream_properties(Buffer *buf, uint64_t len, HV *info, HV *tags);