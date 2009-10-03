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

#include "mp4.h"

static int
get_mp4tags(PerlIO *infile, char *file, HV *info, HV *tags)
{
  mp4info *mp4 = _mp4_parse(infile, file, info, tags, 0);
  
  Safefree(mp4);

  return 0;
}  

// offset is in ms
// This is based on code from Rockbox
int
mp4_find_frame(PerlIO *infile, char *file, int offset)
{
  uint16_t samplerate = 0;
  uint32_t sound_sample_loc;
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t new_sample = 0;
  uint32_t new_sound_sample = 0;
  
  uint32_t chunk = 1;
  uint32_t range_samples = 0;
  uint32_t total_samples = 0;
  uint32_t chunk_sample;
  uint32_t prev_chunk;
  uint32_t prev_chunk_samples;
  uint32_t file_offset;
  
  // We need to read all info first to get some data we need to calculate
  HV *info = newHV();
  HV *tags = newHV();
  mp4info *mp4 = _mp4_parse(infile, file, info, tags, 1);
  
  // Pull out the samplerate
  samplerate = SvIV( *( my_hv_fetch( info, "samplerate" ) ) );
  
  // convert offset to sound_sample_loc
  sound_sample_loc = ((offset - 1) / 10) * (samplerate / 100);
  
  // Make sure we have the necessary metadata
  if ( 
       !mp4->num_time_to_samples 
    || !mp4->num_sample_byte_sizes
    || !mp4->num_sample_to_chunks
    || !mp4->num_chunk_offsets
  ) {
    PerlIO_printf(PerlIO_stderr(), "find_frame: File does not contain seek metadata: %s\n", file);
    return -1;
  }
  
  // Find the destination block from time_to_sample array
  while ( (i < mp4->num_time_to_samples) &&
      (new_sound_sample < sound_sample_loc)
  ) {
      j = (sound_sample_loc - new_sound_sample) / mp4->time_to_sample[i].sample_duration;
      
      DEBUG_TRACE("i = %d / j = %d\n", i, j);
  
      if (j <= mp4->time_to_sample[i].sample_count) {
          new_sample += j;
          new_sound_sample += j * mp4->time_to_sample[i].sample_duration;
          break;
      } 
      else {
          new_sound_sample += (mp4->time_to_sample[i].sample_duration
              * mp4->time_to_sample[i].sample_count);
          new_sample += mp4->time_to_sample[i].sample_count;
          i++;
      }
  }
  
  if ( new_sample >= mp4->num_sample_byte_sizes ) {
    PerlIO_printf(PerlIO_stderr(), "find_frame: Offset out of range (%d >= %d)\n", new_sample, mp4->num_sample_byte_sizes);
    return -1;
  }
  
  DEBUG_TRACE("new_sample: %d, new_sound_sample: %d\n", new_sample, new_sound_sample);
  
  // We know the new block, now calculate the file position
  
  /* Locate the chunk containing the sample */

  prev_chunk         = mp4->sample_to_chunk[0].first_chunk;
  prev_chunk_samples = mp4->sample_to_chunk[0].num_samples;
  
  for (i = 1; i < mp4->num_sample_to_chunks; i++) {
    chunk = mp4->sample_to_chunk[i].first_chunk;
    range_samples = (chunk - prev_chunk) * prev_chunk_samples;

    if (new_sample < total_samples + range_samples)
      break;

    total_samples += range_samples;
    prev_chunk = mp4->sample_to_chunk[i].first_chunk;
    prev_chunk_samples = mp4->sample_to_chunk[i].num_samples;
  }
  
  DEBUG_TRACE("prev_chunk: %d, prev_chunk_samples: %d, total_samples: %d\n", prev_chunk, prev_chunk_samples, total_samples);
  
  if (new_sample >= mp4->sample_to_chunk[0].num_samples) {
    chunk = prev_chunk + (new_sample - total_samples) / prev_chunk_samples;
  }
  else {
    chunk = 1;
  }
  
  DEBUG_TRACE("chunk: %d\n", chunk);
  
  /* Get sample of the first sample in the chunk */
  
  chunk_sample = total_samples + (chunk - prev_chunk) * prev_chunk_samples;
  
  DEBUG_TRACE("chunk_sample: %d\n", chunk_sample);
  
  /* Get offset in file */

  if (chunk > mp4->num_chunk_offsets) {
    file_offset = mp4->chunk_offset[mp4->num_chunk_offsets - 1];
  }
  else {
    file_offset = mp4->chunk_offset[chunk - 1];
  }
  
  DEBUG_TRACE("file_offset: %d\n", file_offset);

  if (chunk_sample > new_sample) {
    PerlIO_printf(PerlIO_stderr(), "find_frame: sample out of range (%d > %d)\n", chunk_sample, new_sample);
    return -1;
  }
  
  for (i = chunk_sample; i < new_sample; i++) {
    file_offset += mp4->sample_byte_size[i];
    DEBUG_TRACE("  file_offset: %d\n", file_offset);
  }
  
  if (file_offset > mp4->audio_offset + mp4->audio_size) {
    PerlIO_printf(PerlIO_stderr(), "find_frame: file offset out of range (%d > %lld)\n", file_offset, mp4->audio_offset + mp4->audio_size);
    return -1;
  }
  
  // Don't leak
  SvREFCNT_dec(info);
  SvREFCNT_dec(tags);
  
  // free seek structs
  Safefree(mp4->time_to_sample);
  Safefree(mp4->sample_to_chunk);
  Safefree(mp4->sample_byte_size);
  Safefree(mp4->chunk_offset);
  
  Safefree(mp4);
  
  return file_offset;
}

