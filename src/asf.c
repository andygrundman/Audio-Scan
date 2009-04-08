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

#include "asf.h"

static void
print_guid(GUID guid)
{
  PerlIO_printf(
    PerlIO_stderr(), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x ",
    guid.l, guid.w[0], guid.w[1],
    guid.b[0], guid.b[1], guid.b[2], guid.b[3],
    guid.b[4], guid.b[5], guid.b[6], guid.b[7]
  );
}

static int
get_asf_metadata(char *file, HV *info, HV *tags)
{
  PerlIO *infile;
  
  Buffer asf_buf;
  ASF_Object hdr;
  ASF_Object tmp;
  
  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to audio
  
  int err = 0;
  
  if (!(infile = PerlIO_open(file, "rb"))) {
    PerlIO_printf(PerlIO_stderr(), "Could not open %s for reading\n", file);
    err = -1;
    goto out;
  }
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  buffer_init(&asf_buf, 0);
  
  if ( !_check_buf(infile, &asf_buf, ASF_BLOCK_SIZE, ASF_BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }
  
  buffer_get(&asf_buf, &hdr.ID, 16);
  
  if ( !IsEqualGUID(&hdr.ID, &ASF_Header_Object) ) {
    PerlIO_printf(PerlIO_stderr(), "Invalid ASF header: %s\n", file);
    err = -1;
    goto out;
  }
  
  hdr.size        = buffer_get_int64_le(&asf_buf);
  hdr.num_objects = buffer_get_int_le(&asf_buf);
  hdr.reserved1   = buffer_get_char(&asf_buf);
  hdr.reserved2   = buffer_get_char(&asf_buf);
  
  //PerlIO_printf(PerlIO_stderr(), "header size: %d\n", hdr.size);
  
  if ( hdr.reserved2 != 0x02 ) {
    PerlIO_printf(PerlIO_stderr(), "Invalid ASF header: %s\n", file);
    err = -1;
    goto out;
  }
  
  while (hdr.num_objects) {
    if ( !_check_buf(infile, &asf_buf, 24, ASF_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    buffer_get(&asf_buf, &tmp.ID, 16);
    
    tmp.size = buffer_get_int64_le(&asf_buf);
    
    if ( !_check_buf(infile, &asf_buf, tmp.size - 24, ASF_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    print_guid(tmp.ID);
    PerlIO_printf(PerlIO_stderr(), "size: %lu\n", tmp.size);
    
    if ( IsEqualGUID(&tmp.ID, &ASF_File_Properties) ) {
      PerlIO_printf(PerlIO_stderr(), "Parsing File_Properties\n");
      _parse_file_properties(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Stream_Properties) ) {
      PerlIO_printf(PerlIO_stderr(), "Parsing Stream_Properties\n");
      _parse_stream_properties(&asf_buf, info, tags);
    }
    else if ( IsEqualGUID(&tmp.ID, &ASF_Header_Extension) ) {
      PerlIO_printf(PerlIO_stderr(), "Parsing Header_Extension\n");
      if ( !_parse_header_extension(&asf_buf, tmp.size, info, tags) ) {
        PerlIO_printf(PerlIO_stderr(), "Invalid ASF file: %s (invalid header extension object)\n", file);
        err = -1;
        goto out;
      }
    }
    else {
      // Unhandled GUID
      buffer_consume(&asf_buf, tmp.size - 24);
    }
    
    hdr.num_objects--;
  }
  
out:
  if (infile) PerlIO_close(infile);

  buffer_free(&asf_buf);

  if (err) return err;

  return 0;
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
  
  buffer_get(buf, &file_id, 16);
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
  
  broadcast = flags & 0x0001 ? 1 : 0;
  seekable  = flags & 0x0002 ? 1 : 0;
  
  if ( !broadcast ) {
    creation_date = (creation_date - 116444736000000000ULL) / 10000000;
    play_duration /= 10000000;
    send_duration /= 10000000;
    
    my_hv_store( info, "file_size", newSViv(file_size) );
    my_hv_store( info, "creation_date", newSViv(creation_date) );
    my_hv_store( info, "data_packets", newSViv(data_packets) );
    my_hv_store( info, "play_duration", newSViv(play_duration) );
    my_hv_store( info, "send_duration", newSViv(send_duration) );
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
  
  buffer_get(buf, &stream_type, 16);
  buffer_get(buf, &ec_type, 16);
  time_offset = buffer_get_int64_le(buf);
  type_data_len = buffer_get_int_le(buf);
  ec_data_len   = buffer_get_int_le(buf);
  flags         = buffer_get_short_le(buf);
  stream_number = flags & 0x007f;
  
  // skip reserved bytes
  buffer_consume(buf, 4);
  
  // skip type-specific data
  buffer_consume(buf, type_data_len);
  
  // skip error-correction data
  buffer_consume(buf, ec_data_len);
  
  if ( IsEqualGUID(&stream_type, &ASF_Audio_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Audio_Media", 0) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Video_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Video_Media", 0) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Command_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Command_Media", 0) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_JFIF_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_JFIF_Media", 0) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Degradable_JPEG_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Degradable_JPEG_Media", 0) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_File_Transfer_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_File_Transfer_Media", 0) );
  }
  else if ( IsEqualGUID(&stream_type, &ASF_Binary_Media) ) {
    _store_stream_info( stream_number, info, newSVpv("stream_type", 0), newSVpv("ASF_Binary_Media", 0) );
  }
  
  if ( IsEqualGUID(&ec_type, &ASF_No_Error_Correction) ) {
    _store_stream_info( stream_number, info, newSVpv("error_correction_type", 0), newSVpv("ASF_No_Error_Correction", 0) );
  }
  else if ( IsEqualGUID(&ec_type, &ASF_Audio_Spread) ) {
    _store_stream_info( stream_number, info, newSVpv("error_correction_type", 0), newSVpv("ASF_Audio_Spread", 0) );
  }
  
  _store_stream_info( stream_number, info, newSVpv("time_offset", 0), newSViv(time_offset) );
  _store_stream_info( stream_number, info, newSVpv("encrypted", 0), newSViv( flags & 0x8000 ) );
}

int
_parse_header_extension(Buffer *buf, uint64_t len, HV *info, HV *tags)
{
  uint32_t ext_size;
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
  
  while (ext_size > 0) {    
    buffer_get(buf, &hdr, 16);
    hdr_size = buffer_get_int64_le(buf);
    ext_size -= hdr_size;
    
    PerlIO_printf(PerlIO_stderr(), "  extended header: ");
    print_guid(hdr);
    PerlIO_printf(PerlIO_stderr(), "size: %lu\n", hdr_size);
    
    if ( IsEqualGUID(&hdr, &ASF_Compatibility) ) {
      // reserved for future use, just ignore
      buffer_consume(buf, 2);
    }
    else if ( IsEqualGUID(&hdr, &ASF_Metadata) ) {
      PerlIO_printf(PerlIO_stderr(), "Parsing Metadata\n");
      if ( !_parse_metadata(buf, info, tags) ) {
        return 0;
      }
    }
    else {
      // Unhandled
      buffer_consume(buf, hdr_size - 24);
    }
  }
  
  return 1;
}

int
_parse_metadata(Buffer *buf, HV *info, HV *tags)
{
  uint16_t records;
  
  records = buffer_get_short_le(buf);
  
  while (records) {
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
    
    PerlIO_printf(PerlIO_stderr(), "name_len: %d, data_type: %d, data_len: %d\n", name_len, data_type, data_len);
    
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
      // XXX: undocumented
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
      PerlIO_printf(PerlIO_stderr(), "Unknown data type %d\n", data_type);
      buffer_consume(buf, data_len);
    }
    
    if (value != NULL) {
      // If stream_number is available, store the data with the stream info
      if (stream_number > 0) {
        _store_stream_info( stream_number, info, key, value );
      }
      else {
        my_hv_store_ent( info, key, value );
      }
    }
    
    records--;
  }
  
  return 1;
}

void
_store_stream_info(int stream_number, HV *info, SV *key, SV *value )
{
  AV *streams;
  HV *streaminfo;
  
  if ( !my_hv_exists( info, "streams" ) ) {
    // Create
    streams = newAV();
    streaminfo = newHV();
    
    my_hv_store( streaminfo, "stream_number", newSViv(stream_number) );
    my_hv_store_ent( streaminfo, key, value );
    SvREFCNT_dec(key);
    
    av_push( streams, (SV *)streaminfo );
    my_hv_store( info, "streams", newRV_noinc( (SV*)streams ) );
  }
  else {
    // Find entry for this stream number
    SV **entry = my_hv_fetch( info, "streams" );
    if (entry != NULL) {
      streams = (AV *)SvRV(*entry);
      int i = 0;
      
      for (i = 0; i <= av_len(streams); i++) {
        SV **stream = av_fetch(streams, i, 0);
        if (stream != NULL) {
          streaminfo = (HV *)*stream;
          SV **sn = my_hv_fetch( streaminfo, "stream_number" );
          if (sn != NULL) {
            if ( SvIV(*sn) == stream_number ) {
              my_hv_store_ent( streaminfo, key, value );
              SvREFCNT_dec(key);
              
              break;
            }
          }
        }
      }
    }
  }
}