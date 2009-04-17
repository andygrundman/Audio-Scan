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
 
/*
  TODO:
  These will be added when I see a real file that uses them.

  Header objects:
  
  Marker (3.7)
  Bitrate Mutual Exclusion (3.8)
  Content Branding (3.13)
  
  Header Extension objects:
  
  Group Mutual Exclusion (4.3)
  Stream Prioritization (4.4)
  Bandwidth Sharing (4.5)
  Media Object Index Parameters (4.10)
  Timecode Index Parameters (4.11)
  Advanced Content Encryption (4.13)
  
  Index objects:
  
  Media Object Index (6.3)
  Timecode Index (6.4)
*/

#include "asf.h"

static void
print_guid(GUID guid)
{
  PerlIO_printf(PerlIO_stderr(),
    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x ",
    guid.l, guid.w[0], guid.w[1],
    guid.b[0], guid.b[1], guid.b[2], guid.b[3],
    guid.b[4], guid.b[5], guid.b[6], guid.b[7]
  );
}

static int
get_asf_metadata(PerlIO *infile, char *file, HV *info, HV *tags)
{
  Buffer asf_buf;
  ASF_Object hdr;
  ASF_Object data;
  ASF_Object tmp;
  
  off_t file_size;
  
  int err = 0;
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  buffer_init(&asf_buf, 0);
  
  if ( !_check_buf(infile, &asf_buf, ASF_BLOCK_SIZE, ASF_BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }
  
  buffer_get_guid(&asf_buf, &hdr.ID);
  
  if ( !IsEqualGUID(&hdr.ID, &ASF_Header_Object) ) {
    PerlIO_printf(PerlIO_stderr(), "Invalid ASF header: %s\n", file);
    err = -1;
    goto out;
  }
  
  hdr.size        = buffer_get_int64_le(&asf_buf);
  hdr.num_objects = buffer_get_int_le(&asf_buf);
  hdr.reserved1   = buffer_get_char(&asf_buf);
  hdr.reserved2   = buffer_get_char(&asf_buf);
  
  if ( hdr.reserved2 != 0x02 ) {
    PerlIO_printf(PerlIO_stderr(), "Invalid ASF header: %s\n", file);
    err = -1;
    goto out;
  }
  
  while ( hdr.num_objects-- ) {
    if ( !_check_buf(infile, &asf_buf, 24, ASF_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    buffer_get_guid(&asf_buf, &tmp.ID);
    tmp.size = buffer_get_int64_le(&asf_buf);
    
    if ( !_check_buf(infile, &asf_buf, tmp.size - 24, ASF_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    if ( IsEqualGUID(&tmp.ID, &ASF_Content_Description) ) {
      DEBUG_TRACE("Content_Description\n");
      _parse_content_description(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_File_Properties) ) {
      DEBUG_TRACE("File_Properties\n");
      _parse_file_properties(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Stream_Properties) ) {
      DEBUG_TRACE("Stream_Properties\n");
      _parse_stream_properties(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Extended_Content_Description) ) {
      DEBUG_TRACE("Extended_Content_Description\n");
      _parse_extended_content_description(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Codec_List) ) {
      DEBUG_TRACE("Codec_List\n");
      _parse_codec_list(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Stream_Bitrate_Properties) ) {
      DEBUG_TRACE("Stream_Bitrate_Properties\n");
      _parse_stream_bitrate_properties(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Content_Encryption) ) {
      DEBUG_TRACE("Content_Encryption\n");
      _parse_content_encryption(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Extended_Content_Encryption) ) {
      DEBUG_TRACE("Extended_Content_Encryption\n");
      _parse_extended_content_encryption(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Script_Command) ) {
      DEBUG_TRACE("Script_Command\n");
      _parse_script_command(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Digital_Signature) ) {
      DEBUG_TRACE("Skipping Digital_Signature\n");
      buffer_consume(&asf_buf, tmp.size - 24);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Header_Extension) ) {
      DEBUG_TRACE("Header_Extension\n");
      if ( !_parse_header_extension(&asf_buf, tmp.size, info, tags) ) {
        PerlIO_printf(PerlIO_stderr(), "Invalid ASF file: %s (invalid header extension object)\n", file);
        err = -1;
        goto out;
      }
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Error_Correction) ) {
      DEBUG_TRACE("Skipping Error_Correction\n");
      buffer_consume(&asf_buf, tmp.size - 24);
    }
    else {
      // Unhandled GUID
      PerlIO_printf(PerlIO_stderr(), "** Unhandled GUID: ");
      print_guid(tmp.ID);
      PerlIO_printf(PerlIO_stderr(), "size: %lu\n", tmp.size);
      
      buffer_consume(&asf_buf, tmp.size - 24);
    }
  }
  
  // We should be at the start of the Data object.
  // Seek past it to find more objects
  if ( !_check_buf(infile, &asf_buf, 24, ASF_BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }
  
  buffer_get_guid(&asf_buf, &data.ID);
  
  if ( !IsEqualGUID(&data.ID, &ASF_Data) ) {
    PerlIO_printf(PerlIO_stderr(), "Invalid ASF file: %s (no Data object after Header)\n", file);
    err = -1;
    goto out;
  }
  
  // Store offset to beginning of data (50 goes past the top-level data packet)
  my_hv_store( info, "audio_offset", newSVuv(hdr.size + 50) );
  
  data.size = buffer_get_int64_le(&asf_buf);
  
  if ( hdr.size + data.size < file_size ) {
    DEBUG_TRACE("Seeking past data: %lu\n", hdr.size + data.size);
      
    if ( PerlIO_seek(infile, hdr.size + data.size, SEEK_SET) != 0 ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid ASF file: %s (Invalid Data object size)\n", file);
      err = -1;
      goto out;
    }
    
    buffer_clear(&asf_buf);

    if ( !_parse_index_objects(infile, file_size - hdr.size - data.size, hdr.size + 50, &asf_buf, info, tags) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid ASF file: %s (Invalid Index object)\n", file);
      err = -1;
      goto out;
    }
  }
  
out:
  buffer_free(&asf_buf);

  if (err) return err;

  return 0;
}

void
_parse_content_description(Buffer *buf, HV *info, HV *tags)
{
  int i;
  uint16_t len[5];
  char fields[5][12] = {
    { "Title" },
    { "Author" },
    { "Copyright" },
    { "Description" },
    { "Rating" }
  };
  
  for (i = 0; i < 5; i++) {
    len[i] = buffer_get_short_le(buf);
  }
  
  for (i = 0; i < 5; i++) {
    SV *value;
    Buffer utf8_buf;
    
    if ( len[i] ) {
      buffer_get_utf16le_as_utf8(buf, &utf8_buf, len[i]);
      value = newSVpv( buffer_ptr(&utf8_buf), 0 );
      sv_utf8_decode(value);
    
      _store_tag( tags, newSVpv(fields[i], 0), value );
    
      buffer_free(&utf8_buf);
    }
  }
}

void
_parse_extended_content_description(Buffer *buf, HV *info, HV *tags)
{
  uint16_t count = buffer_get_short_le(buf);
  
  while ( count-- ) {
    uint16_t name_len;
    uint16_t data_type;
    uint16_t value_len;
    SV *key = NULL;
    SV *value = NULL;
    Buffer utf8_buf;
    
    name_len = buffer_get_short_le(buf);
    
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, name_len);
    key = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(key);
    buffer_free(&utf8_buf);
    
    data_type = buffer_get_short_le(buf);
    value_len = buffer_get_short_le(buf);
    
    if (data_type == TYPE_UNICODE) {
      buffer_get_utf16le_as_utf8(buf, &utf8_buf, value_len);
      value = newSVpv( buffer_ptr(&utf8_buf), 0 );
      sv_utf8_decode(value);
      buffer_free(&utf8_buf);
    }
    else if (data_type == TYPE_BYTE) {
      // handle picture data, interestingly it is compatible with the ID3v2 APIC frame
      if ( !strcmp( SvPVX(key), "WM/Picture" ) ) {
        value = _parse_picture(buf);
      }
      else {
        value = newSVpvn( buffer_ptr(buf), value_len );
        buffer_consume(buf, value_len);
      }
    }
    else if (data_type == TYPE_BOOL) {
      value = newSViv( buffer_get_int_le(buf) );
    }
    else if (data_type == TYPE_DWORD) {
      value = newSViv( buffer_get_int_le(buf) );
    }
    else if (data_type == TYPE_QWORD) {
      value = newSViv( buffer_get_int64_le(buf) );
    }
    else if (data_type == TYPE_WORD) {
      value = newSViv( buffer_get_short_le(buf) );
    }
    else {
      PerlIO_printf(PerlIO_stderr(), "Unknown extended content description data type %d\n", data_type);
      buffer_consume(buf, value_len);
    }
    
    if (value != NULL) {
#ifdef DEBUG
      if ( data_type == 0 ) {
        DEBUG_TRACE("  %s / type %d / %s\n", SvPVX(key), data_type, SvPVX(value));
      }
      else if ( data_type > 1 ) {
        DEBUG_TRACE("  %s / type %d / %d\n", SvPVX(key), data_type, SvIV(value));
      }
      else {
        DEBUG_TRACE("  %s / type %d / <binary>\n", SvPVX(key), data_type);
      }
#endif
      
      _store_tag( tags, key, value );
    }
  }
}
  
void
_parse_file_properties(Buffer *buf, HV *info, HV *tags)
{
  GUID file_id;
  uint64_t file_size;
  uint64_t creation_date;
  uint64_t data_packets;
  uint64_t play_duration;
  uint64_t send_duration;
  uint64_t preroll;
  uint32_t flags;
  uint32_t min_packet_size;
  uint32_t max_packet_size;
  uint32_t max_bitrate;
  uint8_t broadcast;
  uint8_t seekable;
  
  buffer_get_guid(buf, &file_id);
  my_hv_store( 
    info, "file_id", newSVpvf( "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      file_id.l, file_id.w[0], file_id.w[1],
      file_id.b[0], file_id.b[1], file_id.b[2], file_id.b[3],
      file_id.b[4], file_id.b[5], file_id.b[6], file_id.b[7]
    )
  );
  
  file_size       = buffer_get_int64_le(buf);
  creation_date   = buffer_get_int64_le(buf);
  data_packets    = buffer_get_int64_le(buf);
  play_duration   = buffer_get_int64_le(buf);
  send_duration   = buffer_get_int64_le(buf);
  preroll         = buffer_get_int64_le(buf);
  flags           = buffer_get_int_le(buf);
  min_packet_size = buffer_get_int_le(buf);
  max_packet_size = buffer_get_int_le(buf);
  max_bitrate     = buffer_get_int_le(buf);
  
  broadcast = flags & 0x01 ? 1 : 0;
  seekable  = flags & 0x02 ? 1 : 0;
  
  if ( !broadcast ) {
    creation_date = (creation_date - 116444736000000000ULL) / 10000000;
    play_duration /= 10000;
    send_duration /= 10000;
    
    my_hv_store( info, "file_size", newSViv(file_size) );
    my_hv_store( info, "creation_date", newSViv(creation_date) );
    my_hv_store( info, "data_packets", newSViv(data_packets) );
    my_hv_store( info, "play_duration_ms", newSViv(play_duration) );
    my_hv_store( info, "send_duration_ms", newSViv(send_duration) );
    
    // Calculate actual song duration
    my_hv_store( info, "song_length_ms", newSViv( play_duration - preroll ) );
  }
  
  my_hv_store( info, "preroll", newSViv(preroll) );
  my_hv_store( info, "broadcast", newSViv(broadcast) );
  my_hv_store( info, "seekable", newSViv(seekable) );
  my_hv_store( info, "min_packet_size", newSViv(min_packet_size) );
  my_hv_store( info, "max_packet_size", newSViv(max_packet_size) );
  my_hv_store( info, "max_bitrate", newSViv(max_bitrate) );
}

void
_parse_stream_properties(Buffer *buf, HV *info, HV *tags)
{
  GUID stream_type;
  GUID ec_type;
  uint64_t time_offset;
  uint32_t type_data_len;
  uint32_t ec_data_len;
  uint16_t flags;
  uint16_t stream_number;
  Buffer type_data_buf;
  
  buffer_get_guid(buf, &stream_type);
  buffer_get_guid(buf, &ec_type);
  time_offset = buffer_get_int64_le(buf);
  type_data_len = buffer_get_int_le(buf);
  ec_data_len   = buffer_get_int_le(buf);
  flags         = buffer_get_short_le(buf);
  stream_number = flags & 0x007f;
  
  // skip reserved bytes
  buffer_consume(buf, 4);
  
  // type-specific data
  buffer_init(&type_data_buf, type_data_len);
  buffer_append(&type_data_buf, buffer_ptr(buf), type_data_len);
  buffer_consume(buf, type_data_len);
  
  // skip error-correction data
  buffer_consume(buf, ec_data_len);
  
  if ( IsEqualGUID(&stream_type, &ASF_Audio_Media) ) {
    uint8_t is_wma = 0;
    uint16_t codec_id;
    
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Audio_Media", 0) );
    
    // Parse WAVEFORMATEX data
    codec_id = buffer_get_short_le(&type_data_buf);
    switch (codec_id) {
      case 0x000a:
      case 0x0161:
      case 0x0162:
      case 0x0163:
        is_wma = 1;
        break;
    }
    
    _store_stream_info( stream_number, info, newSVpv("codec_id", 0), newSViv(codec_id) );
    _store_stream_info( stream_number, info, newSVpv("channels", 0), newSViv( buffer_get_short_le(&type_data_buf) ) );
    _store_stream_info( stream_number, info, newSVpv("samplerate", 0), newSViv( buffer_get_int_le(&type_data_buf) ) );
    _store_stream_info( stream_number, info, newSVpv("avg_bytes_per_sec", 0), newSViv( buffer_get_int_le(&type_data_buf) ) );
    _store_stream_info( stream_number, info, newSVpv("block_alignment", 0), newSViv( buffer_get_short_le(&type_data_buf) ) );
    _store_stream_info( stream_number, info, newSVpv("bits_per_sample", 0), newSViv( buffer_get_short_le(&type_data_buf) ) );
    
    // Read WMA-specific data
    if (is_wma) {
      buffer_consume(&type_data_buf, 2);
      _store_stream_info( stream_number, info, newSVpv("samples_per_block", 0), newSViv( buffer_get_int_le(&type_data_buf) ) );
      _store_stream_info( stream_number, info, newSVpv("encode_options", 0), newSViv( buffer_get_short_le(&type_data_buf) ) );
      _store_stream_info( stream_number, info, newSVpv("super_block_align", 0), newSViv( buffer_get_int_le(&type_data_buf) ) );
    }
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Video_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Video_Media", 0) );
    
    DEBUG_TRACE("type_data_len: %d\n", type_data_len);
    
    // Read video-specific data
    _store_stream_info( stream_number, info, newSVpv("width", 0), newSVuv( buffer_get_int_le(&type_data_buf) ) );
    _store_stream_info( stream_number, info, newSVpv("height", 0), newSVuv( buffer_get_int_le(&type_data_buf) ) );
        
    // Skip format size, width, height, reserved
    buffer_consume(&type_data_buf, 17);
    
    _store_stream_info( stream_number, info, newSVpv("bpp", 0), newSVuv( buffer_get_short_le(&type_data_buf) ) );
    
    _store_stream_info( stream_number, info, newSVpv("compression_id", 0), newSVpv( buffer_ptr(&type_data_buf), 4 ) );

    // Rest of the data does not seem to apply to video    
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Command_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Command_Media", 0) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_JFIF_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_JFIF_Media", 0) );
    
    // type-specific data
    _store_stream_info( stream_number, info, newSVpv("width", 0), newSVuv( buffer_get_int_le(&type_data_buf) ) );
    _store_stream_info( stream_number, info, newSVpv("height", 0), newSVuv( buffer_get_int_le(&type_data_buf) ) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Degradable_JPEG_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Degradable_JPEG_Media", 0) );
    
    // XXX: type-specific data (section 9.4.2)
  }
  else if ( IsEqualGUID(&stream_type, &ASF_File_Transfer_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_File_Transfer_Media", 0) );
    
    // XXX: type-specific data (section 9.5)
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Binary_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Binary_Media", 0) );
    
    // XXX: type-specific data (section 9.5)
  }
  
  if ( IsEqualGUID(&ec_type, &ASF_No_Error_Correction) ) {
    _store_stream_info( stream_number, info, newSVpv("error_correction_type", 0), newSVpv("ASF_No_Error_Correction", 0) );
  }
  else if ( IsEqualGUID(&ec_type, &ASF_Audio_Spread) ) {
    _store_stream_info( stream_number, info, newSVpv("error_correction_type", 0), newSVpv("ASF_Audio_Spread", 0) );
  }
  
  _store_stream_info( stream_number, info, newSVpv("time_offset", 0), newSViv(time_offset) );
  _store_stream_info( stream_number, info, newSVpv("encrypted", 0), newSVuv( flags & 0x8000 ? 1 : 0 ) );
  
  buffer_free(&type_data_buf);
}

int
_parse_header_extension(Buffer *buf, uint64_t len, HV *info, HV *tags)
{
  int ext_size;
  GUID hdr;
  uint64_t hdr_size;
  
  // Skip reserved fields
  buffer_consume(buf, 18);
  
  ext_size = buffer_get_int_le(buf);
  
  // Sanity check ext size
  // Must be 0 or 24+, and 46 less than header extension object size
  if (ext_size > 0) {
    if (ext_size < 24) {
      return 0;
    }
    if (ext_size != len - 46) {
      return 0;
    }
  }
  
  DEBUG_TRACE("  size: %d\n", ext_size);
  
  while (ext_size > 0) {
    buffer_get_guid(buf, &hdr);
    hdr_size = buffer_get_int64_le(buf);
    ext_size -= hdr_size;
    
    if ( IsEqualGUID(&hdr, &ASF_Metadata) ) {
      DEBUG_TRACE("  Metadata\n");
      _parse_metadata(buf, info, tags);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Extended_Stream_Properties) ) {
      DEBUG_TRACE("  Extended_Stream_Properties size %d\n", hdr_size);
      _parse_extended_stream_properties(buf, hdr_size, info, tags);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Language_List) ) {
      DEBUG_TRACE("  Language_List\n");
      _parse_language_list(buf, info, tags);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Advanced_Mutual_Exclusion) ) {
      DEBUG_TRACE("  Advanced_Mutual_Exclusion\n");
      _parse_advanced_mutual_exclusion(buf, info, tags);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Metadata_Library) ) {
      DEBUG_TRACE("  Metadata_Library\n");
      _parse_metadata_library(buf, info, tags);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Index_Parameters) ) {
      DEBUG_TRACE("  Index_Parameters\n");
      _parse_index_parameters(buf, info, tags);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Compatibility) ) {
      // reserved for future use, just ignore
      DEBUG_TRACE("  Skipping Compatibility\n");
      buffer_consume(buf, 2);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Padding) ) {
      // skip padding
      DEBUG_TRACE("  Skipping Padding\n");
      buffer_consume(buf, hdr_size - 24);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Index_Placeholder) ) {
      // skip undocumented placeholder
      DEBUG_TRACE("  Skipping Index_Placeholder\n");
      buffer_consume(buf, hdr_size - 24);
    }
    else {
      // Unhandled
      PerlIO_printf(PerlIO_stderr(), "  ** Unhandled extended header: ");
      print_guid(hdr);
      PerlIO_printf(PerlIO_stderr(), "size: %lu\n", hdr_size);
      
      buffer_consume(buf, hdr_size - 24);
    }
  }
  
  return 1;
}