mp4info *
_mp4_parse(PerlIO *infile, char *file, HV *info, HV *tags, uint8_t seeking)
{
  off_t file_size;
  uint32_t box_size = 0;
  
  mp4info *mp4;
  Newxz(mp4, sizeof(mp4info), mp4info);
  Newxz(mp4->buf, sizeof(Buffer), Buffer);
  
  mp4->audio_offset  = 0;
  mp4->infile        = infile;
  mp4->file          = file;
  mp4->info          = info;
  mp4->tags          = tags;
  mp4->current_track = 0;
  mp4->seen_moov     = 0;
  mp4->seeking       = seeking ? 1 : 0;
  
  buffer_init(mp4->buf, MP4_BLOCK_SIZE);
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  my_hv_store( info, "file_size", newSVuv(file_size) );
  
  // Create empty tracks array
  my_hv_store( info, "tracks", newRV_noinc( (SV *)newAV() ) );
  
  while ( (box_size = _mp4_read_box(mp4)) > 0 ) {
    mp4->audio_offset += box_size;
    DEBUG_TRACE("read box of size %d / audio_offset %d\n", box_size, mp4->audio_offset);
    
    if (mp4->audio_offset >= file_size)
      break;
  }
  
  // XXX: if no ftyp was found, assume it is brand 'mp41'
  
  // if no bitrate was found (i.e. ALAC), calculate based on file_size/song_length_ms
  if (mp4->need_calc_bitrate) {
    HV *trackinfo = _mp4_get_current_trackinfo(mp4);
    SV **entry = my_hv_fetch(info, "song_length_ms");
    if (entry) {
      SV **audio_offset = my_hv_fetch(info, "audio_offset");
      if (audio_offset) {
        uint32_t song_length_ms = SvIV(*entry);
        uint32_t bitrate = ((file_size - SvIV(*audio_offset) * 1.0) / song_length_ms) * 1000;
      
        my_hv_store( trackinfo, "avg_bitrate", newSVuv(bitrate) );
      }
    }
  }
  
  buffer_free(mp4->buf);
  Safefree(mp4->buf);
  
  return mp4;
}

