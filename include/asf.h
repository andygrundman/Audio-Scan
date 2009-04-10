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

DEFINE_GUID(ASF_Content_Description, SWAP32(0x75b22633), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_File_Properties, SWAP32(0x8cabdca1), SWAP16(0xa947), SWAP16(0x11cf),
	    0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_Stream_Properties, SWAP32(0xb7dc0791), SWAP16(0xa9b7), SWAP16(0x11cf),
	    0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_Header_Extension, SWAP32(0x5fbf03b5), SWAP16(0xa92e), SWAP16(0x11cf),
	    0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65);

DEFINE_GUID(ASF_Compatibility, SWAP32(0x26f18b5d), SWAP16(0x4584), SWAP16(0x47ec),
	    0x9f, 0x5f, 0x0e, 0x65, 0x1f, 0x04, 0x52, 0xc9);

DEFINE_GUID(ASF_Metadata, SWAP32(0xc5f8cbea), SWAP16(0x5baf), SWAP16(0x4877),
	    0x84, 0x67, 0xaa, 0x8c, 0x44, 0xfa, 0x4c, 0xca);

DEFINE_GUID(ASF_Padding, SWAP32(0x1806d474), SWAP16(0xcadf), SWAP16(0x4509),
	    0xa4, 0xba, 0x9a, 0xab, 0xcb, 0x96, 0xaa, 0xe8);

DEFINE_GUID(ASF_Extended_Stream_Properties, SWAP32(0x14E6A5CB), SWAP16(0xC672), SWAP16(0x4332),
	    0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A);

DEFINE_GUID(ASF_Extended_Content_Description, SWAP32(0xd2d0a440), SWAP16(0xe307), SWAP16(0x11d2),
	    0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50);
	    
DEFINE_GUID(ASF_Language_List, SWAP32(0x7c4346a9), SWAP16(0xefe0), SWAP16(0x4bfc),
	    0xb2, 0x29, 0x39, 0x3e, 0xde, 0x41, 0x5c, 0x85);

DEFINE_GUID(ASF_Advanced_Mutual_Exclusion, SWAP32(0xa08649cf), SWAP16(0x4775), SWAP16(0x4670),
	    0x8a, 0x16, 0x6e, 0x35, 0x35, 0x75, 0x66, 0xcd);

DEFINE_GUID(ASF_Index_Parameters, SWAP32(0xd6e229df), SWAP16(0x35da), SWAP16(0x11d1),
	    0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe);

DEFINE_GUID(ASF_Codec_List, SWAP32(0x86d15240), SWAP16(0x311d), SWAP16(0x11d0),
      0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6);

DEFINE_GUID(ASF_Stream_Bitrate_Properties, SWAP32(0x7bf875ce), SWAP16(0x468d), SWAP16(0x11d1),
	    0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2);

DEFINE_GUID(ASF_Metadata_Library, SWAP32(0x44231c94), SWAP16(0x9498), SWAP16(0x49d1),
	    0xa1, 0x41, 0x1d, 0x13, 0x4e, 0x45, 0x70, 0x54);

// XXX: Can't find any documentation on this object, we will just ignore it
DEFINE_GUID(ASF_Index_Placeholder, SWAP32(0xd9aade20), SWAP16(0x7c17), SWAP16(0x4f9c),
	    0xbc, 0x28, 0x85, 0x55, 0xdd, 0x98, 0xe2, 0xa2);

DEFINE_GUID(ASF_Error_Correction, SWAP32(0x75b22635), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_Data, SWAP32(0x75b22636), SWAP16(0x668e), SWAP16(0x11cf),
	    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c);

DEFINE_GUID(ASF_Index, SWAP32(0xd6e229d3), SWAP16(0x35da), SWAP16(0x11d1),
	    0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe);

DEFINE_GUID(ASF_Simple_Index, SWAP32(0x33000890), SWAP16(0xe5b1), SWAP16(0x11cf),
	    0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb);
	    
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

// Mutual Exclusion types
DEFINE_GUID(ASF_Mutex_Language, SWAP32(0xd6e22a00), SWAP16(0x35da), SWAP16(0x11d1),
      0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe);

DEFINE_GUID(ASF_Mutex_Bitrate, SWAP32(0xd6e22a01), SWAP16(0x35da), SWAP16(0x11d1),
      0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe);

DEFINE_GUID(ASF_Mutex_Unknown, SWAP32(0xd6e22a02), SWAP16(0x35da), SWAP16(0x11d1),
      0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe);
      
typedef struct _asf_object_t {
  GUID     ID;
  uint64_t size;
  uint32_t num_objects;
  uint8_t  reserved1;
  uint8_t  reserved2;
} _PACKED ASF_Object;

enum types {
  TYPE_UNICODE,
  TYPE_BYTE,
  TYPE_BOOL,
  TYPE_DWORD,
  TYPE_QWORD,
  TYPE_WORD,
  TYPE_GUID
};

static int get_asf_metadata(char *file, HV *info, HV *tags);
void _parse_content_description(Buffer *buf, HV *info, HV *tags);
void _parse_extended_content_description(Buffer *buf, HV *info, HV *tags);
void _parse_file_properties(Buffer *buf, HV *info, HV *tags);
void _parse_stream_properties(Buffer *buf, HV *info, HV *tags);
void _store_stream_info(int stream_number, HV *info, SV *key, SV *value);
void _store_tag(HV *tags, SV *key, SV *value);
int _parse_header_extension(Buffer *buf, uint64_t len, HV *info, HV *tags);
void _parse_metadata(Buffer *buf, HV *info, HV *tags);
void _parse_extended_stream_properties(Buffer *buf, uint64_t len, HV *info, HV *tags);
void _parse_language_list(Buffer *buf, HV *info, HV *tags);
void _parse_advanced_mutual_exclusion(Buffer *buf, HV *info, HV *tags);
void _parse_codec_list(Buffer *buf, HV *info, HV *tags);
void _parse_stream_bitrate_properties(Buffer *buf, HV *info, HV *tags);
void _parse_metadata_library(Buffer *buf, HV *info, HV *tags);
void _parse_index_parameters(Buffer *buf, HV *info, HV *tags);
int _parse_index_objects(PerlIO *infile, int index_size, uint64_t audio_offset, Buffer *buf, HV *info, HV *tags);
void _parse_index(Buffer *buf, uint64_t audio_offset, HV *info, HV *tags);