void
_parse_metadata(Buffer *buf, HV *info, HV *tags)
{
  uint16_t count = buffer_get_short_le(buf);
  
  while ( count-- ) {
    uint16_t stream_number;
    uint16_t name_len;
    uint16_t data_type;
    uint32_t data_len;
    SV *key = NULL;
    SV *value = NULL;
    Buffer utf8_buf;
    
    // Skip reserved
    buffer_consume(buf, 2);
    
    stream_number = buffer_get_short_le(buf);
    name_len      = buffer_get_short_le(buf);
    data_type     = buffer_get_short_le(buf);
    data_len      = buffer_get_int_le(buf);
    
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, name_len);
    key = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(key);
    buffer_free(&utf8_buf);
    
    if (data_type == TYPE_UNICODE) {
      buffer_get_utf16le_as_utf8(buf, &utf8_buf, data_len);
      value = newSVpv( buffer_ptr(&utf8_buf), 0 );
      sv_utf8_decode(value);
      buffer_free(&utf8_buf);
    }
    else if (data_type == TYPE_BYTE) {
      value = newSVpvn( buffer_ptr(buf), data_len );
      buffer_consume(buf, data_len);
    }
    else if (data_type == TYPE_BOOL || data_type == TYPE_WORD) {
      value = newSViv( buffer_get_short_le(buf) );
    }
    else if (data_type == TYPE_DWORD) {
      value = newSViv( buffer_get_int_le(buf) );
    }
    else if (data_type == TYPE_QWORD) {
      value = newSViv( buffer_get_int64_le(buf) );
    }
    else {
      DEBUG_TRACE("Unknown metadata data type %d\n", data_type);
      buffer_consume(buf, data_len);
    }
    
    if (value != NULL) {
#ifdef DEBUG
      if ( data_type == 0 ) {
        DEBUG_TRACE("    %s / type %d / stream_number %d / %s\n", SvPVX(key), data_type, stream_number, SvPVX(value));
      }
      else if ( data_type > 1 ) {
        DEBUG_TRACE("    %s / type %d / stream_number %d / %d\n", SvPVX(key), data_type, stream_number, SvIV(value));
      }
      else {
        DEBUG_TRACE("    %s / type %d / stream_number %d / <binary>\n", SvPVX(key), stream_number, data_type);
      }
#endif
      
      // If stream_number is available, store the data with the stream info
      if (stream_number > 0) {
        _store_stream_info( stream_number, info, key, value );
      }
      else {
        my_hv_store_ent( info, key, value );
        SvREFCNT_dec(key);
      }
    }
  }
}