int
_mp4_read_box(mp4info *mp4)
{
  uint64_t size;  // total size of box
  char type[5];
  uint8_t skip = 0;
  
  mp4->rsize = 0; // remaining size in box
  
  if ( !_check_buf(mp4->infile, mp4->buf, 16, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  size = buffer_get_int(mp4->buf);
  strncpy( type, (char *)buffer_ptr(mp4->buf), 4 );
  type[4] = '\0';
  buffer_consume(mp4->buf, 4);
  
  // Check for 64-bit size
  if (size == 1) {
    size = buffer_get_int64(mp4->buf);
    mp4->hsize = 16;
  }
  else if (size == 0) {
    // XXX: size extends to end of file
    mp4->hsize = 8;
  }
  else {
    mp4->hsize = 8;
  }
  
  if (size) {
    mp4->rsize = size - mp4->hsize;
  }
  
  mp4->size = size;
  
  DEBUG_TRACE("%s size %d\n", type, size);
  
  if ( FOURCC_EQ(type, "ftyp") ) {
    if ( !_mp4_parse_ftyp(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad ftyp box): %s\n", mp4->file);
      return 0;
    }
  }
  else if ( 
       FOURCC_EQ(type, "moov") 
    || FOURCC_EQ(type, "trak") 
    || FOURCC_EQ(type, "edts")
    || FOURCC_EQ(type, "mdia")
    || FOURCC_EQ(type, "minf")
    || FOURCC_EQ(type, "dinf")
    || FOURCC_EQ(type, "stbl")
    || FOURCC_EQ(type, "udta")
  ) {
    // These boxes are containers for nested boxes, return only the fact that
    // we read the header size of the container
    size = mp4->hsize;
  }
  else if ( FOURCC_EQ(type, "mvhd") ) {
    mp4->seen_moov = 1;
    
    if ( !_mp4_parse_mvhd(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad mvhd box): %s\n", mp4->file);
      return 0;
    }
  }
  else if ( FOURCC_EQ(type, "tkhd") ) {
    if ( !_mp4_parse_tkhd(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad tkhd box): %s\n", mp4->file);
      return 0;
    }
  }
  else if ( FOURCC_EQ(type, "mdhd") ) {
    if ( !_mp4_parse_mdhd(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad mdhd box): %s\n", mp4->file);
      return 0;
    }
  }
  else if ( FOURCC_EQ(type, "hdlr") ) {
    if ( !_mp4_parse_hdlr(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad hdlr box): %s\n", mp4->file);
      return 0;
    }
  }
  else if ( FOURCC_EQ(type, "stsd") ) {
    if ( !_mp4_parse_stsd(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad stsd box): %s\n", mp4->file);
      return 0;
    }
    
    // stsd is a special real box + container, count only the real bytes (8)
    size = 8 + mp4->hsize;
  }
  else if ( FOURCC_EQ(type, "mp4a") ) {
    if ( !_mp4_parse_mp4a(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad mp4a box): %s\n", mp4->file);
      return 0;
    }
    
    // mp4a is a special real box + container, count only the real bytes (28)
    size = 28 + mp4->hsize;
  }
  else if ( FOURCC_EQ(type, "alac") ) {
    // Mark encoding
    HV *trackinfo = _mp4_get_current_trackinfo(mp4);
    
    my_hv_store( trackinfo, "encoding", newSVpvn("alac", 4) );
    
    // Flag that we'll have to calculate bitrate later
    mp4->need_calc_bitrate = 1;
        
    // Skip rest
    skip = 1;
  }
  else if ( FOURCC_EQ(type, "drms") ) {
    // Mark encoding
    HV *trackinfo = _mp4_get_current_trackinfo(mp4);
    
    my_hv_store( trackinfo, "encoding", newSVpvn("drms", 4) );
    
    // Skip rest
    skip = 1;
  }
  else if ( FOURCC_EQ(type, "esds") ) {
    if ( !_mp4_parse_esds(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad esds box): %s\n", mp4->file);
      return 0;
    }
  }
  else if ( FOURCC_EQ(type, "stts") ) {
    if ( mp4->seeking ) {
      if ( !_mp4_parse_stts(mp4) ) {
        PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad stts box): %s\n", mp4->file);
        return 0;
      }
    }
    else {
      skip = 1;
    }
  }
  else if ( FOURCC_EQ(type, "stsc") ) {
    if ( mp4->seeking ) {
      if ( !_mp4_parse_stsc(mp4) ) {
        PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad stsc box): %s\n", mp4->file);
        return 0;
      }
    }
    else {
      skip = 1;
    }
  }
  else if ( FOURCC_EQ(type, "stsz") ) {
    if ( mp4->seeking ) {
      if ( !_mp4_parse_stsz(mp4) ) {
        PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad stsz box): %s\n", mp4->file);
        return 0;
      }
    }
    else {
      skip = 1;
    }
  }
  else if ( FOURCC_EQ(type, "stco") ) {
    if ( mp4->seeking ) {
      if ( !_mp4_parse_stco(mp4) ) {
        PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad stco box): %s\n", mp4->file);
        return 0;
      }
    }
    else {
      skip = 1;
    }
  }
  else if ( FOURCC_EQ(type, "meta") ) {
    uint8_t meta_size = _mp4_parse_meta(mp4);
    if ( !meta_size ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad meta box): %s\n", mp4->file);
      return 0;
    }
    
    // meta is a special real box + container, count only the real bytes
    size = meta_size + mp4->hsize;
  }
  else if ( FOURCC_EQ(type, "ilst") ) {
    if ( !_mp4_parse_ilst(mp4) ) {
      PerlIO_printf(PerlIO_stderr(), "Invalid MP4 file (bad ilst box): %s\n", mp4->file);
      return 0;
    }
  }
  else if ( FOURCC_EQ(type, "mdat") ) {
    // Audio data here, there may be boxes after mdat, so we have to skip it
    skip = 1;
    
    // If we haven't seen moov yet, set a flag so we can print a warning
    // or handle it some other way
    if ( !mp4->seen_moov ) {
      my_hv_store( mp4->info, "leading_mdat", newSVuv(1) );
    }
    
    // Record audio offset and length
    my_hv_store( mp4->info, "audio_offset", newSVuv(mp4->audio_offset) );
    mp4->audio_size = size;
  }
  else {
    DEBUG_TRACE("  Unhandled box, skipping\n");
    skip = 1;
  }
  
  if (skip) {
    if ( buffer_len(mp4->buf) >= mp4->rsize ) {
      //buffer_dump(mp4->buf, mp4->rsize);
      buffer_consume(mp4->buf, mp4->rsize);
    }
    else {
      PerlIO_seek(mp4->infile, mp4->rsize - buffer_len(mp4->buf), SEEK_CUR);
      buffer_clear(mp4->buf);
      
      DEBUG_TRACE("  seeked to %d\n", PerlIO_tell(mp4->infile));
    }
  }
  
  return size;
}

