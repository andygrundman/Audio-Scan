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
 
 find_frame:
   Requires parsing of: elst, stts, stss, stsc, stsz, stco
   See section A.7 page 101

 Only parse boxes needed for seeking during find_frame, store
 in internal data structure, not Perl.
   
*/

#include "mp4.h"

static int
get_mp4tags(PerlIO *infile, char *file, HV *info, HV *tags)
{
  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to audio
  uint32_t box_size = 0;
  
  int err = 0;
  
  mp4info *mp4;
  Newxz(mp4, sizeof(mp4info), mp4info);
  Newxz(mp4->buf, sizeof(Buffer), Buffer);
  
  mp4->infile        = infile;
  mp4->file          = file;
  mp4->info          = info;
  mp4->tags          = tags;
  mp4->current_track = 0;
  
  buffer_init(mp4->buf, MP4_BLOCK_SIZE);
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  my_hv_store( info, "file_size", newSVuv(file_size) );
  
  // Create empty tracks array
  my_hv_store( info, "tracks", newRV_noinc( (SV *)newAV() ) );
  
  while ( (box_size = _mp4_read_box(mp4)) > 0 ) {
    audio_offset += box_size;
    DEBUG_TRACE("read box of size %d / audio_offset %d\n", box_size, audio_offset);
  }
  
  my_hv_store( info, "audio_offset", newSVuv(audio_offset) );
  
  DEBUG_TRACE("audio_offset: %d\n", audio_offset);
  
  // XXX: if no ftyp was found, assume it is brand 'mp41'
  
  // if no bitrate was found (i.e. ALAC), calculate based on file_size/song_length_ms
  if (mp4->need_calc_bitrate) {
    HV *trackinfo = _mp4_get_current_trackinfo(mp4);
    SV **entry = my_hv_fetch(info, "song_length_ms");
    if (entry) {
      uint32_t song_length_ms = SvIV(*entry);
      uint32_t bitrate = ((file_size - audio_offset * 1.0) / song_length_ms) * 1000;
      
      my_hv_store( trackinfo, "avg_bitrate", newSVuv(bitrate) );
    }
  }
  
//out:
  buffer_free(mp4->buf);
  Safefree(mp4->buf);
  Safefree(mp4);

  if (err) return err;

  return 0;
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
    // Audio data here
    return 0;
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
    my_hv_store( mp4->info, "timescale", newSVuv(timescale) );
    
    my_hv_store( mp4->info, "song_length_ms", newSVuv( (buffer_get_int(mp4->buf) * 1.0 / timescale ) * 1000 ) );
  }
  else if (version == 1) { // 64-bit values
    // Skip ctime and mtime
    buffer_consume(mp4->buf, 16);
    
    timescale = buffer_get_int(mp4->buf);
    my_hv_store( mp4->info, "timescale", newSVuv(timescale) );
    
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
  
  uint32_t timescale = SvIV( *(my_hv_fetch(mp4->info, "timescale")) );
  
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
    
    my_hv_store( trackinfo, "track_length_ms", newSVuv( (buffer_get_int64(mp4->buf) * 1.0 / timescale ) * 1000 ) );
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
  
  my_hv_store( trackinfo, "samplerate", newSVuv( buffer_get_short(mp4->buf) ) );
  
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
  uint32_t entry_count;
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  entry_count = buffer_get_int(mp4->buf);
  DEBUG_TRACE("  XXX entry_count %d\n", entry_count);
  
  for (i = 0; i < entry_count; i++) {
    // XXX store in internal data structure
    /*
    uint32_t sample_count = buffer_get_int(mp4->buf);
    uint32_t sample_delta = buffer_get_int(mp4->buf);
    //DEBUG_TRACE("  XXX sample_count %d sample_delta %d\n", sample_count, sample_delta);
    */
  }
  
  return 1;
}

uint8_t
_mp4_parse_stsc(mp4info *mp4)
{
  uint32_t entry_count;
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  entry_count = buffer_get_int(mp4->buf);
  DEBUG_TRACE("  XXX entry_count %d\n", entry_count);
  
  for (i = 0; i < entry_count; i++) {
    // XXX store in internal data structure
    /*
    uint32_t first_chunk       = buffer_get_int(mp4->buf);
    uint32_t samples_per_chunk = buffer_get_int(mp4->buf);
    uint32_t sample_desc_index = buffer_get_int(mp4->buf);
    //DEBUG_TRACE("  XXX first_chunk %d samples_per_chunk %d sample_desc_index %d\n",
    //  first_chunk, samples_per_chunk, sample_desc_index);
    */
  }
  
  return 1;
}

uint8_t
_mp4_parse_stsz(mp4info *mp4)
{
  uint32_t sample_size;
  uint32_t sample_count;
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  sample_size  = buffer_get_int(mp4->buf);
  sample_count = buffer_get_int(mp4->buf);
  
  if (sample_size == 0) {
    DEBUG_TRACE("  XXX sample_count %d\n", sample_count);
    for (i = 0; i < sample_count; i++) {
      /*
      uint32_t entry_size = buffer_get_int(mp4->buf);
      //DEBUG_TRACE("  XXX entry_size %d\n", entry_size);
      */
    }
  }
  else {
    DEBUG_TRACE("  XXX sample_size %d\n", sample_size);
  }
  
  return 1;
}

uint8_t
_mp4_parse_stco(mp4info *mp4)
{
  uint32_t entry_count;
  int i;
  
  if ( !_check_buf(mp4->infile, mp4->buf, mp4->rsize, MP4_BLOCK_SIZE) ) {
    return 0;
  }
  
  // Skip version/flags
  buffer_consume(mp4->buf, 4);
  
  entry_count = buffer_get_int(mp4->buf);
  DEBUG_TRACE("  XXX entry_count %d\n", entry_count);
  
  for (i = 0; i < entry_count; i++) {
    /*
    uint32_t chunk_offset = buffer_get_int(mp4->buf);
    //DEBUG_TRACE("  XXX chunk_offset %d\n", chunk_offset);
    */
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
      
      char *bptr = buffer_ptr(mp4->buf);
      if ( !FOURCC_EQ(bptr, "data") ) {
        return 0;
      }
      
      buffer_consume(mp4->buf, 4);
      
      if ( !_mp4_parse_ilst_data(mp4, bsize - 8, newSVpv(key, 0)) ) {
        return 0;
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

  DEBUG_TRACE("    flags %d\n", flags);

  // XXX store multiple values as array
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
      
      my_hv_store_ent( mp4->tags, key, data );
    }
  }
  else { // text data
    SV *value = newSVpvn( buffer_ptr(mp4->buf), size - 8 );
    sv_utf8_decode(value);
  
    // strip copyright symbol 0xA9 out of key
    if ( SvPVX(key)[0] == -87 ) {
      my_hv_store( mp4->tags, SvPVX(key) + 1, value );
    }
    else {
      my_hv_store_ent( mp4->tags, key, value );
    }
    buffer_consume(mp4->buf, size - 8);
  }
  
  SvREFCNT_dec(key);
  
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
        return 0;
      }
    }
    else {
      // skip (mean, or other boxes)
      buffer_consume(mp4->buf, bsize - 8);
    }
    
    size -= bsize;
  }
  
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
