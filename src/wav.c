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

#include "wav.h"

static int
get_wav_metadata(PerlIO *infile, char *file, HV *info, HV *tags)
{
  Buffer buf;
  off_t file_size;
  int err = 0;
  uint32_t chunk_size;
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  buffer_init(&buf, 0);
  
  if ( !_check_buf(infile, &buf, 12, WAV_BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }
  
  if ( !strncmp( (char *)buffer_ptr(&buf), "RIFF", 4 ) ) {
    // We've got a RIFF file
    buffer_consume(&buf, 4);
  
    chunk_size = buffer_get_int_le(&buf);
    
    // Check format
    if ( strncmp( (char *)buffer_ptr(&buf), "WAVE", 4 ) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid WAV file: missing WAVE header: %s\n", file);
      err = -1;
      goto out;
    }
    
    buffer_consume(&buf, 4);
    
    my_hv_store( info, "file_size", newSVuv(file_size) );
    
    _parse_wav(infile, &buf, file, file_size, info, tags);
  }
  else if ( !strncmp( (char *)buffer_ptr(&buf), "FORM", 4 ) ) {
    // We've got an AIFF file
    char *bptr;
    
    buffer_consume(&buf, 4);
    
    chunk_size = buffer_get_int(&buf);
    
    // Check format
    bptr = buffer_ptr(&buf);
    if ( bptr[0] == 'A' && bptr[1] == 'I' && bptr[2] == 'F' && (bptr[3] == 'F' || bptr[3] == 'C') ) {
      buffer_consume(&buf, 4);

      my_hv_store( info, "file_size", newSVuv(file_size) );

      _parse_aiff(infile, &buf, file, file_size, info, tags);
    }
    else {
      PerlIO_printf(PerlIO_stderr(), "Invalid AIFF file: missing AIFF header: %s\n", file);
      err = -1;
      goto out;
    }
  }
  else {
    PerlIO_printf(PerlIO_stderr(), "Invalid WAV file: missing RIFF header: %s\n", file);
    err = -1;
    goto out;
  }
  
out:
  buffer_free(&buf);

  if (err) return err;

  return 0;
}

void
_parse_wav(PerlIO *infile, Buffer *buf, char *file, uint32_t file_size, HV *info, HV *tags)
{
  uint32_t offset = 12;
  
  while ( offset < file_size - 8 ) {
    char chunk_id[5];
    uint32_t chunk_size;
    
    // Verify we have at least 8 bytes
    if ( !_check_buf(infile, buf, 8, WAV_BLOCK_SIZE) ) {
      return;
    }
    
    strncpy( chunk_id, (char *)buffer_ptr(buf), 4 );
    chunk_id[4] = '\0';
    buffer_consume(buf, 4);
    
    chunk_size = buffer_get_int_le(buf);
    
    // Adjust for padding
    if ( chunk_size % 2 ) {
      chunk_size++;
    }
    
    offset += 8;
    
    DEBUG_TRACE("%s size %d\n", chunk_id, chunk_size);
    
    // Seek past data, everything else we parse
    // XXX: Are there other large chunks we should ignore?
    if ( !strcmp( chunk_id, "data" ) ) {
      SV **bitrate;
      
      my_hv_store( info, "audio_offset", newSVuv(offset) );
      
      // Calculate duration, unless we already know it (i.e. from 'fact')
      if ( !my_hv_fetch( info, "song_length_ms" ) ) {
        bitrate = my_hv_fetch( info, "bitrate" );
        if (bitrate != NULL) {
          my_hv_store( info, "song_length_ms", newSVuv( (chunk_size / (SvIV(*bitrate) / 8.)) * 1000 ) );
        }
      }
      
      // sanity check size, this is inside the data chunk code
      // to support setting audio_offset even when the data size is wrong
      if (chunk_size > file_size - offset) {
        DEBUG_TRACE("data size > file_size, skipping\n");
        return;
      }
      
      // Seek past data if there are more chunks after it
      if ( file_size > offset + chunk_size ) {
        PerlIO_seek(infile, offset + chunk_size, SEEK_SET);
      }
      
      buffer_clear(buf);
    }
    else if ( !strcmp( chunk_id, "id3 " ) || !strcmp( chunk_id, "ID3 " ) || !strcmp( chunk_id, "ID32" ) ) {
      // Read header to verify version
      unsigned char *bptr = buffer_ptr(buf);
      
      if (
        (bptr[0] == 'I' && bptr[1] == 'D' && bptr[2] == '3') &&
        bptr[3] < 0xff && bptr[4] < 0xff &&
        bptr[6] < 0x80 && bptr[7] < 0x80 && bptr[8] < 0x80 && bptr[9] < 0x80
      ) {
        my_hv_store( info, "id3_version", newSVpvf( "ID3v2.%d.%d", bptr[3], bptr[4] ) );
        
        // Start parsing ID3 from offset
        parse_id3(infile, file, info, tags, offset);
      }
      
      // Seek past ID3 and clear buffer
      PerlIO_seek(infile, offset + chunk_size, SEEK_SET);
      buffer_clear(buf);
    }
    else {
      // sanity check size
      if (chunk_size > file_size - offset) {
        DEBUG_TRACE("chunk_size > file_size, skipping\n");
        return;
      }
      
      // Make sure we have enough data
      if ( !_check_buf(infile, buf, chunk_size, WAV_BLOCK_SIZE) ) {
        return;
      }
      
      if ( !strcmp( chunk_id, "fmt " ) ) {
        _parse_wav_fmt(buf, chunk_size, info);
      }
      else if ( !strcmp( chunk_id, "LIST" ) ) {
        _parse_wav_list(buf, chunk_size, tags);
      }
      else if ( !strcmp( chunk_id, "PEAK" ) ) {
        _parse_wav_peak(buf, chunk_size, info, 0);
      }
      else if ( !strcmp( chunk_id, "fact" ) ) {
        // A 4-byte fact chunk in a non-PCM wav is the number of samples
        // Use it to calculate duration
        if ( chunk_size == 4 ) {
          uint32_t num_samples = buffer_get_int_le(buf);
          SV **samplerate = my_hv_fetch( info, "samplerate" );
          if (samplerate != NULL) {
            my_hv_store( info, "song_length_ms", newSVuv( (num_samples * 1000) / SvIV(*samplerate) ) );
          }
        }
        else {
          // Unknown, skip it
          buffer_consume(buf, chunk_size);
        }
      }
      else {
        PerlIO_printf(PerlIO_stderr(), "Unhandled WAV chunk %s size %d (skipped)\n", chunk_id, chunk_size);
        buffer_consume(buf, chunk_size);
      }
    }
    
    offset += chunk_size;
  }
}

void
_parse_wav_fmt(Buffer *buf, uint32_t chunk_size, HV *info)
{
  uint16_t format = buffer_get_short_le(buf);
  
  my_hv_store( info, "format", newSVuv(format) );
  my_hv_store( info, "channels", newSVuv( buffer_get_short_le(buf) ) );
  my_hv_store( info, "samplerate", newSVuv( buffer_get_int_le(buf) ) );
  my_hv_store( info, "bitrate", newSVuv( buffer_get_int_le(buf) * 8 ) );
  my_hv_store( info, "block_align", newSVuv( buffer_get_short_le(buf) ) );
  my_hv_store( info, "bits_per_sample", newSVuv( buffer_get_short_le(buf) ) );
  
  if ( chunk_size > 16 ) {
    uint16_t extra_len = buffer_get_short_le(buf);
    
    // Bug 14462, a WAV file with only an 18-byte fmt chunk should ignore extra_len bytes
    if (extra_len && chunk_size > 18) {
      DEBUG_TRACE(" skipping extra_len bytes in fmt: %d\n", extra_len);
      buffer_consume(buf, extra_len);
    }
  }
}

void
_parse_wav_list(Buffer *buf, uint32_t chunk_size, HV *tags)
{
  char type_id[5];
  uint32_t pos = 4;
  
  strncpy( type_id, (char *)buffer_ptr(buf), 4 );
  type_id[4] = '\0';
  buffer_consume(buf, 4);
  
  DEBUG_TRACE("  LIST type %s\n", type_id);
  
  if ( !strcmp( type_id, "adtl" ) ) {
    // XXX need test file
    PerlIO_printf(PerlIO_stderr(), "Unhandled LIST type adtl\n");
    buffer_consume(buf, chunk_size - 4);
  }
  else if ( !strcmp( type_id, "INFO" ) ) {
    while ( pos < chunk_size ) {
      uint32_t len;
      SV *key;
      SV *value;
      
      key = newSVpvn( buffer_ptr(buf), 4 );
      buffer_consume(buf, 4);
      pos += 4;
      
      len = buffer_get_int_le(buf);
      
      // Bug 12250, apparently some WAV files don't use the padding byte
      // so we can't read them.
      if ( len > chunk_size - pos ) {
        PerlIO_printf(PerlIO_stderr(), "Invalid data in WAV LIST INFO chunk\n");
        break;
      }
          
      value = newSVpvn( buffer_ptr(buf), len );
      buffer_consume(buf, len);
      pos += 4 + len;
      
      DEBUG_TRACE("    %s / %s\n", SvPVX(key), SvPVX(value));
      
      my_hv_store_ent( tags, key, value );
      SvREFCNT_dec(key);
      
      // Handle padding
      if ( len % 2 ) {
        buffer_consume(buf, 1);
        pos++;
      }
    }
  }
  else {
    PerlIO_printf(PerlIO_stderr(), "Unhandled LIST type %s\n", type_id);
    buffer_consume(buf, chunk_size - 4);
  }
}

void
_parse_wav_peak(Buffer *buf, uint32_t chunk_size, HV *info, uint8_t big_endian)
{
  uint16_t channels  = 0;
  AV *peaklist = newAV();
  
  SV **entry = my_hv_fetch( info, "channels" );
  if ( entry != NULL ) {
    channels = SvIV(*entry);
  }
  
  // Skip version/timestamp
  buffer_consume(buf, 8);
  
  while ( channels-- ) {
    HV *peak = newHV();
    
    my_hv_store( peak, "value", newSVnv( big_endian ? buffer_get_float32(buf) : buffer_get_float32_le(buf) ) );
    my_hv_store( peak, "position", newSVuv( big_endian ? buffer_get_int(buf) : buffer_get_int_le(buf) ) );
    
    av_push( peaklist, newRV_noinc( (SV *)peak) );
  }
  
  my_hv_store( info, "peak", newRV_noinc( (SV *)peaklist ) );
}

void
_parse_aiff(PerlIO *infile, Buffer *buf, char *file, uint32_t file_size, HV *info, HV *tags)
{
  uint32_t offset = 12;
  
  while ( offset < file_size - 8 ) {
    char chunk_id[5];
    int chunk_size;
    
    // Verify we have at least 8 bytes
    if ( !_check_buf(infile, buf, 8, WAV_BLOCK_SIZE) ) {
      return;
    }
    
    strncpy( chunk_id, (char *)buffer_ptr(buf), 4 );
    chunk_id[4] = '\0';
    buffer_consume(buf, 4);
    
    chunk_size = buffer_get_int(buf);
    
    // Adjust for padding
    if ( chunk_size % 2 ) {
      chunk_size++;
    }
    
    offset += 8;
    
    DEBUG_TRACE("%s size %d\n", chunk_id, chunk_size);
    
    // Seek past SSND, everything else we parse
    // XXX: Are there other large chunks we should ignore?
    if ( !strcmp( chunk_id, "SSND" ) ) {
      my_hv_store( info, "audio_offset", newSVuv(offset) );

      // Seek past data if there are more chunks after it
      if ( file_size > offset + chunk_size ) {
        PerlIO_seek(infile, offset + chunk_size, SEEK_SET);
      }
      
      buffer_clear(buf);
    }
    else if ( !strcmp( chunk_id, "id3 " ) || !strcmp( chunk_id, "ID3 " ) || !strcmp( chunk_id, "ID32" ) ) {
      // Read header to verify version
      unsigned char *bptr = buffer_ptr(buf);
      
      if (
        (bptr[0] == 'I' && bptr[1] == 'D' && bptr[2] == '3') &&
        bptr[3] < 0xff && bptr[4] < 0xff &&
        bptr[6] < 0x80 && bptr[7] < 0x80 && bptr[8] < 0x80 && bptr[9] < 0x80
      ) {
        my_hv_store( info, "id3_version", newSVpvf( "ID3v2.%d.%d", bptr[3], bptr[4] ) );
        
        // Start parsing ID3 from offset
        parse_id3(infile, file, info, tags, offset);
      }
      
      // Seen ID3 chunks with the chunk size in little-endian instead of big-endian
      if (chunk_size < 0 || offset + chunk_size > file_size) {
        break;
      }
      
      // Seek past ID3 and clear buffer
      DEBUG_TRACE("Seeking past ID3 to %d\n", offset + chunk_size);
      PerlIO_seek(infile, offset + chunk_size, SEEK_SET);
      buffer_clear(buf);
    }
    else {
      // Make sure we have enough data
      if ( !_check_buf(infile, buf, chunk_size, WAV_BLOCK_SIZE) ) {
        return;
      }
      
      if ( !strcmp( chunk_id, "COMM" ) ) {
        _parse_aiff_comm(buf, chunk_size, info);
      }
      else if ( !strcmp( chunk_id, "PEAK" ) ) {
        _parse_wav_peak(buf, chunk_size, info, 1);
      }
      else {
        PerlIO_printf(PerlIO_stderr(), "Unhandled AIFF chunk %s size %d (skipped)\n", chunk_id, chunk_size);
        buffer_consume(buf, chunk_size);
      }
    }
    
    offset += chunk_size;
  }
}

void
_parse_aiff_comm(Buffer *buf, uint32_t chunk_size, HV *info)
{
  uint16_t channels = buffer_get_short(buf);
  uint32_t frames = buffer_get_int(buf);
  uint16_t bits_per_sample = buffer_get_short(buf);
  double samplerate = buffer_get_ieee_float(buf);
  
  my_hv_store( info, "channels", newSVuv(channels) );
  my_hv_store( info, "bits_per_sample", newSVuv(bits_per_sample) );
  my_hv_store( info, "samplerate", newSVuv(samplerate) );
  
  my_hv_store( info, "bitrate", newSVuv( samplerate * channels * bits_per_sample ) );
  my_hv_store( info, "song_length_ms", newSVuv( ((frames * 1.0) / samplerate) * 1000 ) );
  my_hv_store( info, "block_align", newSVuv( channels * bits_per_sample / 8 ) );
  
  if (chunk_size > 18) {
    // AIFC extra data
    my_hv_store( info, "compression_type", newSVpvn( buffer_ptr(buf), 4 ) );
    buffer_consume(buf, 4);
    
    my_hv_store( info, "compression_name", newSVpvn( buffer_ptr(buf), chunk_size - 22 ) );
    buffer_consume(buf, chunk_size - 22);
  }
}