uint8_t
_mp4_parse_ftyp(mp4info *mp4)
{
  AV *compatible_brands = newAV();
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  my_hv_store( mp4->info, "major_brand", newSVpvn( buffer_ptr(mp4->buf), 4 ) );
  buffer_consume(mp4->buf, 4);
  
  my_hv_store( mp4->info, "minor_version", newSVuv( buffer_get_int(mp4->buf) ) );
  
  mp4->rsize -= 8;
  
  if (mp4->rsize % 4) {
    // invalid ftyp
    return 0;
  }
  
  while (mp4->rsize > 0) {
    av_push( compatible_brands, newSVpvn( buffer_ptr(mp4->buf), 4 ) );
    buffer_consume(mp4->buf, 4);
    mp4->rsize -= 4;
  }
    
  my_hv_store( mp4->info, "compatible_brands", newRV_noinc( (SV *)compatible_brands ) );
  
  return 1;
}

uint8_t
_mp4_parse_mvhd(mp4info *mp4)
{
  uint32_t timescale;
  uint8_t version;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  version = buffer_get_char(mp4->buf);
  buffer_consume(mp4->buf, 3); // flags
  
  if (version == 0) { // 32-bit values
    // Skip ctime and mtime
    buffer_consume(mp4->buf, 8);
    
    timescale = buffer_get_int(mp4->buf);
    my_hv_store( mp4->info, "mv_timescale", newSVuv(timescale) );
    
    my_hv_store( mp4->info, "song_length_ms", newSVuv( (buffer_get_int(mp4->buf) * 1.0 / timescale ) * 1000 ) );
  }
  else if (version == 1) { // 64-bit values
    // Skip ctime and mtime
    buffer_consume(mp4->buf, 16);
    
    timescale = buffer_get_int(mp4->buf);
    my_hv_store( mp4->info, "mv_timescale", newSVuv(timescale) );
    
    my_hv_store( mp4->info, "song_length_ms", newSVuv( (buffer_get_int64(mp4->buf) * 1.0 / timescale ) * 1000 ) );
  }
  else {
    return 0;
  }
    
  // Skip rest
  buffer_consume(mp4->buf, 80);
    
  return 1;
}

uint8_t
_mp4_parse_tkhd(mp4info *mp4)
{
  AV *tracks = (AV *)SvRV( *(my_hv_fetch(mp4->info, "tracks")) );
  HV *trackinfo = newHV();
  uint32_t id;
  double width;
  double height;
  uint8_t version;
  
  uint32_t timescale = SvIV( *(my_hv_fetch(mp4->info, "mv_timescale")) );
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  version = buffer_get_char(mp4->buf);
  buffer_consume(mp4->buf, 3); // flags
  
  if (version == 0) { // 32-bit values
    // Skip ctime and mtime
    buffer_consume(mp4->buf, 8);
    
    id = buffer_get_int(mp4->buf);
    
    my_hv_store( trackinfo, "id", newSVuv(id) );
    
    // Skip reserved
    buffer_consume(mp4->buf, 4);
    
    my_hv_store( trackinfo, "duration", newSVuv( (buffer_get_int(mp4->buf) * 1.0 / timescale ) * 1000 ) );
  }
  else if (version == 1) { // 64-bit values
    // Skip ctime and mtime
    buffer_consume(mp4->buf, 16);
    
    id = buffer_get_int(mp4->buf);
    
    my_hv_store( trackinfo, "id", newSVuv(id) );
    
    // Skip reserved
    buffer_consume(mp4->buf, 4);
    
    my_hv_store( trackinfo, "duration", newSVuv( (buffer_get_int64(mp4->buf) * 1.0 / timescale ) * 1000 ) );
  }
  else {
    return 0;
  }
  
  // Skip reserved, layer, alternate_group, volume, reserved, matrix
  buffer_consume(mp4->buf, 52);
  
  // width/height are fixed-point 16.16
  width = buffer_get_short(mp4->buf);
  width += buffer_get_short(mp4->buf) / 65536.;
  if (width > 0) {
    my_hv_store( trackinfo, "width", newSVnv(width) );
  }
  
  height = buffer_get_short(mp4->buf);
  height += buffer_get_short(mp4->buf) / 65536.;
  if (height > 0) {
    my_hv_store( trackinfo, "height", newSVnv(height) );
  }
  
  av_push( tracks, newRV_noinc( (SV *)trackinfo ) );
  
  // Remember the current track we're dealing with
  mp4->current_track = id;
  
  return 1;
}