void
_parse_extended_stream_properties(Buffer *buf, uint64_t len, HV *info, HV *tags)
{
  uint64_t start_time          = buffer_get_int64_le(buf);
  uint64_t end_time            = buffer_get_int64_le(buf);
  uint32_t bitrate             = buffer_get_int_le(buf);
  uint32_t buffer_size         = buffer_get_int_le(buf);
  uint32_t buffer_fullness     = buffer_get_int_le(buf);
  uint32_t alt_bitrate         = buffer_get_int_le(buf);
  uint32_t alt_buffer_size     = buffer_get_int_le(buf);
  uint32_t alt_buffer_fullness = buffer_get_int_le(buf);
  uint32_t max_object_size     = buffer_get_int_le(buf);
  uint32_t flags               = buffer_get_int_le(buf);
  uint16_t stream_number       = buffer_get_short_le(buf);
  uint16_t lang_id             = buffer_get_short_le(buf);
  uint64_t avg_time_per_frame  = buffer_get_int64_le(buf);
  uint16_t stream_name_count   = buffer_get_short_le(buf);
  uint16_t payload_ext_count   = buffer_get_short_le(buf);
  
  len -= 88;
  
  if (start_time > 0) {
    _store_stream_info( stream_number, info, newSVpv("start_time", 0), newSViv(start_time) );
  }
  
  if (end_time > 0) {
    _store_stream_info( stream_number, info, newSVpv("end_time", 0), newSViv(end_time) );
  }
  
  _store_stream_info( stream_number, info, newSVpv("bitrate", 0), newSViv(bitrate) );
  _store_stream_info( stream_number, info, newSVpv("buffer_size", 0), newSViv(buffer_size) );
  _store_stream_info( stream_number, info, newSVpv("buffer_fullness", 0), newSViv(buffer_fullness) );
  _store_stream_info( stream_number, info, newSVpv("alt_bitrate", 0), newSViv(alt_bitrate) );
  _store_stream_info( stream_number, info, newSVpv("alt_buffer_size", 0), newSViv(alt_buffer_size) );
  _store_stream_info( stream_number, info, newSVpv("alt_buffer_fullness", 0), newSViv(alt_buffer_fullness) );
  _store_stream_info( stream_number, info, newSVpv("alt_buffer_size", 0), newSViv(alt_buffer_size) );
  _store_stream_info( stream_number, info, newSVpv("max_object_size", 0), newSViv(max_object_size) );
  
  if ( flags & 0x01 )
    _store_stream_info( stream_number, info, newSVpv("flag_reliable", 0), newSViv(1) );
  
  if ( flags & 0x02 )
    _store_stream_info( stream_number, info, newSVpv("flag_seekable", 0), newSViv(1) );
  
  if ( flags & 0x04 )
    _store_stream_info( stream_number, info, newSVpv("flag_no_cleanpoint", 0), newSViv(1) );
  
  if ( flags & 0x08 )
    _store_stream_info( stream_number, info, newSVpv("flag_resend_cleanpoints", 0), newSViv(1) );
  
  _store_stream_info( stream_number, info, newSVpv("language_index", 0), newSViv(lang_id) );
  
  if (avg_time_per_frame > 0) {
    // XXX: can't get this to divide properly (?!)
    //_store_stream_info( stream_number, info, newSVpv("avg_time_per_frame", 0), newSVuv(avg_time_per_frame / 10000) );
  }
  
  while ( stream_name_count-- ) {
    uint16_t stream_name_len;
    
    // stream_name_lang_id
    buffer_consume(buf, 2);
    stream_name_len = buffer_get_short_le(buf);
    
    DEBUG_TRACE("stream_name_len: %d\n", stream_name_len);
    
    // XXX, store this?
    buffer_consume(buf, stream_name_len);
    
    len -= 4 + stream_name_len;
  }
  
  while ( payload_ext_count-- ) {
    // Skip
    uint32_t payload_len;
    
    buffer_consume(buf, 18);
    payload_len = buffer_get_int_le(buf);
    buffer_consume(buf, payload_len);
    
    len -= 22 + payload_len;
  }
  
  if (len) {
    // Anything left over means we have an embedded Stream Properties Object
    DEBUG_TRACE("      embedded Stream_Properties, size %d\n", len);
    buffer_consume(buf, 24);
    _parse_stream_properties(buf, info, tags);
  }
}

