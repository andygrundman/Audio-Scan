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

#include "ogg.h"

static int
get_ogg_metadata(PerlIO *infile, char *file, HV *info, HV *tags)
{
  Buffer ogg_buf, vorbis_buf;
  unsigned char *bptr;
  unsigned int buf_size;

  unsigned int id3_size = 0; // size of leading ID3 data

  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to audio
  
  unsigned char ogghdr[28];
  char header_type;
  int serialno;
  int pagenum;
  char num_segments;
  int pagelen;
  int page = 0;
  int packets = 0;
  int streams = 0;
  
  unsigned char vorbishdr[23];
  unsigned char channels;
  unsigned int blocksize_0 = 0;
  unsigned int avg_buf_size;
  unsigned int samplerate = 0;
  unsigned int bitrate_nominal = 0;
  uint64_t granule_pos = 0;
  
  unsigned char vorbis_type = 0;

  int i;
  int err = 0;
  
  buffer_init(&ogg_buf, OGG_BLOCK_SIZE);
  buffer_init(&vorbis_buf, 0);
  
  file_size = _file_size(infile);
  
  if ( !_check_buf(infile, &ogg_buf, 10, OGG_BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }

  // Skip ID3 tags if any
  bptr = (unsigned char *)buffer_ptr(&ogg_buf);
  if (
    (bptr[0] == 'I' && bptr[1] == 'D' && bptr[2] == '3') &&
    bptr[3] < 0xff && bptr[4] < 0xff &&
    bptr[6] < 0x80 && bptr[7] < 0x80 && bptr[8] < 0x80 && bptr[9] < 0x80
  ) {
    /* found an ID3 header... */
    id3_size = 10 + (bptr[6]<<21) + (bptr[7]<<14) + (bptr[8]<<7) + bptr[9];

    if (bptr[5] & 0x10) {
      // footer present
      id3_size += 10;
    }
    
    buffer_clear(&ogg_buf);
    
    audio_offset += id3_size;
    
    DEBUG_TRACE("Skipping ID3v2 tag of size %d\n", id3_size);

    PerlIO_seek(infile, id3_size, SEEK_SET);
  }
  
  while (1) {
    // Grab 28-byte Ogg header
    if ( !_check_buf(infile, &ogg_buf, 28, OGG_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    buffer_get(&ogg_buf, ogghdr, 28);
    
    audio_offset += 28;
    
    // check that the first four bytes are 'OggS'
    if ( ogghdr[0] != 'O' || ogghdr[1] != 'g' || ogghdr[2] != 'g' || ogghdr[3] != 'S' ) {
      PerlIO_printf(PerlIO_stderr(), "Not an Ogg file (bad OggS header): %s\n", file);
      goto out;
    }
  
    // Header type flag
    header_type = ogghdr[5];
    
    // Absolute granule position, used to find the first audio page
    bptr = ogghdr + 6;
    granule_pos = (uint64_t)CONVERT_INT32LE(bptr);
    bptr += 4;
    granule_pos |= (uint64_t)CONVERT_INT32LE(bptr) << 32;
    
    // Stream serial number
    serialno = CONVERT_INT32LE((ogghdr+14));
    
    // Count start-of-stream pages
    if ( header_type & 0x02 ) {
      streams++;
    }
    
    // Keep track of packet count
    if ( !(header_type & 0x01) ) {
      packets++;
    }
    
    // stop processing if we reach the 3rd packet and have no data
    if (packets > 2 * streams && !buffer_len(&vorbis_buf) ) {
      break;
    }
    
    // Page seq number
    pagenum = CONVERT_INT32LE((ogghdr+18));
    
    if (page >= 0 && page == pagenum) {
      page++;
    }
    else {
      page = -1;
      PerlIO_printf(PerlIO_stderr(), "Missing page(s) in Ogg file: %s\n", file);
    }
    
    DEBUG_TRACE("OggS page %d / packet %d at %d\n", pagenum, packets, (int)(audio_offset - 28));
    DEBUG_TRACE("  granule_pos: %llu\n", granule_pos);
    
    // If the granule_pos > 0, we have reached the end of headers and
    // this is the first audio page
    if (granule_pos > 0 && granule_pos != -1) {
      _parse_vorbis_comments(&vorbis_buf, tags, 1);

      DEBUG_TRACE("  parsed vorbis comments\n");

      buffer_clear(&vorbis_buf);
      
      break;
    }
    
    // Number of page segments
    num_segments = ogghdr[26];
    
    // Calculate total page size
    pagelen = ogghdr[27];
    if (num_segments > 1) {
      int i;
      
      if ( !_check_buf(infile, &ogg_buf, num_segments, OGG_BLOCK_SIZE) ) {
        err = -1;
        goto out;
      }
      
      for( i = 0; i < num_segments - 1; i++ ) {
        u_char x;
        x = buffer_get_char(&ogg_buf);
        pagelen += x;
      }

      audio_offset += num_segments - 1;
    }
    
    if ( !_check_buf(infile, &ogg_buf, pagelen, OGG_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
  
    // Still don't have enough data, must have reached the end of the file
    if ( buffer_len(&ogg_buf) < pagelen ) {
      PerlIO_printf(PerlIO_stderr(), "Premature end of file: %s\n", file);
    
      err = -1;
      goto out;
    }
    
    audio_offset += pagelen;

    // Copy page into vorbis buffer
    buffer_append( &vorbis_buf, buffer_ptr(&ogg_buf), pagelen );
    DEBUG_TRACE("  Read %d into vorbis buffer\n", pagelen);
    
    // Process vorbis packet
    if ( !vorbis_type ) {
      vorbis_type = buffer_get_char(&vorbis_buf);
      // Verify 'vorbis' string
      if ( strncmp( buffer_ptr(&vorbis_buf), "vorbis", 6 ) ) {
        PerlIO_printf(PerlIO_stderr(), "Not a Vorbis file (bad vorbis header): %s\n", file);
        goto out;
      }
      buffer_consume( &vorbis_buf, 6 );
      
      DEBUG_TRACE("  Found vorbis packet type %d\n", vorbis_type);
    }
      
    if (vorbis_type == 1) {
      // Parse info
      // Grab 23-byte Vorbis header
      if ( buffer_len(&vorbis_buf) < 23 ) {
        PerlIO_printf(PerlIO_stderr(), "Not a Vorbis file (bad vorbis header): %s\n", file);
        goto out;
      }

      buffer_get(&vorbis_buf, vorbishdr, 23);

      my_hv_store( info, "version", newSViv( CONVERT_INT32LE(vorbishdr) ) );

      channels = vorbishdr[4];
      my_hv_store( info, "channels", newSViv(channels) );
      my_hv_store( info, "stereo", newSViv( channels == 2 ? 1 : 0 ) );

      samplerate = CONVERT_INT32LE((vorbishdr+5));
      my_hv_store( info, "samplerate", newSViv(samplerate) );
      my_hv_store( info, "bitrate_upper", newSViv( CONVERT_INT32LE((vorbishdr+9)) ) );

      bitrate_nominal = CONVERT_INT32LE((vorbishdr+13));
      my_hv_store( info, "bitrate_nominal", newSViv(bitrate_nominal) );
      my_hv_store( info, "bitrate_lower", newSViv( CONVERT_INT32LE((vorbishdr+17)) ) );

      blocksize_0 = 2 << ((vorbishdr[21] & 0xF0) >> 4);
      my_hv_store( info, "blocksize_0", newSViv( blocksize_0 ) );
      my_hv_store( info, "blocksize_1", newSViv( 2 << (vorbishdr[21] & 0x0F) ) );

      DEBUG_TRACE("  parsed vorbis info header\n");

      buffer_clear(&vorbis_buf);
      vorbis_type = 0;
    }
    
    // Skip rest of this page
    buffer_consume( &ogg_buf, pagelen );
  }
  
  buffer_clear(&ogg_buf);
  
  // audio_offset is 28 less because we read the Ogg header
  // from the first packet past the comments
  my_hv_store( info, "audio_offset", newSViv(audio_offset - 28) );
  
  // calculate average bitrate and duration
  avg_buf_size = blocksize_0 * 2;
  if ( file_size > avg_buf_size ) {
    DEBUG_TRACE("Seeking to %d to calculate bitrate/duration\n", (int)(file_size - avg_buf_size));
    PerlIO_seek(infile, file_size - avg_buf_size, SEEK_SET);
  }
  else {
    DEBUG_TRACE("Seeking to %d to calculate bitrate/duration\n", (int)(audio_offset - 28));
    PerlIO_seek(infile, audio_offset - 28, SEEK_SET);
  }

  if ( PerlIO_read(infile, buffer_append_space(&ogg_buf, avg_buf_size), avg_buf_size) == 0 ) {
    if ( PerlIO_error(infile) ) {
      PerlIO_printf(PerlIO_stderr(), "Error reading: %s\n", strerror(errno));
    }
    else {
      PerlIO_printf(PerlIO_stderr(), "File too small. Probably corrupted.\n");
    }

    err = -1;
    goto out;
  }

  // Find sync
  bptr = (unsigned char *)buffer_ptr(&ogg_buf);
  buf_size = buffer_len(&ogg_buf);
  while (
    buf_size >= 14
    && (bptr[0] != 'O' || bptr[1] != 'g' || bptr[2] != 'g' || bptr[3] != 'S')
  ) {
    bptr++;
    buf_size--;

    if ( buf_size < 14 ) {
      // Give up, use less accurate bitrate for length
      DEBUG_TRACE("buf_size %d, using less accurate bitrate for length\n", buf_size);
      
      my_hv_store( info, "song_length_ms", newSVpvf( "%d", (int)((file_size * 8) / bitrate_nominal) * 1000) );
      my_hv_store( info, "bitrate_average", newSViv(bitrate_nominal) );

      goto out;
    }
  }
  bptr += 6;

  // Get absolute granule value
  granule_pos = (uint64_t)CONVERT_INT32LE(bptr);
  bptr += 4;
  granule_pos |= (uint64_t)CONVERT_INT32LE(bptr) << 32;

  if ( granule_pos && samplerate ) {
    int length = (int)((granule_pos * 1.0 / samplerate) * 1000);
    my_hv_store( info, "song_length_ms", newSVuv(length) );
    my_hv_store( info, "bitrate_average", newSVuv( _bitrate(file_size, length) ) );
    
    DEBUG_TRACE("Using granule_pos/samplerate to calculate bitrate/duration\n");
  }
  else {
    // Use nominal bitrate
    my_hv_store( info, "song_length_ms", newSVpvf( "%d", (int)((file_size * 8) / bitrate_nominal) * 1000) );
    my_hv_store( info, "bitrate_average", newSVuv(bitrate_nominal) );
    
    DEBUG_TRACE("Using nominal bitrate for average\n");
  }
  
  my_hv_store( info, "file_size", newSVuv(file_size) );
  
out:
  buffer_free(&ogg_buf);
  buffer_free(&vorbis_buf);

  if (err) return err;

  return 0;
}

void
_parse_vorbis_comments(Buffer *vorbis_buf, HV *tags, int has_framing)
{
  unsigned int len;
  unsigned int num_comments;
  char *tmp;
  char *bptr;
  SV *vendor;
  
  // Vendor string
  len = buffer_get_int_le(vorbis_buf);
  vendor = newSVpvn( buffer_ptr(vorbis_buf), len );
  sv_utf8_decode(vendor);
  my_hv_store( tags, "VENDOR", vendor );
  buffer_consume(vorbis_buf, len);
  
  // Number of comments
  num_comments = buffer_get_int_le(vorbis_buf);
  
  while (num_comments--) {
    len = buffer_get_int_le(vorbis_buf);
    
    bptr = buffer_ptr(vorbis_buf);
    
    if (
#ifdef _MSC_VER
      !strnicmp(bptr, "COVERART=", 9)
#else
      !strncasecmp(bptr, "COVERART=", 9)
#endif
      &&
      _env_true("AUDIO_SCAN_NO_ARTWORK")
    ) {
      my_hv_store_ent( tags, newSVpvn("COVERART", 8), newSVuv(len - 9) );
      
      buffer_consume(vorbis_buf, len);
    }
    else {    
      New(0, tmp, (int)len + 1, char);
      buffer_get(vorbis_buf, tmp, len);
      tmp[len] = '\0';
    
      _split_vorbis_comment( tmp, tags );
    
      Safefree(tmp);
    }
  }
  
  if (has_framing) {
    // Skip framing byte (Ogg only)
    buffer_consume(vorbis_buf, 1);
  }
}

static int
ogg_find_frame(PerlIO *infile, char *file, int offset)
{
  Buffer ogg_buf;
  unsigned char *bptr;
  unsigned int buf_size;
  int frame_offset = -1;
  
  PerlIO_seek(infile, offset, SEEK_SET);
  
  buffer_init(&ogg_buf, OGG_BLOCK_SIZE);
  
  if ( !_check_buf(infile, &ogg_buf, 512, OGG_BLOCK_SIZE) ) {
    goto out;
  }
  
  bptr = (unsigned char *)buffer_ptr(&ogg_buf);
  buf_size = buffer_len(&ogg_buf);
  
  while (
    buf_size >= 4
    && (bptr[0] != 'O' || bptr[1] != 'g' || bptr[2] != 'g' || bptr[3] != 'S')
  ) {
    bptr++;
    buf_size--;
  }
  
  if (buf_size >= 4) {
    frame_offset = offset + OGG_BLOCK_SIZE - buf_size;
  }

out:
  buffer_free(&ogg_buf);

  return frame_offset;
}