uint8_t
_mp4_parse_mdhd(mp4info *mp4)
{
  uint32_t timescale;
  uint8_t version;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  version = buffer_get_char(mp4->buf);
  buffer_consume(mp4->buf, 3); // flags
  
  if (version == 0) { // 32-bit values
    // Skip ctime and mtime
    buffer_consume(mp4->buf, 8);
    
    timescale = buffer_get_int(mp4->buf);
    my_hv_store( mp4->info, "samplerate", newSVuv(timescale) );
    
    my_hv_store( mp4->info, "song_length_ms", newSVuv( (buffer_get_int(mp4->buf) * 1.0 / timescale ) * 1000 ) );
  }
  else if (version == 1) { // 64-bit values
    // Skip ctime and mtime
    buffer_consume(mp4->buf, 16);
    
    timescale = buffer_get_int(mp4->buf);
    my_hv_store( mp4->info, "samplerate", newSVuv(timescale) );
    
    my_hv_store( mp4->info, "song_length_ms", newSVuv( (buffer_get_int64(mp4->buf) * 1.0 / timescale ) * 1000 ) );
  }
  else {
    return 0;
  }
    
  // Skip rest
  buffer_consume(mp4->buf, 4);
    
  return 1;
}

uint8_t
_mp4_parse_hdlr(mp4info *mp4)
{
  HV *trackinfo = _mp4_get_current_trackinfo(mp4);
  SV *handler_name;
  
  if (!trackinfo) {
    return 0;
  }
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version, flags, pre_defined
  buffer_consume(mp4->buf, 8);
  
  my_hv_store( trackinfo, "handler_type", newSVpvn( buffer_ptr(mp4->buf), 4 ) );
  buffer_consume(mp4->buf, 4);
  
  // Skip reserved
  buffer_consume(mp4->buf, 12);
  
  handler_name = newSVpv( buffer_ptr(mp4->buf), 0 );
  sv_utf8_decode(handler_name);
  my_hv_store( trackinfo, "handler_name", handler_name );
  
  buffer_consume(mp4->buf, mp4->rsize - 24);
  
  return 1;
}