void
_parse_language_list(Buffer *buf, HV *info, HV *tags)
{
  Buffer utf8_buf;
  AV *list = newAV();
  uint16_t count = buffer_get_short_le(buf);
  
  while ( count-- ) {
    SV *value;
    
    uint8_t len = buffer_get_char(buf);
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, len);
    value = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(value);
    buffer_free(&utf8_buf);
    
    av_push( list, value );
  }
  
  my_hv_store( info, "language_list", newRV_noinc( (SV*)list ) );
}

void
_parse_advanced_mutual_exclusion(Buffer *buf, HV *info, HV *tags)
{
  GUID mutex_type;
  uint16_t count;
  AV *mutex_list;
  HV *mutex_hv = newHV();
  SV *mutex_type_sv;
  AV *mutex_streams = newAV();
  
  buffer_get_guid(buf, &mutex_type);
  count = buffer_get_short_le(buf);
  
  if ( IsEqualGUID(&mutex_type, &ASF_Mutex_Language) ) {
    mutex_type_sv = newSVpv( "ASF_Mutex_Language", 0 );
  }
  else if ( IsEqualGUID(&mutex_type, &ASF_Mutex_Bitrate) ) {
    mutex_type_sv = newSVpv( "ASF_Mutex_Bitrate", 0 );
  }
  else {
    mutex_type_sv = newSVpv( "ASF_Mutex_Unknown", 0 );
  }
  
  while ( count-- ) {
    av_push( mutex_streams, newSViv( buffer_get_short_le(buf) ) );
  }
  
  my_hv_store_ent( mutex_hv, mutex_type_sv, newRV_noinc( (SV *)mutex_streams ) );
  SvREFCNT_dec(mutex_type_sv);
  
  if ( !my_hv_exists( info, "mutex_list" ) ) {
    mutex_list = newAV();
    av_push( mutex_list, newRV_noinc( (SV *)mutex_hv ) );
    my_hv_store( info, "mutex_list", newRV_noinc( (SV *)mutex_list ) );
  }
  else {
    SV **entry = my_hv_fetch( info, "mutex_list" );
    if (entry != NULL) {
      mutex_list = (AV *)SvRV(*entry);
    }
    else {
      return;
    }
    
    av_push( mutex_list, newRV_noinc( (SV *)mutex_hv ) );
  }
}

