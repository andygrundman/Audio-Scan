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

#include "wavpack.h"

static int
get_wavpack_info(PerlIO *infile, char *file, HV *info)
{
  wvpinfo *wvp = _wavpack_parse(infile, file, info, 0);
  
  Safefree(wvp);
  
  return 0;
}

wvpinfo *
_wavpack_parse(PerlIO *infile, char *file, HV *info, uint8_t seeking)
{
  int err = 0;
  int done = 0;
  u_char *bptr;
  
  wvpinfo *wvp;
  Newz(0, wvp, sizeof(wvpinfo), wvpinfo);
  Newz(0, wvp->buf, sizeof(Buffer), Buffer);
  Newz(0, wvp->header, sizeof(WavpackHeader), WavpackHeader);
  
  wvp->infile         = infile;
  wvp->file           = file;
  wvp->info           = info;
  wvp->file_offset    = 0;
  wvp->audio_offset   = 0;
  wvp->seeking        = seeking ? 1 : 0;
  
  buffer_init(wvp->buf, WAVPACK_BLOCK_SIZE);
  
  wvp->file_size = _file_size(infile);
  my_hv_store( info, "file_size", newSVuv(wvp->file_size) );
  
  // Loop through each wvpk block until we find a good one
  while (!done) {
    if ( !_check_buf(infile, wvp->buf, 32, WAVPACK_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    // XXX If first byte is 'R', assume old version
    /*
    if ( bptr[0] == 'R' ) {
      _wavpack_parse_v3(wvp);
      break;
    }
    */
    
    // May need to read past some junk before wvpk header
    bptr = buffer_ptr(wvp->buf);
    while ( bptr[0] != 'w' || bptr[1] != 'v' || bptr[2] != 'p' || bptr[3] != 'k' ) {
      buffer_consume(wvp->buf, 1);
     
      wvp->audio_offset++;

      if ( !buffer_len(wvp->buf) ) {
        if ( !_check_buf(infile, wvp->buf, 32, WAVPACK_BLOCK_SIZE) ) {
          PerlIO_printf(PerlIO_stderr(), "Unable to find a valid WavPack block in file: %s\n", file);
          err = -1;
          goto out;
        }
      }
      
      bptr = buffer_ptr(wvp->buf);
    }
    
    if ( _wavpack_parse_block(wvp) ) {
      done = 1;
    }
  }
  
  my_hv_store( info, "audio_offset", newSVuv(wvp->audio_offset) );
  
out:
  buffer_free(wvp->buf);
  Safefree(wvp->buf);
  Safefree(wvp->header);

  return wvp;
}

int
_wavpack_parse_block(wvpinfo *wvp)
{
  unsigned char *bptr;
  uint16_t remaining;
  
  bptr = buffer_ptr(wvp->buf);
  
  // Verify wvpk signature
  if ( bptr[0] != 'w' || bptr[1] != 'v' || bptr[2] != 'p' || bptr[3] != 'k' ) {
    DEBUG_TRACE("Invalid wvpk header at %llu\n", wvp->file_offset);
    return 1;
  }
  
  buffer_consume(wvp->buf, 4);
  
  wvp->header->ckSize        = buffer_get_int_le(wvp->buf);
  wvp->header->version       = buffer_get_short_le(wvp->buf);
  wvp->header->track_no      = buffer_get_char(wvp->buf);
  wvp->header->index_no      = buffer_get_char(wvp->buf);
  wvp->header->total_samples = buffer_get_int_le(wvp->buf);
  wvp->header->block_index   = buffer_get_int_le(wvp->buf);
  wvp->header->block_samples = buffer_get_int_le(wvp->buf);
  wvp->header->flags         = buffer_get_int_le(wvp->buf);
  wvp->header->crc           = buffer_get_int_le(wvp->buf);
  
  DEBUG_TRACE("wvpk header @ %llu:\n", wvp->file_offset);
  DEBUG_TRACE("  size: %u\n", wvp->header->ckSize);
  DEBUG_TRACE("  version: 0x%x\n", wvp->header->version);
  DEBUG_TRACE("  track_no: 0x%x\n", wvp->header->track_no);
  DEBUG_TRACE("  index_no: 0x%x\n", wvp->header->index_no);
  DEBUG_TRACE("  total_samples: %u\n", wvp->header->total_samples);
  DEBUG_TRACE("  block_index: %u\n", wvp->header->block_index);
  DEBUG_TRACE("  block_samples: %u\n", wvp->header->block_samples);
  DEBUG_TRACE("  flags: 0x%x\n", wvp->header->flags);
  DEBUG_TRACE("  crc: 0x%x\n", wvp->header->crc);
  
  wvp->file_offset += 32;
  
  my_hv_store( wvp->info, "encoder_version", newSVuv(wvp->header->version) );
  
  if (wvp->header->version < 0x4) {
    // XXX old version and not handled by 'R' check above for v3
    PerlIO_printf(PerlIO_stderr(), "Unsupported old WavPack version: 0x%x\n", wvp->header->version);
    return 1;
  }
  
  // Read data from flags
  my_hv_store( wvp->info, "bits_per_sample", newSVuv( 8 * ((wvp->header->flags & 0x3) + 1) ) );

  // Encoding mode
  my_hv_store( wvp->info, (wvp->header->flags & 0x8) ? "hybrid" : "lossless", newSVuv(1) );
  
  {
    // samplerate, may be overridden by a later ID_SAMPLE_RATE metadata block
    uint32_t samplerate_index = (wvp->header->flags & 0x7800000) >> 23;
    if ( samplerate_index >= 0 && samplerate_index < 0xF ) {
      my_hv_store( wvp->info, "samplerate", newSVuv( wavpack_sample_rates[samplerate_index] ) );
    }
    else {
      // Default to 44.1 just in case
      my_hv_store( wvp->info, "samplerate", newSVuv(44100) );
    }
  }
  
  // Channels, may be overridden by a later ID_CHANNEL_INFO metadata block
  my_hv_store( wvp->info, "channels", newSVuv( (wvp->header->flags & 0x4) ? 1 : 2 ) );
  
  // Parse metadata sub-blocks
  remaining = wvp->header->ckSize - 24; // ckSize is 8 less than the block size
  
  // If block_samples is 0, we need to skip to the next block
  // XXX need test file
  if ( !wvp->header->block_samples ) {
    wvp->file_offset += remaining;
    _wavpack_skip(wvp, remaining);
    return 0;
  }
  
  while (remaining > 0) {
    // Read sub-block header (2-4 bytes)
    uint8_t id;
    uint32_t size;
    
    if ( !_check_buf(wvp->infile, wvp->buf, 4, WAVPACK_BLOCK_SIZE) ) {
      return 0;
    }
    
    id = buffer_get_char(wvp->buf);
    remaining--;
    
    // Size is in words
    if (id & ID_LARGE) {
      // 24-bit large size
      id &= ~ID_LARGE;
      size = buffer_get_int24_le(wvp->buf) << 1;
      remaining -= 3;
    }
    else {
      // 8-bit size
      size = buffer_get_char(wvp->buf) << 1;
      remaining--;
    }
    
    if (id & ID_ODD_SIZE) {
      id &= ~ID_ODD_SIZE;
      size--;
    }
    
    if ( !size || id & ID_WV_BITSTREAM ) {
      // Found the bitstream, don't read any farther
      DEBUG_TRACE("  Sub-Chunk: WV_BITSTREAM (size %u)\n", size);
      break;
    }
    
    // We only care about 0x27 (ID_SAMPLE_RATE) and 0xd (ID_CHANNEL_INFO)    
    switch (id) {
    case ID_SAMPLE_RATE:
      // XXX need test file
      DEBUG_TRACE("  Sub-Chunk: ID_SAMPLE_RATE (size: %u)\n", size);
      _wavpack_parse_sample_rate(wvp, size);
      break;
      
    case ID_CHANNEL_INFO:
      // XXX need test file
      DEBUG_TRACE("  Sub-Chunk: ID_CHANNEL_INFO (size: %u)\n", size);
      _wavpack_parse_channel_info(wvp, size);
      break;
      
    default: 
      // Skip it
      DEBUG_TRACE("  Sub-Chunk: %x (size: %u) (skipped)\n", id, size);
      _wavpack_skip(wvp, size);
    }
    
    remaining -= size;
  }
  
  // Calculate bitrate
  if ( wvp->header->total_samples && wvp->file_size > 0 ) {
    SV **samplerate = my_hv_fetch( wvp->info, "samplerate" );
    if (samplerate != NULL) {
      uint32_t song_length_ms = (wvp->header->total_samples * 1000) / SvIV(*samplerate);
      my_hv_store( wvp->info, "song_length_ms", newSVuv(song_length_ms) );
      my_hv_store( wvp->info, "bitrate", newSVuv( _bitrate(wvp->file_size - wvp->audio_offset, song_length_ms) ) );
      my_hv_store( wvp->info, "total_samples", newSVuv(wvp->header->total_samples) );
    }
  }
  
  return 1;
}

int
_wavpack_parse_sample_rate(wvpinfo *wvp, uint32_t size)
{
  uint32_t samplerate = buffer_get_int24_le(wvp->buf);
  
  my_hv_store( wvp->info, "samplerate", newSVuv(samplerate) );
  
  return 1;
}

int
_wavpack_parse_channel_info(wvpinfo *wvp, uint32_t size)
{
  uint8_t channels = buffer_get_char(wvp->buf);
  
  my_hv_store( wvp->info, "channels", newSVuv(channels) );
  
  return 1;
}

void
_wavpack_skip(wvpinfo *wvp, uint32_t size)
{
  if ( buffer_len(wvp->buf) >= size ) {
    //buffer_dump(mp4->buf, size);
    buffer_consume(wvp->buf, size);
    
    DEBUG_TRACE("  skipped buffer data size %d\n", size);
  }
  else {
    PerlIO_seek(wvp->infile, size - buffer_len(wvp->buf), SEEK_CUR);
    buffer_clear(wvp->buf);
    
    DEBUG_TRACE("  seeked past %d bytes to %d\n", size, (int)PerlIO_tell(wvp->infile));
  }
}