uint8_t
_mp4_parse_stsd(mp4info *mp4)
{
  uint32_t entry_count;
  
  if ( !_check_buf(mp4->infile, mp4->buf, 8, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  entry_count = buffer_get_int(mp4->buf);
  
  return 1;
}

uint8_t
_mp4_parse_mp4a(mp4info *mp4)
{
  HV *trackinfo = _mp4_get_current_trackinfo(mp4);
  
  if ( !_check_buf(mp4->infile, mp4->buf, 28, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  my_hv_store( trackinfo, "encoding", newSVpvn("mp4a", 4) );
  
  // Skip reserved
  buffer_consume(mp4->buf, 16);
  
  my_hv_store( trackinfo, "channels", newSVuv( buffer_get_short(mp4->buf) ) );
  my_hv_store( trackinfo, "bits_per_sample", newSVuv( buffer_get_short(mp4->buf) ) );
  
  // Skip reserved
  buffer_consume(mp4->buf, 4);
  
  // Skip bogus samplerate
  buffer_consume(mp4->buf, 2);
  
  // Skip reserved
  buffer_consume(mp4->buf, 2);
  
  return 1;
}

uint8_t
_mp4_parse_esds(mp4info *mp4)
{
  HV *trackinfo = _mp4_get_current_trackinfo(mp4);
  uint32_t len = 0;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  // Public docs on esds are hard to find, this is based on faad
  // and http://www.geocities.com/xhelmboyx/quicktime/formats/mp4-layout.txt
  
  // verify ES_DescrTag
  if (buffer_get_char(mp4->buf) == 0x03) {
    // read length
    if ( _mp4_descr_length(mp4->buf) < 5 + 15 ) {
      return 0;
    }
    
    // skip 3 bytes
    buffer_consume(mp4->buf, 3);
  }
  else {
    // skip 2 bytes
    buffer_consume(mp4->buf, 2);
  }
  
  // verify DecoderConfigDescrTab
  if (buffer_get_char(mp4->buf) != 0x04) {
    return 0;
  }
  
  // read length
  if ( _mp4_descr_length(mp4->buf) < 13 ) {
    return 0;
  }
  
  // XXX: map to string
  my_hv_store( trackinfo, "audio_type", newSVuv( buffer_get_char(mp4->buf) ) );
  
  buffer_consume(mp4->buf, 4);
  
  my_hv_store( trackinfo, "max_bitrate", newSVuv( buffer_get_int(mp4->buf) ) );
  my_hv_store( trackinfo, "avg_bitrate", newSVuv( buffer_get_int(mp4->buf) ) );
  
  // verify DecSpecificInfoTag
  if (buffer_get_char(mp4->buf) != 0x05) {
    return 0;
  }
  
  // Read to end of box
  len = _mp4_descr_length(mp4->buf);
  buffer_consume(mp4->buf, len);
  
  // verify SL config descriptor type tag
  if (buffer_get_char(mp4->buf) != 0x06) {
    return 0;
  }
  
  _mp4_descr_length(mp4->buf);
  
  // verify SL value
  if (buffer_get_char(mp4->buf) != 0x02) {
    return 0;
  }
  
  return 1;
}

uint8_t
_mp4_parse_stts(mp4info *mp4)
{
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  mp4->num_time_to_samples = buffer_get_int(mp4->buf);
  DEBUG_TRACE("  num_time_to_samples %d\n", mp4->num_time_to_samples);
  
  Newx(
    mp4->time_to_sample,
    mp4->num_time_to_samples * sizeof(*mp4->time_to_sample),
    struct tts
  );
  
  if ( !mp4->time_to_sample ) {
    PerlIO_printf(PerlIO_stderr(), "Unable to parse stts: too large\n");
    return 0;
  }
  
  for (i = 0; i < mp4->num_time_to_samples; i++) {
    mp4->time_to_sample[i].sample_count    = buffer_get_int(mp4->buf);
    mp4->time_to_sample[i].sample_duration = buffer_get_int(mp4->buf);
    
    DEBUG_TRACE(
      "  sample_count %d sample_duration %d\n",
      mp4->time_to_sample[i].sample_count,
      mp4->time_to_sample[i].sample_duration
    );
  }
  
  return 1;
}

uint8_t
_mp4_parse_stsc(mp4info *mp4)
{
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  mp4->num_sample_to_chunks = buffer_get_int(mp4->buf);
  DEBUG_TRACE("  num_sample_to_chunks %d\n", mp4->num_sample_to_chunks);
  
  Newx(
    mp4->sample_to_chunk,
    mp4->num_sample_to_chunks * sizeof(*mp4->sample_to_chunk),
    struct stc
  );
  
  if ( !mp4->sample_to_chunk ) {
    PerlIO_printf(PerlIO_stderr(), "Unable to parse stsc: too large\n");
    return 0;
  }
  
  for (i = 0; i < mp4->num_sample_to_chunks; i++) {
    mp4->sample_to_chunk[i].first_chunk = buffer_get_int(mp4->buf);
    mp4->sample_to_chunk[i].num_samples = buffer_get_int(mp4->buf);
    
    // Skip sample desc index
    buffer_consume(mp4->buf, 4);
    
    DEBUG_TRACE("  first_chunk %d num_samples %d\n",
      mp4->sample_to_chunk[i].first_chunk,
      mp4->sample_to_chunk[i].num_samples
    );
  }
  
  return 1;
}

uint8_t
_mp4_parse_stsz(mp4info *mp4)
{
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  // Check sample size is 0
  if ( buffer_get_int(mp4->buf) != 0 ) {
    DEBUG_TRACE("  stsz uses fixed sample size\n");
    buffer_consume(mp4->buf, 4);
    return 1;
  }
  
  mp4->num_sample_byte_sizes = buffer_get_int(mp4->buf);
  
  DEBUG_TRACE("  num_sample_byte_sizes %d\n", mp4->num_sample_byte_sizes);
  
  Newx(
    mp4->sample_byte_size,
    mp4->num_sample_byte_sizes * sizeof(*mp4->sample_byte_size),
    uint16_t
  );
  
  if ( !mp4->sample_byte_size ) {
    PerlIO_printf(PerlIO_stderr(), "Unable to parse stsz: too large\n");
    return 0;
  }
  
  for (i = 0; i < mp4->num_sample_byte_sizes; i++) {
    uint32_t v = buffer_get_int(mp4->buf);
    
    if (v > 0x0000ffff) {
      DEBUG_TRACE("stsz[%d] > 65 kB (%ld)\n", i, (long)v);
      return 0;
    }
    
    mp4->sample_byte_size[i] = v;
    
    DEBUG_TRACE("  sample_byte_size %d\n", v);
  }
  
  return 1;
}

uint8_t
_mp4_parse_stco(mp4info *mp4)
{
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  mp4->num_chunk_offsets = buffer_get_int(mp4->buf);
  DEBUG_TRACE("  num_chunk_offsets %d\n", mp4->num_chunk_offsets);
      
  Newx(
    mp4->chunk_offset,
    mp4->num_chunk_offsets * sizeof(*mp4->chunk_offset),
    uint32_t
  );
  
  if ( !mp4->chunk_offset ) {
    PerlIO_printf(PerlIO_stderr(), "Unable to parse stco: too large\n");
    return 0;
  }
  
  for (i = 0; i < mp4->num_chunk_offsets; i++) {
    mp4->chunk_offset[i] = buffer_get_int(mp4->buf);
    
    DEBUG_TRACE("  chunk_offset %d\n", mp4->chunk_offset[i]);
  }
  
  return 1;
}

uint8_t
_mp4_parse_meta(mp4info *mp4)
{
  uint32_t hdlr_size;
  char type[5];
  
  if ( !_check_buf(mp4->infile, mp4->buf, 12, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  // Parse/skip meta version of hdlr
  hdlr_size = buffer_get_int(mp4->buf);
  strncpy( type, (char *)buffer_ptr(mp4->buf), 4 );
  type[4] = '\0';
  buffer_consume(mp4->buf, 4);
  
  if ( !FOURCC_EQ(type, "hdlr") ) {
    return 0;
  }
  
  // Skip rest of hdlr
  if ( !_check_buf(mp4->infile, mp4->buf, hdlr_size - 8, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  buffer_consume(mp4->buf, hdlr_size - 8);  
  
  return 12 + hdlr_size - 8;
}

uint8_t
_mp4_parse_ilst(mp4info *mp4)
{
  while (mp4->rsize) {
    uint32_t size;
    char key[5];
    
    if ( !_check_buf(mp4->infile, mp4->buf, 8, MP4_BLOCK_SIZE) ) {
      return 0;
    }
    
    DEBUG_TRACE("  ilst rsize %d\n", mp4->rsize);
    
    // Read Apple annotation box
    size = buffer_get_int(mp4->buf);
    strncpy( key, (char *)buffer_ptr(mp4->buf), 4 );
    key[4] = '\0';
    buffer_consume(mp4->buf, 4);
    
    DEBUG_TRACE("  %s size %d\n", key, size);
    
    if ( !_check_buf(mp4->infile, mp4->buf, size - 8, MP4_BLOCK_SIZE) ) {
      return 0;
    }
    
    upcase(key);
    
    if ( FOURCC_EQ(key, "----") ) {
      // user-specified key/value pair
      if ( !_mp4_parse_ilst_custom(mp4, size - 8) ) {
        return 0;
      }
    }
    else {
      // Verify data box
      uint32_t bsize = buffer_get_int(mp4->buf);
      
      DEBUG_TRACE("    box size %d\n", bsize);
      
      // Sanity check for bad data size
      if ( bsize <= size - 8 ) {
        SV *skey;
        
        char *bptr = buffer_ptr(mp4->buf);
        if ( !FOURCC_EQ(bptr, "data") ) {
          return 0;
        }
      
        buffer_consume(mp4->buf, 4);
        
        skey = newSVpv(key, 0);
      
        if ( !_mp4_parse_ilst_data(mp4, bsize - 8, skey) ) {
          SvREFCNT_dec(skey);
          return 0;
        }
        
        SvREFCNT_dec(skey);
        
        // XXX: bug 14476, files with multiple COVR images aren't handled here, just skipped for now
        if ( bsize < size - 8 ) {
          DEBUG_TRACE("    skipping rest of box, %d\n", size - 8 - bsize );
          buffer_consume(mp4->buf, size - 8 - bsize);
        }
      }
      else {
        DEBUG_TRACE("    invalid data size %d, skipping value\n", bsize);
        
        buffer_consume(mp4->buf, size - 12);
      }
    }
    
    mp4->rsize -= size;
  }
  
  return 1;
}

uint8_t
_mp4_parse_ilst_data(mp4info *mp4, uint32_t size, SV *key)
{
  uint32_t flags;

  // Version(0) + Flags
  flags = buffer_get_int(mp4->buf);

  // Skip reserved
  buffer_consume(mp4->buf, 4);

  DEBUG_TRACE("      flags %d\n", flags);
  
  if ( !flags || flags == 21 ) {
    if ( FOURCC_EQ( SvPVX(key), "TRKN" ) || FOURCC_EQ( SvPVX(key), "DISK" ) ) {
      // Special case trkn, disk (pair of 16-bit ints)
      uint16_t num, total;
    
      buffer_consume(mp4->buf, 2); // padding
    
      num   = buffer_get_short(mp4->buf);
      total = buffer_get_short(mp4->buf);
    
      buffer_consume(mp4->buf, size - 14); // optional padding
    
      if (total) {
        my_hv_store_ent( mp4->tags, key, newSVpvf( "%d/%d", num, total ) );
      }
      else if (num) {
        my_hv_store_ent( mp4->tags, key, newSVuv(num) );
      }
    }
    else if ( FOURCC_EQ( SvPVX(key), "GNRE" ) ) {
      // Special case genre, 16-bit int as id3 genre code
      char *genre_string;
      uint16_t genre_num = buffer_get_short(mp4->buf);
    
      if (genre_num > 0 && genre_num < 148) {
        genre_string = (char *)id3_ucs4_utf8duplicate( id3_genre_index(genre_num - 1) );
        my_hv_store_ent( mp4->tags, key, newSVpv( genre_string, 0 ) );
        free(genre_string);
      }
    }
    else {
      // Other binary type, try to guess type based on size
      SV *data;
      uint32_t dsize = size - 8;
      
      if (dsize == 1) {
        data = newSVuv( buffer_get_char(mp4->buf) );
      }
      else if (dsize == 2) {
        data = newSVuv( buffer_get_short(mp4->buf) );
      }
      else if (dsize == 4) {
        data = newSVuv( buffer_get_int(mp4->buf) );
      }
      else if (dsize == 8) {
        data = newSVuv( buffer_get_int64(mp4->buf) );
      }
      else {
        data = newSVpvn( buffer_ptr(mp4->buf), dsize );
        buffer_consume(mp4->buf, dsize);
      }
      
      // if key exists, create array
      if ( my_hv_exists_ent( mp4->tags, key ) ) {
        SV **entry = my_hv_fetch( mp4->tags, SvPVX(key) );
        if (entry != NULL) {
          if ( SvROK(*entry) && SvTYPE(SvRV(*entry)) == SVt_PVAV ) {
            av_push( (AV *)SvRV(*entry), data );
          }
          else {
            // A non-array entry, convert to array.
            AV *ref = newAV();
            av_push( ref, newSVsv(*entry) );
            av_push( ref, data );
            my_hv_store_ent( mp4->tags, key, newRV_noinc( (SV*)ref ) );
          }
        }
      }
      else {
        my_hv_store_ent( mp4->tags, key, data );
      }
    }
  }
  else { // text data
    char *ckey = SvPVX(key);
    SV *value = newSVpvn( buffer_ptr(mp4->buf), size - 8 );
    sv_utf8_decode(value);
    
    // strip copyright symbol 0xA9 out of key
    if ( ckey[0] == -87 ) {
      ckey++;
    }
    
    DEBUG_TRACE("      %s = %s\n", ckey, SvPVX(value));
    
    // if key exists, create array
    if ( my_hv_exists( mp4->tags, ckey ) ) {
      SV **entry = my_hv_fetch( mp4->tags, ckey );
      if (entry != NULL) {
        if ( SvROK(*entry) && SvTYPE(SvRV(*entry)) == SVt_PVAV ) {
          av_push( (AV *)SvRV(*entry), value );
        }
        else {
          // A non-array entry, convert to array.
          AV *ref = newAV();
          av_push( ref, newSVsv(*entry) );
          av_push( ref, value );
          my_hv_store( mp4->tags, ckey, newRV_noinc( (SV*)ref ) );
        }
      }
    }
    else {
      my_hv_store( mp4->tags, ckey, value );  
    }
    
    buffer_consume(mp4->buf, size - 8);
  }
  
  return 1;
} 

uint8_t
_mp4_parse_ilst_custom(mp4info *mp4, uint32_t size)
{
  SV *key = NULL;
  
  while (size) {
    char type[5];
    uint32_t bsize;
    
    // Read box
    bsize = buffer_get_int(mp4->buf);
    strncpy( type, (char *)buffer_ptr(mp4->buf), 4 );
    type[4] = '\0';
    buffer_consume(mp4->buf, 4);
    
    DEBUG_TRACE("    %s size %d\n", type, bsize);
    
    if ( FOURCC_EQ(type, "name") ) {
      buffer_consume(mp4->buf, 4); // padding
      key = newSVpvn( buffer_ptr(mp4->buf), bsize - 12);
      sv_utf8_decode(key);
      upcase(SvPVX(key));
      buffer_consume(mp4->buf, bsize - 12);
      
      DEBUG_TRACE("      %s\n", SvPVX(key));
    }
    else if ( FOURCC_EQ(type, "data") ) {
      if (!key) {
        // No key yet, data is out of order
        return 0;
      }
      
      if ( !_mp4_parse_ilst_data(mp4, bsize - 8, key) ) {
        SvREFCNT_dec(key);
        return 0;
      }
    }
    else {
      // skip (mean, or other boxes)
      buffer_consume(mp4->buf, bsize - 8);
    }
    
    size -= bsize;
  }
  
  SvREFCNT_dec(key);
  
  return 1;
}

HV *
_mp4_get_current_trackinfo(mp4info *mp4)
{
  // Return the trackinfo hash for track id == mp4->current_track
  AV *tracks;
  HV *trackinfo;
  int i;
  
  SV **entry = my_hv_fetch(mp4->info, "tracks");
  if (entry != NULL) {
    tracks = (AV *)SvRV(*entry);
  }
  else {
    return NULL;
  }

  // Find entry for this stream number
  for (i = 0; av_len(tracks) >= 0 && i <= av_len(tracks); i++) {
    SV **info = av_fetch(tracks, i, 0);
    if (info != NULL) {
      SV **tid;
      
      trackinfo = (HV *)SvRV(*info);        
      tid = my_hv_fetch( trackinfo, "id" );
      if (tid != NULL) {
        if ( SvIV(*tid) == mp4->current_track ) {
          return trackinfo;
        }
      }
    }
  }
  
  return NULL;
}

uint32_t
_mp4_descr_length(Buffer *buf)
{
  uint8_t b;
  uint8_t num_bytes = 0;
  uint32_t length = 0;
  
  do {
    b = buffer_get_char(buf);
    num_bytes++;
    length = (length << 7) | (b & 0x7f);
  } while ( (b & 0x80) && num_bytes < 4 );
  
  return length;
}