void
_parse_codec_list(Buffer *buf, HV *info, HV *tags)
{
  uint32_t count;
  AV *list = newAV();
  
  // Skip reserved
  buffer_consume(buf, 16);
  
  count = buffer_get_int_le(buf);
  
  while ( count-- ) {
    HV *codec_info = newHV();
    uint16_t name_len;
    uint16_t desc_len;
    Buffer utf8_buf;
    SV *name = NULL;
    SV *desc = NULL;
    
    uint16_t codec_type = buffer_get_short_le(buf);
    
    switch (codec_type) {
      case 0x0001:
        my_hv_store( codec_info, "type", newSVpv("Video", 0) );
        break;
      case 0x0002:
        my_hv_store( codec_info, "type", newSVpv("Audio", 0) );
        break;
      default:
        my_hv_store( codec_info, "type", newSVpv("Unknown", 0) );
    }
    
    // Unlike other objects, these lengths are the
    // "number of Unicode chars", not bytes, so we need to double it
    name_len = buffer_get_short_le(buf) * 2;
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, name_len);
    name = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(name);
    my_hv_store( codec_info, "name", name );
    
    // Set a 'lossless' flag in info if Lossless codec is used
    if ( strstr( buffer_ptr(&utf8_buf), "Lossless" ) ) {
      my_hv_store( info, "lossless", newSVuv(1) );
    }
    
    buffer_free(&utf8_buf);
    
    desc_len = buffer_get_short_le(buf) * 2;
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, desc_len);
    desc = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(desc);
    my_hv_store( codec_info, "description", desc );
    buffer_free(&utf8_buf);
    
    // Skip info
    buffer_consume(buf, buffer_get_short_le(buf));
    
    av_push( list, newRV_noinc( (SV *)codec_info ) );
  }
  
  my_hv_store( info, "codec_list", newRV_noinc( (SV *)list ) );
}

void
_parse_stream_bitrate_properties(Buffer *buf, HV *info, HV *tags)
{
  uint16_t count = buffer_get_short_le(buf);
  
  while ( count-- ) {
    uint16_t stream_number = buffer_get_short_le(buf) & 0x007f;
    
    _store_stream_info( stream_number, info, newSVpv("avg_bitrate", 0), newSViv( buffer_get_int_le(buf) ) );
  }
}

