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

#include "flac.h"

/* frame header size (16 bytes) + 4608 stereo 16-bit samples (higher than 4608 is possible, but not done) */
#define FLAC_FRAME_MAX_BLOCK 18448
#define FLAC_HEADER_LEN 16

int
get_flac_metadata(PerlIO *infile, char *file, HV *info, HV *tags)
{
  flacinfo *flac = _flac_parse(infile, file, info, tags, 0);
  
  Safefree(flac);
  
  return 0;
}

flacinfo *
_flac_parse(PerlIO *infile, char *file, HV *info, HV *tags, uint8_t seeking)
{
  int err = 0;
  int done = 0;
  unsigned char *bptr;
  unsigned int id3_size = 0;
  uint32_t song_length_ms;
  
  flacinfo *flac;
  Newz(0, flac, sizeof(flacinfo), flacinfo);
  Newz(0, flac->buf, sizeof(Buffer), Buffer);
  
  flac->infile         = infile;
  flac->file           = file;
  flac->info           = info;
  flac->tags           = tags;
  flac->audio_offset   = 0;
  flac->seeking        = seeking ? 1 : 0;
  flac->num_seekpoints = 0;
  
  buffer_init(flac->buf, FLAC_BLOCK_SIZE);
  
  PerlIO_seek(infile, 0, SEEK_END);
  flac->file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  if ( !_check_buf(infile, flac->buf, 10, FLAC_BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }
  
  // Check for ID3 tags
  bptr = buffer_ptr(flac->buf);
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
    
    DEBUG_TRACE("Found ID3v2 tag of size %d\n", id3_size);
    
    my_hv_store( info, "id3_version", newSVpvf( "ID3v2.%d.%d", bptr[3], bptr[4] ) );
    
    flac->audio_offset += id3_size;
            
    // seek past ID3, we will parse it later
    if ( id3_size < buffer_len(flac->buf) ) {
      buffer_consume(flac->buf, id3_size);
    }
    else {
       buffer_clear(flac->buf);
       
      if (PerlIO_seek(infile, id3_size, SEEK_SET) < 0) {
        err = -1;
        goto out;
      }
    }
    
    if ( !_check_buf(infile, flac->buf, 4, FLAC_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
  }
  
  // Verify fLaC magic
  bptr = buffer_ptr(flac->buf);
  if ( memcmp(bptr, "fLaC", 4) != 0 ) {
    PerlIO_printf(PerlIO_stderr(), "Not a valid FLAC file: %s\n", file);
    err = -1;
    goto out;
  }
  
  buffer_consume(flac->buf, 4);
  
  flac->audio_offset += 4;
  
  // Parse all metadata blocks
  while ( !done ) {
    uint8_t type;
    unsigned int len;
    
    if ( !_check_buf(infile, flac->buf, 4, FLAC_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    bptr = buffer_ptr(flac->buf);
    
    if ( bptr[0] & 0x80 ) {
      // last metadata block flag
      done = 1;
    }
    
    type = bptr[0] & 0x7f;
    len  = (bptr[1] << 16) | (bptr[2] << 8) | bptr[3];
    
    buffer_consume(flac->buf, 4);
    
    DEBUG_TRACE("Parsing metadata block, type %d, len %d, done %d\n", type, len, done);
    
    if ( len > flac->file_size - flac->audio_offset ) {
      err = -1;
      goto out;
    }
    
    if ( !_check_buf(infile, flac->buf, len, len) ) {
      err = -1;
      goto out;
    }
    
    flac->audio_offset += 4 + len;
    
    switch (type) {
      case FLAC_TYPE_STREAMINFO:
        _flac_parse_streaminfo(flac);
        break;
      
      case FLAC_TYPE_VORBIS_COMMENT:
        if ( !flac->seeking ) {
          // Vorbis comment parsing code from ogg.c
          _parse_vorbis_comments(flac->buf, tags, 0);
        }
        else {
          DEBUG_TRACE("  seeking, not parsing comments\n");
          buffer_consume(flac->buf, len);
        }
        break;
      
      case FLAC_TYPE_APPLICATION:
        if ( !flac->seeking ) {
          _flac_parse_application(flac, len);
        }
        else {
          DEBUG_TRACE("  seeking, skipping application\n");
          buffer_consume(flac->buf, len);
        }
        break;
        
      case FLAC_TYPE_SEEKTABLE:
        if (flac->seeking) {
          _flac_parse_seektable(flac, len);
        }
        else {
          DEBUG_TRACE("  not seeking, skipping seektable\n");
          buffer_consume(flac->buf, len);
        }
        break;
        
      case FLAC_TYPE_CUESHEET:
        if ( !flac->seeking ) {
          _flac_parse_cuesheet(flac);
        }
        else {
          DEBUG_TRACE("  seeking, skipping cuesheet\n");
          buffer_consume(flac->buf, len);
        }
        break;
      
      case FLAC_TYPE_PICTURE:
        if ( !flac->seeking ) {
          if ( !_flac_parse_picture(flac) ) {
            goto out;
          }
        }
        else {
          DEBUG_TRACE("  seeking, skipping picture\n");
          buffer_consume(flac->buf, len);
        }
        break;
      
      case FLAC_TYPE_PADDING:
      default:
        DEBUG_TRACE("  unhandled or padding, skipping\n");
        buffer_consume(flac->buf, len);
    } 
  }
  
  song_length_ms = SvIV( *( my_hv_fetch(info, "song_length_ms") ) );
  
  if (song_length_ms > 0) {
    my_hv_store( info, "bitrate", newSVuv(8 * (flac->file_size - flac->audio_offset) / (1. * song_length_ms / 1000) ));
  }
  
  my_hv_store( info, "file_size", newSVuv(flac->file_size) );
  my_hv_store( info, "audio_offset", newSVuv(flac->audio_offset) );
  
  // Parse ID3 last, due to an issue with libid3tag screwing
  // up the filehandle
  if (id3_size) {
    parse_id3(infile, file, info, tags, 0);
  }

out:
  buffer_free(flac->buf);
  Safefree(flac->buf);
  
  return flac;
}

// offset is in ms, does sample-accurate seeking, using seektable if available
static int
flac_find_frame(PerlIO *infile, char *file, int offset)
{
  int frame_offset = -1;
  uint32_t samplerate;
  uint64_t target_sample;
  
  // We need to read all metadata first to get some data we need to calculate
  HV *info = newHV();
  HV *tags = newHV();
  flacinfo *flac = _flac_parse(infile, file, info, tags, 1);
  
  samplerate   = SvIV( *(my_hv_fetch( info, "samplerate" )) );
  
  // Determine target sample we're looking for
  target_sample = ((offset - 1) / 10) * (samplerate / 100);
  DEBUG_TRACE("Looking for target sample %d\n", target_sample);
  
  if (flac->num_seekpoints) {
    // Use seektable to find seek point
    // Start looking at seekpoint 1
    int i;
    uint32_t start_point;
    uint32_t stop_point;
    
    for ( i = 1; i < flac->num_seekpoints; i++ ) {
      // Skip placeholder entries
      if ( flac->seekpoints[i].sample_number == 0xFFFFFFFFFFFFFFFFLL ) {
        continue;
      }
      
      if ( flac->seekpoints[i].sample_number >= target_sample ) {
        uint32_t diff = target_sample - flac->seekpoints[i - 1].sample_number;
        
        DEBUG_TRACE("  using seekpoint %d, diff %d samples\n", i - 1, diff);
        
        start_point = flac->audio_offset + flac->seekpoints[i - 1].stream_offset;
        
        if ( diff < flac->seekpoints[i - 1].frame_samples ) {
          // Target sample is within the seekpoint frame, shortcut and use it
          frame_offset = start_point;
        }
        else {
          // Search for frame containing this sample, between 2 seekpoints
          stop_point = flac->audio_offset + flac->seekpoints[i].stream_offset;
          
          frame_offset = _flac_binary_search_sample(flac, target_sample, start_point, stop_point);
        }
        
        break;
      }
    }
    
    if ( frame_offset == -1 ) {
      // Target sample was beyond the last seekpoint
      start_point = flac->audio_offset + flac->seekpoints[ flac->num_seekpoints - 1 ].stream_offset;
      stop_point  = flac->file_size;
      
      frame_offset = _flac_binary_search_sample(flac, target_sample, start_point, stop_point);
    }      
  }
  else {
    // No seektable available, search for it
    DEBUG_TRACE("  no seektable available\n");
    frame_offset = _flac_binary_search_sample(flac, target_sample, flac->audio_offset, flac->file_size);
  }
  
  // Don't leak
  SvREFCNT_dec(info);
  SvREFCNT_dec(tags);
  
  // free seek struct
  Safefree(flac->seekpoints);
  
  Safefree(flac);
  
  return frame_offset;
}

int
_flac_binary_search_sample(flacinfo *flac, uint64_t target_sample, off_t low, off_t high)
{
  off_t mid;
  Buffer buf;
  unsigned char *bptr;
  unsigned int buf_size;
  int frame_offset = -1;
  uint64_t first_sample;
  uint64_t last_sample;
  int i;
  
  buffer_init(&buf, FLAC_FRAME_MAX_BLOCK);
  
  while (low <= high) {
    mid = low + ((high - low) / 2);
  
    DEBUG_TRACE("  Searching for sample %d between %d and %d (mid %d)\n", target_sample, low, high, mid);
  
    PerlIO_seek(flac->infile, mid, SEEK_SET);
      
    if ( !_check_buf(flac->infile, &buf, FLAC_FRAME_MAX_HEADER, FLAC_FRAME_MAX_BLOCK) ) {
      goto out;
    }
  
    bptr = buffer_ptr(&buf);
    buf_size = buffer_len(&buf);
  
    for (i = 0; i != buf_size - FLAC_HEADER_LEN; i++) {
      if (bptr[i] != 0xFF)
        continue;
      
      // Verify we have a valid FLAC frame header
      // and get the first/last sample numbers in the frame if it's valid
      if ( !_flac_first_sample( &bptr[i], &first_sample, &last_sample ) )
        continue;
      
      frame_offset = mid + i;
      
      break;
    }
    
    DEBUG_TRACE("  first_sample %ld, last_sample %ld\n", first_sample, last_sample);
    
    if (first_sample <= target_sample && last_sample >= target_sample) {
      // found frame
      DEBUG_TRACE("  found frame at %d\n", frame_offset);
      goto out;
    }
  
    if (target_sample < first_sample) {
      high = mid - 1;
      DEBUG_TRACE("  high = %d\n", high);
    }
    else {
      low = mid + 1;
      DEBUG_TRACE("  low = %d\n", low);
    }
    
    buffer_clear(&buf);
  }
  
out:
  buffer_free(&buf);

  return frame_offset;
}

int
_flac_first_sample(unsigned char *buf, uint64_t *first_sample, uint64_t *last_sample)
{
  // A lot of this code is based on libFLAC stream_decoder.c read_frame_header_
  uint32_t x;
  uint64_t xx;
  uint32_t blocksize = 0;
  uint32_t blocksize_hint = 0;
  uint32_t samplerate_hint = 0;
  uint32_t frame_number = 0;
  uint8_t  raw_header_len = 4;
  uint8_t  crc8;
  
  // Verify sync and various reserved bits
  if ( buf[0] != 0xFF 
    || buf[1] & 0x02
    || buf[3] & 0x01
  ) {
    return 0;
  }
  
  // Block size
  switch(x = buf[2] >> 4) {
    case 0:
      return 0;
    case 1:
      blocksize = 192;
      break;
    case 2: case 3: case 4: case 5:
      blocksize = 576 << (x-2);
      break;
    case 6: case 7:
      blocksize_hint = x;
      break;
    case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15:
      blocksize = 256 << (x-8);
      break;
    default:
      break;
  }
  
  // Sample rate, all we need here is the hint
  switch(x = buf[2] & 0x0f) {
    case 12: case 13: case 14:
      samplerate_hint = x;
      break;
    case 15:
      return 0;
    default:
      break;
  }
  
  //DEBUG_TRACE("Checking frame header %0x %0x %0x %0x\n", buf[0], buf[1], buf[2], buf[3]);
  
  if ( buf[1] & 0x1 ) {
    // Variable blocksize
    // XXX Flake support requires checking min_blocksize != max_blocksize from streaminfo to determine this
    if ( !_flac_read_utf8_uint64(buf, &xx, &raw_header_len) )
      return 0;
    
    if ( xx == 0xFFFFFFFFFFFFFFFFLL )
      return 0;
      
    //DEBUG_TRACE("  variable blocksize, first sample %ld\n", xx);
    
    *first_sample = xx;
  }
  else {
    // Fixed blocksize, x = frame number
    if ( !_flac_read_utf8_uint32(buf, &x, &raw_header_len) )
      return 0;
    
    if ( x == 0xFFFFFFFF )
      return 0;
    
    //DEBUG_TRACE("  fixed blocksize, frame number %d\n", x);
    
    frame_number = x;
  }
  
  // XXX need test
  if (blocksize_hint) {
    x = buf[raw_header_len++];
    if (blocksize_hint == 7) {
      uint32_t _x = buf[raw_header_len++];
      x = (x << 8) | _x;
    }
    blocksize = x + 1;
  }
  
  //DEBUG_TRACE("  blocksize %d\n", blocksize);
  
  // XXX need test
  if (samplerate_hint) {
    raw_header_len++;
    if (samplerate_hint != 12) {
      raw_header_len++;
    }
  }
  
  // Verify CRC-8
  crc8 = buf[raw_header_len];
  if ( _flac_crc8(buf, raw_header_len) != crc8 ) {
    //DEBUG_TRACE("  CRC failed\n");
    return 0;
  }
  
  // Calculate sample number from frame number if needed
  if (frame_number) {
    *first_sample = frame_number * blocksize;
  }
  
  *last_sample = *first_sample + blocksize;
  
  return 1;
}

void
_flac_parse_streaminfo(flacinfo *flac)
{
  uint64_t tmp;
  SV *md5;
  unsigned char *bptr;
  int i;
  uint32_t samplerate;
  uint64_t total_samples;
  uint32_t song_length_ms;
  
  my_hv_store( flac->info, "minimum_blocksize", newSVuv( buffer_get_short(flac->buf) ) );
  my_hv_store( flac->info, "maximum_blocksize", newSVuv( buffer_get_short(flac->buf) ) );
  
  my_hv_store( flac->info, "minimum_framesize", newSVuv( buffer_get_int24(flac->buf) ) );
  my_hv_store( flac->info, "maximum_framesize", newSVuv( buffer_get_int24(flac->buf) ) );
  
  tmp = buffer_get_int64(flac->buf);
  
  samplerate = (tmp >> 44) & 0xFFFFF;
  total_samples = tmp & 0xFFFFFFFFFLL;
  
  my_hv_store( flac->info, "samplerate", newSVuv(samplerate) );
  my_hv_store( flac->info, "channels", newSVuv( ((tmp >> 41) & 0x7) + 1 ) );
  my_hv_store( flac->info, "bits_per_sample", newSVuv( ((tmp >> 36) & 0x1F) + 1 ) );
  my_hv_store( flac->info, "total_samples", newSVnv(total_samples) );
  
  bptr = buffer_ptr(flac->buf);
  md5 = newSVpvf("%02x", bptr[0]);

  for (i = 1; i < 16; i++) {
    sv_catpvf(md5, "%02x", bptr[i]);
  }

  my_hv_store(flac->info, "md5", md5);
  buffer_consume(flac->buf, 16);
  
  song_length_ms = ( (total_samples * 1.0) / samplerate) * 1000;
  my_hv_store( flac->info, "song_length_ms", newSVuv(song_length_ms) );
}

void
_flac_parse_application(flacinfo *flac, int len)
{
  HV *app;
  SV *id = newSVuv( buffer_get_int(flac->buf) );
  SV *data = newSVpvn( buffer_ptr(flac->buf), len - 4 );
  buffer_consume(flac->buf, len - 4);
  
  if ( my_hv_exists(flac->tags, "APPLICATION") ) {
    // XXX needs test
    SV **entry = my_hv_fetch(flac->tags, "APPLICATION");
    if (entry != NULL) {
      app = (HV *)SvRV(*entry);
      my_hv_store_ent(app, id, data);
    }
  }
  else {
    app = newHV();
    
    my_hv_store_ent(app, id, data);

    my_hv_store( flac->tags, "APPLICATION", newRV_noinc( (SV *)app ) );
  }
  
  SvREFCNT_dec(id);
}

void
_flac_parse_seektable(flacinfo *flac, int len)
{
  int i;
  uint32_t count = len / 18;
  
  flac->num_seekpoints = count;
  
  New(0, 
    flac->seekpoints,
    count * sizeof(*flac->seekpoints),
    struct seekpoint
  );
  
  for (i = 0; i < count; i++) {
    flac->seekpoints[i].sample_number = buffer_get_int64(flac->buf);
    flac->seekpoints[i].stream_offset = buffer_get_int64(flac->buf);
    flac->seekpoints[i].frame_samples = buffer_get_short(flac->buf);
    
    DEBUG_TRACE(
      "  sample_number %ld stream_offset %ld frame_samples %d\n",
      flac->seekpoints[i].sample_number,
      flac->seekpoints[i].stream_offset,
      flac->seekpoints[i].frame_samples
    );
  }
}

void
_flac_parse_cuesheet(flacinfo *flac)
{
  AV *cue = newAV();
  unsigned char *bptr;
  uint64_t leadin;
  uint8_t is_cd;
  char decimal[21];
  uint8_t num_tracks;
  
  // Catalog number, may be empty
  bptr = buffer_ptr(flac->buf);
  if (bptr[0]) {
    av_push( cue, newSVpvf("CATALOG %s\n", bptr) );
  }
  buffer_consume(flac->buf, 128);
  
  leadin = buffer_get_int64(flac->buf);
  is_cd = (uint8_t)buffer_get_char(flac->buf);
  
  buffer_consume(flac->buf, 258);
  
  num_tracks = (uint8_t)buffer_get_char(flac->buf);
  DEBUG_TRACE("  number of cue tracks: %d\n", num_tracks);
  
  av_push( cue, newSVpvf("FILE \"%s\" FLAC\n", flac->file) );
  
  while (num_tracks--) {
    char isrc[13];
    uint8_t tmp;
    uint8_t type;
    uint8_t pre;
    uint8_t num_index;
    
    uint64_t track_offset = buffer_get_int64(flac->buf);
    uint8_t  tracknum = (uint8_t)buffer_get_char(flac->buf);
    
    buffer_get(flac->buf, isrc, 12);
    isrc[12] = '\0';
    
    tmp = (uint8_t)buffer_get_char(flac->buf);
    type = (tmp >> 7) & 0x1;
    pre  = (tmp >> 6) & 0x1;
    buffer_consume(flac->buf, 13);
    
    num_index = (uint8_t)buffer_get_char(flac->buf);
    
    DEBUG_TRACE("    track %d: offset %ld, type %d, pre %d, num_index %d\n", tracknum, track_offset, type, pre, num_index);
    
    if (tracknum > 0 && tracknum < 100) {
      av_push( cue, newSVpvf("  TRACK %02u %s\n",
        tracknum, type == 0 ? "AUDIO" : "DATA"
      ) );
      
      if (pre) {
        av_push( cue, newSVpv("    FLAGS PRE\n", 0) );
      }
      
      if (isrc[0]) {
        av_push( cue, newSVpvf("    ISRC %s\n", isrc) );
      }
    }
    
    while (num_index--) {
      SV *index;
      
      uint64_t index_offset = buffer_get_int64(flac->buf);
      uint8_t index_num = (uint8_t)buffer_get_char(flac->buf);
      buffer_consume(flac->buf, 3);
      
      DEBUG_TRACE("      index %d, offset %ld\n", index_num, index_offset);
      
      index = newSVpvf("    INDEX %02u ", index_num);
      
      if (is_cd) {
        uint32_t samplerate = SvIV( *( my_hv_fetch( flac->info, "samplerate") ) );
        uint64_t frame = ((track_offset + index_offset) / (samplerate / 75));
        uint8_t m, s, f;
        
        f = frame % 75;
        frame /= 75;
        s = frame % 60;
        frame /= 60;
        m = frame;

        sv_catpvf(index, "%02u:%02u:%02u\n", m, s, f);
      }
      else {
        // XXX need test
        sprintf(decimal, "%"PRIu64, track_offset + index_offset);
        sv_catpvf(index, "%s\n", decimal);
      }
      
      av_push( cue, index );
    }
    
    if (tracknum == 170) {
      // Add lead-in and lead-out
      sprintf(decimal, "%"PRIu64, leadin);
      av_push( cue, newSVpvf("REM FLAC__lead-in %s\n", decimal) );
      
      // XXX is tracknum right here?
      sprintf(decimal, "%"PRIu64, track_offset);
      av_push( cue, newSVpvf("REM FLAC__lead-out %u %s\n", tracknum, decimal) );
    }
  }
  
  my_hv_store( flac->tags, "CUESHEET_BLOCK", newRV_noinc( (SV *)cue ) );
}

int
_flac_parse_picture(flacinfo *flac)
{
  AV *pictures;
  HV *picture = newHV();
  int ret = 1;
  uint32_t mime_length;
  uint32_t desc_length;
  uint32_t pic_length;
  SV *desc;
  
  my_hv_store( picture, "picture_type", newSVuv( buffer_get_int(flac->buf) ) );
  
  mime_length = buffer_get_int(flac->buf);
  DEBUG_TRACE("  mime_length: %d\n", mime_length);
  if (mime_length > buffer_len(flac->buf)) {
    PerlIO_printf(PerlIO_stderr(), "Invalid FLAC file: %s, bad picture block\n", flac->file);
    ret = 0;
    goto out;
  }
  
  my_hv_store( picture, "mime_type", newSVpvn( buffer_ptr(flac->buf), mime_length ) );
  buffer_consume(flac->buf, mime_length);
  
  desc_length = buffer_get_int(flac->buf);
  DEBUG_TRACE("  desc_length: %d\n", mime_length);
  if (desc_length > buffer_len(flac->buf)) {
    PerlIO_printf(PerlIO_stderr(), "Invalid FLAC file: %s, bad picture block\n", flac->file);
    ret = 0;
    goto out;
  }
  
  desc = newSVpvn( buffer_ptr(flac->buf), desc_length );
  sv_utf8_decode(desc); // XXX needs test with utf8 desc
  my_hv_store( picture, "description", desc );
  buffer_consume(flac->buf, desc_length);
  
  my_hv_store( picture, "width", newSVuv( buffer_get_int(flac->buf) ) );
  my_hv_store( picture, "height", newSVuv( buffer_get_int(flac->buf) ) );
  my_hv_store( picture, "depth", newSVuv( buffer_get_int(flac->buf) ) );
  my_hv_store( picture, "color_index", newSVuv( buffer_get_int(flac->buf) ) );
  
  pic_length = buffer_get_int(flac->buf);
  DEBUG_TRACE("  pic_length: %d\n", pic_length);
  if (pic_length > buffer_len(flac->buf)) {
    PerlIO_printf(PerlIO_stderr(), "Invalid FLAC file: %s, bad picture block\n", flac->file);
    ret = 0;
    goto out;
  }
  
  my_hv_store( picture, "image_data", newSVpvn( buffer_ptr(flac->buf), pic_length ) );
  buffer_consume(flac->buf, pic_length);
  
  DEBUG_TRACE("  found picture of length %d\n", pic_length);
  
  if ( my_hv_exists(flac->tags, "ALLPICTURES") ) {
    // XXX needs test
    SV **entry = my_hv_fetch(flac->tags, "ALLPICTURES");
    if (entry != NULL) {
      pictures = (AV *)SvRV(*entry);
      av_push( pictures, newRV_noinc( (SV *)picture ) );
    }
  }
  else {
    pictures = newAV();
    
    av_push( pictures, newRV_noinc( (SV *)picture ) );

    my_hv_store( flac->tags, "ALLPICTURES", newRV_noinc( (SV *)pictures ) );
  }

out:
  return ret;
}

/* CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0 */
uint8_t const _flac_crc8_table[256] = {
  0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
  0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
  0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
  0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
  0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
  0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
  0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
  0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
  0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
  0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
  0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
  0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
  0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
  0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
  0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
  0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
  0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
  0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
  0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
  0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
  0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
  0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
  0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
  0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
  0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
  0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
  0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
  0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
  0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
  0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
  0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
  0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

uint8_t
_flac_crc8(const unsigned char *buf, unsigned len)
{
  uint8_t crc = 0;

  while(len--)
    crc = _flac_crc8_table[crc ^ *buf++];

  return crc;
}

int
_flac_read_utf8_uint64(unsigned char *raw, uint64_t *val, uint8_t *rawlen)
{
  uint64_t v = 0;
  uint32_t x;
  unsigned i;
  
  x = raw[(*rawlen)++];
  
  if(!(x & 0x80)) { /* 0xxxxxxx */
    v = x;
    i = 0;
  }
  else if(x & 0xC0 && !(x & 0x20)) { /* 110xxxxx */
    v = x & 0x1F;
    i = 1;
  }
  else if(x & 0xE0 && !(x & 0x10)) { /* 1110xxxx */
    v = x & 0x0F;
    i = 2;
  }
  else if(x & 0xF0 && !(x & 0x08)) { /* 11110xxx */
    v = x & 0x07;
    i = 3;
  }
  else if(x & 0xF8 && !(x & 0x04)) { /* 111110xx */
    v = x & 0x03;
    i = 4;
  }
  else if(x & 0xFC && !(x & 0x02)) { /* 1111110x */
    v = x & 0x01;
    i = 5;
  }
  else if(x & 0xFE && !(x & 0x01)) { /* 11111110 */
    v = 0;
    i = 6;
  }
  else {
    *val = 0xffffffffffffffffLL;
    return 1;
  }
  
  for( ; i; i--) {
    x = raw[(*rawlen)++];
    if(!(x & 0x80) || (x & 0x40)) { /* 10xxxxxx */
      *val = 0xffffffffffffffffLL;
      return 1;
    }
    v <<= 6;
    v |= (x & 0x3F);
  }
  *val = v;
  return 1;
}

int
_flac_read_utf8_uint32(unsigned char *raw, uint32_t *val, uint8_t *rawlen)
{
  uint32_t v = 0;
  uint32_t x;
  unsigned i;
  
  x = raw[(*rawlen)++];
  
  if(!(x & 0x80)) { /* 0xxxxxxx */
    v = x;
    i = 0;
  }
  else if(x & 0xC0 && !(x & 0x20)) { /* 110xxxxx */
    v = x & 0x1F;
    i = 1;
  }
  else if(x & 0xE0 && !(x & 0x10)) { /* 1110xxxx */
    v = x & 0x0F;
    i = 2;
  }
  else if(x & 0xF0 && !(x & 0x08)) { /* 11110xxx */
    v = x & 0x07;
    i = 3;
  }
  else if(x & 0xF8 && !(x & 0x04)) { /* 111110xx */
    v = x & 0x03;
    i = 4;
  }
  else if(x & 0xFC && !(x & 0x02)) { /* 1111110x */
    v = x & 0x01;
    i = 5;
  }
  else {
    *val = 0xffffffff;
    return 1;
  }
  
  for( ; i; i--) {
    x = raw[(*rawlen)++];
    if(!(x & 0x80) || (x & 0x40)) { /* 10xxxxxx */
      *val = 0xffffffff;
      return 1;
    }
    v <<= 6;
    v |= (x & 0x3F);
  }
  *val = v;
  return 1;
}