void
_parse_metadata_library(Buffer *buf, HV *info, HV *tags)
{
  uint16_t count = buffer_get_short_le(buf);
  
  while ( count-- ) {
    SV *key = NULL;
    SV *value = NULL;
    Buffer utf8_buf;
    
    uint16_t lang_index    = buffer_get_short_le(buf);
    uint16_t stream_number = buffer_get_short_le(buf);
    uint16_t name_len      = buffer_get_short_le(buf);
    uint16_t data_type     = buffer_get_short_le(buf);
    uint32_t data_len      = buffer_get_int_le(buf);
    
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, name_len);
    key = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(key);
    buffer_free(&utf8_buf);
    
    if (data_type == TYPE_UNICODE) {
      buffer_get_utf16le_as_utf8(buf, &utf8_buf, data_len);
      value = newSVpv( buffer_ptr(&utf8_buf), 0 );
      sv_utf8_decode(value);
      buffer_free(&utf8_buf);
    }
    else if (data_type == TYPE_BYTE) {
      // handle picture data
      if ( !strcmp( SvPVX(key), "WM/Picture" ) ) {
        value = _parse_picture(buf);
      }
      else {
        value = newSVpvn( buffer_ptr(buf), data_len );
        buffer_consume(buf, data_len);
      }
    }
    else if (data_type == TYPE_BOOL || data_type == TYPE_WORD) {
      value = newSViv( buffer_get_short_le(buf) );
    }
    else if (data_type == TYPE_DWORD) {
      value = newSViv( buffer_get_int_le(buf) );
    }
    else if (data_type == TYPE_QWORD) {
      value = newSViv( buffer_get_int64_le(buf) );
    }
    else if (data_type == TYPE_GUID) {
      GUID g;
      buffer_get_guid(buf, &g);
      value = newSVpvf(
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        g.l, g.w[0], g.w[1],
        g.b[0], g.b[1], g.b[2], g.b[3],
        g.b[4], g.b[5], g.b[6], g.b[7]
      );
    }
    else {
      PerlIO_printf(PerlIO_stderr(), "Unknown metadata library data type %d\n", data_type);
      buffer_consume(buf, data_len);
    }
    
    if (value != NULL) {
#ifdef DEBUG
      if ( data_type == 0 || data_type == 6 ) {
        DEBUG_TRACE("    %s / type %d / lang_index %d / stream_number %d / %s\n", SvPVX(key), data_type, lang_index, stream_number, SvPVX(value));
      }
      else if ( data_type > 1 ) {
        DEBUG_TRACE("    %s / type %d / lang_index %d / stream_number %d / %d\n", SvPVX(key), data_type, lang_index, stream_number, SvIV(value));
      }
      else {
        DEBUG_TRACE("    %s / type %d / lang_index %d / stream_number %d / <binary>\n", SvPVX(key), lang_index, stream_number, data_type);
      }
#endif
      
      // If stream_number is available, store the data with the stream info
      // XXX: should store lang_index?
      if (stream_number > 0) {
        _store_stream_info( stream_number, info, key, value );
      }
      else {
        _store_tag( tags, key, value );
      }
    }
  }
}

void
_parse_index_parameters(Buffer *buf, HV *info, HV *tags)
{
  uint16_t count;
  
  my_hv_store( info, "index_entry_interval", newSViv( buffer_get_int_le(buf) ) );
  
  count = buffer_get_short_le(buf);
  
  while ( count-- ) {
    uint16_t stream_number = buffer_get_short_le(buf);
    uint16_t index_type    = buffer_get_short_le(buf);
    
    switch (index_type) {
      case 0x0001:
        _store_stream_info( stream_number, info, newSVpv("index_type", 0), newSVpv("Nearest Past Data Packet", 0) );
        break;
      case 0x0002:
        _store_stream_info( stream_number, info, newSVpv("index_type", 0), newSVpv("Nearest Past Media Object", 0) );
        break;
      case 0x0003:
        _store_stream_info( stream_number, info, newSVpv("index_type", 0), newSVpv("Nearest Past Cleanpoint", 0) );
        break;
      default:
        _store_stream_info( stream_number, info, newSVpv("index_type", 0), newSViv(index_type) );
    }
  }
}  

void
_store_stream_info(int stream_number, HV *info, SV *key, SV *value )
{
  AV *streams;
  HV *streaminfo;
  uint8_t found = 0;
  int i = 0;
  
  if ( !my_hv_exists( info, "streams" ) ) {
    // Create
    streams = newAV();
    my_hv_store( info, "streams", newRV_noinc( (SV*)streams ) );
  }
  else {
    SV **entry = my_hv_fetch( info, "streams" );
    if (entry != NULL) {
      streams = (AV *)SvRV(*entry);
    }
    else {
      return;
    }
  }
  
  if (streams != NULL) {
    // Find entry for this stream number
    for (i = 0; av_len(streams) >= 0 && i <= av_len(streams); i++) {
      SV **stream = av_fetch(streams, i, 0);
      if (stream != NULL) {
        SV **sn;
        
        streaminfo = (HV *)SvRV(*stream);        
        sn = my_hv_fetch( streaminfo, "stream_number" );
        if (sn != NULL) {
          if ( SvIV(*sn) == stream_number ) {
            // XXX: if item exists, create array
            my_hv_store_ent( streaminfo, key, value );
            SvREFCNT_dec(key);
          
            found = 1;            
            break;
          }
        }
      }
    }
  
    if ( !found ) {
      // New stream number
      streaminfo = newHV();
    
      my_hv_store( streaminfo, "stream_number", newSViv(stream_number) );
      my_hv_store_ent( streaminfo, key, value );
      SvREFCNT_dec(key);
    
      av_push( streams, newRV_noinc( (SV *)streaminfo ) );
    }
  }
}

void
_store_tag(HV *tags, SV *key, SV *value)
{
  // if key exists, create array
  if ( my_hv_exists_ent( tags, key ) ) {
    SV **entry = my_hv_fetch( tags, SvPVX(key) );
    if (entry != NULL) {
      if ( SvTYPE(SvRV(*entry)) == SVt_PVAV ) {
        av_push( (AV *)SvRV(*entry), value );
      }
      else {
      // A non-array entry, convert to array.
        AV *ref = newAV();
        av_push( ref, newSVsv(*entry) );
        av_push( ref, value );
        my_hv_store_ent( tags, key, newRV_noinc( (SV*)ref ) );
      }
    }
  }
  else {
    my_hv_store_ent( tags, key, value );  
  }
  
  SvREFCNT_dec(key);
}

int
_parse_index_objects(PerlIO *infile, int index_size, uint64_t audio_offset, Buffer *buf, HV *info, HV *tags)
{
  GUID tmp;
  uint64_t size;
  
  while (index_size > 0) {
    // Make sure we have enough data
    if ( !_check_buf(infile, buf, 24, ASF_BLOCK_SIZE) ) {
      return 0;
    }
  
    buffer_get_guid(buf, &tmp);    
    size = buffer_get_int64_le(buf);
  
    if ( !_check_buf(infile, buf, size - 24, ASF_BLOCK_SIZE) ) {
      return 0;
    }
    
    if ( IsEqualGUID(&tmp, &ASF_Index) ) {
      DEBUG_TRACE("Index size %d\n", size);
      _parse_index(buf, audio_offset, info, tags);
    }
    else if ( IsEqualGUID(&tmp, &ASF_Simple_Index) ) {
      DEBUG_TRACE("Skipping Simple_Index size %d\n", size);
      // Simple Index is used for video files only
      buffer_consume(buf, size - 24);
    }
    else {
      // Unhandled GUID
      PerlIO_printf(PerlIO_stderr(), "** Unhandled Index GUID: ");
      print_guid(tmp);
      PerlIO_printf(PerlIO_stderr(), "size: %lu\n", size);
      
      buffer_consume(buf, size - 24);
    }
      
    index_size -= size;
  }
  
  return 1;
}

// XXX: These don't really seem that useful for seeking after all...
void
_parse_index(Buffer *buf, uint64_t audio_offset, HV *info, HV *tags)
{
  AV *specs   = newAV();
  AV *blocks  = newAV();
  uint16_t spec_count;
  uint32_t blocks_count;
  int i;
  
  // Skip index entry time interval, it is read from Index Parameters
  buffer_consume(buf, 4);
  
  spec_count   = buffer_get_short_le(buf);
  blocks_count = buffer_get_int_le(buf);
  
  for (i = 0; i < spec_count; i++) {
    // Add stream number
    av_push( specs, newSViv( buffer_get_short_le(buf) ) );
    // Skip index type, this is already read from Index Parameters
    buffer_consume(buf, 2);
  }
  
  my_hv_store( info, "index_specifiers", newRV_noinc( (SV *)specs ) );
  
  // XXX: if blocks_count > 1 the file is larger than 2^32 bytes and
  // our stored index data is not valid.  This seems unlikely to occur in real life...
  while ( blocks_count-- ) {
    AV *offsets[spec_count];
    uint32_t entry_count = buffer_get_int_le(buf);
    
    for (i = 0; i < spec_count; i++) {
      uint64_t block_pos;
      
      // Init offsets array for each spec_count
      offsets[i] = newAV();
      
      block_pos = buffer_get_int64_le(buf);
      av_push( blocks, newSViv(block_pos) );
    }
    
    my_hv_store( info, "index_blocks", newRV_noinc( (SV *)blocks ) );
    
    while ( entry_count-- ) {      
      for (i = 0; i < spec_count; i++) {
        // These are byte offsets relative to start of the first data packet,
        // so we add audio_offset here.  An additional 50 bytes are added
        // to skip past the top-level Data Object
        av_push( offsets[i], newSViv( audio_offset + buffer_get_int_le(buf) ) );
      }
    }
    
    if (spec_count == 1) {
      my_hv_store( info, "index_offsets", newRV_noinc( (SV *)offsets[0] ) );
    }
    else {
      // Nested arrays, one per spec_count (stream)
      AV *offset_list = newAV();
          
      for (i = 0; i < spec_count; i++) {
        av_push( offset_list, newRV_noinc( (SV *)offsets[i] ) );
      }
      
      my_hv_store( info, "index_offsets", newRV_noinc( (SV *)offset_list ) );
    }
  }
}

void
_parse_content_encryption(Buffer *buf, HV *info, HV *tags)
{
  uint32_t protection_type_len;
  uint32_t key_len;
  uint32_t license_url_len;
  
  // Skip secret data
  buffer_consume(buf, buffer_get_int_le(buf));
  
  protection_type_len = buffer_get_int_le(buf);
  my_hv_store( info, "drm_protection_type", newSVpvn( buffer_ptr(buf), protection_type_len - 1 ) );
  buffer_consume(buf, protection_type_len);
  
  key_len = buffer_get_int_le(buf);
  my_hv_store( info, "drm_key", newSVpvn( buffer_ptr(buf), key_len - 1 ) );
  buffer_consume(buf, key_len);
  
  license_url_len = buffer_get_int_le(buf);
  my_hv_store( info, "drm_license_url", newSVpvn( buffer_ptr(buf), license_url_len - 1 ) );
  buffer_consume(buf, license_url_len);
}

void
_parse_extended_content_encryption(Buffer *buf, HV *info, HV *tags)
{
  uint32_t len = buffer_get_int_le(buf);
  Buffer utf8_buf;
  SV *value;
  unsigned char *tmp_ptr = buffer_ptr(buf);
    
  if ( tmp_ptr[0] == 0xFF && tmp_ptr[1] == 0xFE ) {
    buffer_consume(buf, 2);
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, len - 2);
    value = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(value);
    buffer_free(&utf8_buf);
    
    my_hv_store( info, "drm_data", value );
  }
  else {
    buffer_consume(buf, len);
  }
}

void
_parse_script_command(Buffer *buf, HV *info, HV *tags)
{
  uint16_t command_count;
  uint16_t type_count;
  Buffer utf8_buf;
  AV *types = newAV();
  AV *commands = newAV();
  
  // Skip reserved
  buffer_consume(buf, 16);
  
  command_count = buffer_get_short_le(buf);
  type_count    = buffer_get_short_le(buf);
  
  while ( type_count-- ) {
    SV *value;
    uint16_t len = buffer_get_short_le(buf);
    
    buffer_get_utf16le_as_utf8(buf, &utf8_buf, len * 2);
    value = newSVpv( buffer_ptr(&utf8_buf), 0 );
    sv_utf8_decode(value);
    buffer_free(&utf8_buf);
    
    av_push( types, value );
  }
  
  while ( command_count-- ) {
    HV *command = newHV();
    SV *value;
    
    uint32_t pres_time  = buffer_get_int_le(buf);
    uint16_t type_index = buffer_get_short_le(buf);
    uint16_t name_len   = buffer_get_short_le(buf);
    
    if (name_len) {
      buffer_get_utf16le_as_utf8(buf, &utf8_buf, name_len * 2);
      value = newSVpv( buffer_ptr(&utf8_buf), 0 );
      sv_utf8_decode(value);
      buffer_free(&utf8_buf);
      my_hv_store( command, "command", value );
    }
    
    my_hv_store( command, "time", newSVuv(pres_time) );
    my_hv_store( command, "type", newSVuv(type_index) );
    
    av_push( commands, newRV_noinc( (SV *)command ) );
  }
  
  my_hv_store( info, "script_types", newRV_noinc( (SV *)types ) );
  my_hv_store( info, "script_commands", newRV_noinc( (SV *)commands ) );
}

SV *
_parse_picture(Buffer *buf)
{
  char *tmp_ptr;
  uint16_t mime_len = 2; // to handle double-null
  uint16_t desc_len = 2;
  uint32_t image_len;
  SV *mime;
  SV *desc;
  HV *picture = newHV();
  Buffer utf8_buf;
  
  my_hv_store( picture, "image_type", newSVuv( buffer_get_char(buf) ) );

  image_len = buffer_get_int_le(buf);
  
  // MIME type is a double-null-terminated UTF-16 string
  tmp_ptr = buffer_ptr(buf);
  while ( tmp_ptr[0] != '\0' || tmp_ptr[1] != '\0' ) {
    mime_len += 2;
    tmp_ptr += 2;
  }
  
  buffer_get_utf16le_as_utf8(buf, &utf8_buf, mime_len);
  mime = newSVpv( buffer_ptr(&utf8_buf), 0 );
  sv_utf8_decode(mime);
  my_hv_store( picture, "mime_type", mime );
  buffer_free(&utf8_buf);
  
  // Description is a double-null-terminated UTF-16 string
  tmp_ptr = buffer_ptr(buf);
  while ( tmp_ptr[0] != '\0' || tmp_ptr[1] != '\0' ) {
    desc_len += 2;
    tmp_ptr += 2;
  }
  
  buffer_get_utf16le_as_utf8(buf, &utf8_buf, desc_len);
  desc = newSVpv( buffer_ptr(&utf8_buf), 0 );
  sv_utf8_decode(desc);
  my_hv_store( picture, "description", desc );
  buffer_free(&utf8_buf);
  
  my_hv_store( picture, "image", newSVpvn( buffer_ptr(buf), image_len ) );
  buffer_consume(buf, image_len);
  
  return newRV_noinc( (SV *)picture );
}

// offset is in ms
// Based on some code from Rockbox
int
asf_find_frame(PerlIO *infile, char *file, int offset)
{
  int frame_offset = -1;
  uint32_t audio_offset, max_packet_size, max_bitrate;
  uint64_t data_packets;
  uint32_t packet_num;
  uint8_t count = 0;
  
  // We need to read all tags first to get some data we need to calculate
  HV *info = newHV();
  HV *tags = newHV();
  get_asf_metadata(infile, file, info, tags);
  
  audio_offset    = SvIV( *(my_hv_fetch( info, "audio_offset" )) );
  data_packets    = SvIV( *(my_hv_fetch( info, "data_packets" )) );
  max_packet_size = SvIV( *(my_hv_fetch( info, "max_packet_size" )) );
  max_bitrate     = SvIV( *(my_hv_fetch( info, "max_bitrate" )) );
  
  packet_num = ( ( ((int64_t)offset) * (max_bitrate>>3) ) / max_packet_size / 1000 ) + 1;
  
  if (packet_num > data_packets) {
    packet_num = data_packets;
  }
  
  frame_offset = audio_offset + ( (packet_num - 1) * max_packet_size);
  
  // Double-check above packet, make sure we have the right one
  // with a timestamp within our desired range
  DEBUG_TRACE("Looking for packet with timestamp %d (total packets %d)\n", offset, data_packets);
  
  while ( count < 10 ) {
    int time, duration;
    
    time = _timestamp(infile, frame_offset, &duration);
    
    DEBUG_TRACE("  Timestamp for packet %d at %d: %d, duration: %d\n", packet_num, frame_offset, time, duration);
    
    if (time < 0) {
      DEBUG_TRACE("  Invalid timestamp, giving up\n");
      break;
    }
    
    if ( time + duration >= offset && time <= offset ) {
      DEBUG_TRACE("  Found packet at offset %d\n", frame_offset);
      break;
    }
    else {
      int delta = offset - time;
      DEBUG_TRACE("  Wrong packet, delta: %d\n", delta);
      
      // XXX: too simplistic, but the re-calc code from Rockbox is not correct either
      if (delta > 0) {
        packet_num++;
      }
      else {
        packet_num--;
      }
      
      if (packet_num < 1 || packet_num > data_packets) {
        // Probably we were passed an offset out of bounds for this file
        frame_offset = -1;
        break;
      }
      
      frame_offset = audio_offset + ( (packet_num - 1) * max_packet_size);
      
      count++;
      
      DEBUG_TRACE("  Try #%d packet %d at frame offset %d\n", count, packet_num, frame_offset);
    }
  }
  
  // Don't leak
  SvREFCNT_dec(info);
  SvREFCNT_dec(tags);
  
  return frame_offset;
}

// Return the timestamp of the data packet at offset
int
_timestamp(PerlIO *infile, int offset, int *duration)
{
  Buffer asf_buf;
  int timestamp = -1;
  uint8_t tmp;
  
  PerlIO_seek(infile, offset, SEEK_SET);
  
  buffer_init(&asf_buf, 0);
  
  if ( !_check_buf(infile, &asf_buf, 64, 64) ) {
    goto out;
  }
  
  // Read Error Correction Flags
  tmp = buffer_get_char(&asf_buf);
  
  if (tmp & 0x80) {
    // Skip error correction data
    buffer_consume(&asf_buf, tmp & 0x0f);
    
    // Read Length Type Flags
    tmp = buffer_get_char(&asf_buf);
  }
  else {
    // The byte we already read is Length Type Flags
  }
  
  // Skip Property Flags, Packet Length, Sequence, Padding Length
  buffer_consume( &asf_buf,
    1 + GETLEN2b((tmp >> 1) & 0x03) +
        GETLEN2b((tmp >> 3) & 0x03) +
        GETLEN2b((tmp >> 5) & 0x03)
  );
  
  timestamp = buffer_get_int_le(&asf_buf);
  *duration = buffer_get_short_le(&asf_buf);
  
out:
  buffer_free(&asf_buf);

  return timestamp;
}