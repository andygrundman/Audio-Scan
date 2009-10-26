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
 * This file is derived from mt-daap project.
 */

#include "mp3.h"

static int
get_mp3tags(PerlIO *infile, char *file, HV *info, HV *tags)
{
  int ret;
  
  // See if this file has an APE tag as fast as possible
  // This is still a big performance hit :(
  if ( _has_ape(infile) ) {
    get_ape_metadata(infile, file, info, tags);
  }
  
  ret = parse_id3(infile, file, info, tags, 0);

  return ret;
}

static int
_has_ape(PerlIO *infile)
{
  Buffer buf;
  uint8_t ret = 0;
  char *bptr;
  
  buffer_init(&buf, 8);
  
  if ( (PerlIO_seek(infile, -160, SEEK_END)) == -1 ) {
    goto out;
  }
  
  DEBUG_TRACE("Seeked to %d looking for APE tag\n", PerlIO_tell(infile));
  
  // Bug 9942, read 136 bytes so we can check at -32 bytes in case file
  // does not have an ID3v1 tag
  if ( !_check_buf(infile, &buf, 136, 136) ) {
    goto out;
  }
  
  bptr = buffer_ptr(&buf);
  
  if ( bptr[0] == 'A' && bptr[1] == 'P' && bptr[2] == 'E'
    && bptr[3] == 'T' && bptr[4] == 'A' && bptr[5] == 'G'
    && bptr[6] == 'E' && bptr[7] == 'X'
  ) {
    DEBUG_TRACE("APE tag found at -160 (with ID3v1)\n");
    ret = 1;
  }
  else {
    // APE tag without ID3v1 tag will be -32 bytes from end
    buffer_consume(&buf, 128);
    
    bptr = buffer_ptr(&buf);

    if ( bptr[0] == 'A' && bptr[1] == 'P' && bptr[2] == 'E'
      && bptr[3] == 'T' && bptr[4] == 'A' && bptr[5] == 'G'
      && bptr[6] == 'E' && bptr[7] == 'X'
    ) {
      DEBUG_TRACE("APE tag found at -32 (no ID3v1)\n");
      ret = 1;
    }
  }
  
out:
  buffer_free(&buf);
  
  return ret;
}

// _decode_mp3_frame
static int
_decode_mp3_frame(unsigned char *frame, struct mp3_frameinfo *pfi)
{
  int ver;
  int layer_index;
  int sample_index;
  int bitrate_index;
  int samplerate_index;

  if ((frame[0] != 0xFF) || (frame[1] < 224)) {
    return -1;
  }

  ver = (frame[1] & 0x18) >> 3;
  if (ver == 1) return -1;
    
  pfi->layer = 4 - ((frame[1] & 0x6) >> 1);
  if (pfi->layer == 4) return -1;
  
  pfi->crc_protected = !(frame[1] & 0x1);

  layer_index = sample_index = -1;

  switch(ver) {
  case 0:
    pfi->mpeg_version = 0x25;			// 2.5
    sample_index = 2;
    if (pfi->layer == 1)
      layer_index = 3;
    if ((pfi->layer == 2) || (pfi->layer == 3))
      layer_index = 4;
	break;
  case 2:
    pfi->mpeg_version = 0x20;			// 2.0
    sample_index = 1;
    if (pfi->layer == 1)
      layer_index = 3;
    if ((pfi->layer == 2) || (pfi->layer == 3))
      layer_index = 4;
    break;
  case 3:
    pfi->mpeg_version = 0x10;			// 1.0
    sample_index = 0;
    if (pfi->layer == 1)
      layer_index = 0;
    if (pfi->layer == 2)
      layer_index = 1;
    if (pfi->layer == 3)
      layer_index = 2;
    break;
  }

  if ((layer_index < 0) || (layer_index > 4)) {
    return -1;
  }

  if ((sample_index < 0) || (sample_index > 2)) {
    return -1;
  }

  if (pfi->layer==1) pfi->samples_per_frame = 384;
  if (pfi->layer==2) pfi->samples_per_frame = 1152;
  if (pfi->layer==3) {
    if (pfi->mpeg_version == 0x10)
      pfi->samples_per_frame = 1152;
    else
      pfi->samples_per_frame = 576;
  }

  bitrate_index = (frame[2] & 0xF0) >> 4;
  samplerate_index = (frame[2] & 0x0C) >> 2;

  if ((bitrate_index == 0xF) || (bitrate_index==0x0)) {
    return -1;
  }

  if (samplerate_index == 3) {
    return -1;
  }

  pfi->bitrate = bitrate_tbl[layer_index][bitrate_index];
  pfi->samplerate = sample_rate_tbl[sample_index][samplerate_index];
  
  // Validate emphasis is not reserved
  if ((frame[3] & 0x3) == 2) return -1;

  if (((frame[3] & 0xC0) >> 6) == 3)
    pfi->stereo = 0;
  else
    pfi->stereo = 1;

  if (frame[2] & 0x02)
    pfi->padding = 1;
  else
    pfi->padding=0;

  if (pfi->mpeg_version == 0x10) {
    if (pfi->stereo)
      pfi->xing_offset = 36;
    else
      pfi->xing_offset = 21;
  }
  else {
    if (pfi->stereo)
      pfi->xing_offset = 21;
    else
      pfi->xing_offset = 13;
  }

  if (pfi->layer == 1)
    pfi->frame_length = (12 * pfi->bitrate * 1000 / pfi->samplerate + pfi->padding) * 4;
  else
    pfi->frame_length = 144 * pfi->bitrate * 1000 / pfi->samplerate + pfi->padding;

  if ((pfi->frame_length > 2880) || (pfi->frame_length <= 0)) {
    return -1;
  }

  return 0;
}

// _mp3_get_average_bitrate
// average bitrate by averaging all the frames in the file.  This used
// to seek to the middle of the file and take a 32K chunk but this was
// found to have bugs if it seeked near invalid FF sync bytes that could
// be detected as a real frame
static short _mp3_get_average_bitrate(mp3info *mp3, uint32_t offset, uint32_t audio_size)
{
  struct mp3_frameinfo fi;
  int frame_count   = 0;
  int bitrate_total = 0;
  int err = 0;
  int done = 0;
  int wrap_skip = 0;
  int prev_bitrate = 0;
  bool vbr = FALSE;

  unsigned char *bptr;
  
  buffer_clear(mp3->buf);

  // Seek to offset
  PerlIO_seek(mp3->infile, 0, SEEK_END);
  PerlIO_seek(mp3->infile, offset, SEEK_SET);
  
  while ( done < audio_size - 4 ) {
    if ( !_check_buf(mp3->infile, mp3->buf, 4, 65536) ) {
      err = -1;
      goto out;
    }
    
    done += buffer_len(mp3->buf);
    
    if (wrap_skip) {
      // Skip rest of frame from last buffer
      DEBUG_TRACE("Wrapped, consuming %d bytes from previous frame\n", wrap_skip);
      buffer_consume(mp3->buf, wrap_skip);
      wrap_skip = 0;
    }
  
    while ( buffer_len(mp3->buf) >= 4 ) {
      bptr = buffer_ptr(mp3->buf);
      while ( *bptr != 0xFF ) {
        buffer_consume(mp3->buf, 1);
      
        if ( !buffer_len(mp3->buf) ) {
          // ran out of data
          goto out;
        }
      
        bptr = buffer_ptr(mp3->buf);
      }

      if ( !_decode_mp3_frame( buffer_ptr(mp3->buf), &fi ) ) {
        // Found a valid frame
        frame_count++;
        bitrate_total += fi.bitrate;
        
        if ( !vbr ) {
          // If we see the bitrate changing, we have a VBR file, and read
          // the entire file.  Otherwise, if we see 20 frames with the same
          // bitrate, assume CBR and stop
          if (prev_bitrate > 0 && prev_bitrate != fi.bitrate) {
            DEBUG_TRACE("Bitrate changed, assuming file is VBR\n");
            vbr = TRUE;
          }
          else {
            if (frame_count > 20) {
              DEBUG_TRACE("Found 20 frames with same bitrate, assuming CBR\n");
              goto out;
            }
            
            prev_bitrate = fi.bitrate;
          }
        }
        
        DEBUG_TRACE("  Frame %d: %dkbps\n", frame_count, fi.bitrate);

        if (fi.frame_length > buffer_len(mp3->buf)) {
          // Partial frame in buffer
          wrap_skip = fi.frame_length - buffer_len(mp3->buf);
          buffer_consume(mp3->buf, buffer_len(mp3->buf));
        }
        else {
          buffer_consume(mp3->buf, fi.frame_length);
        }
      }
      else {
        // Not a valid frame, stray 0xFF
        buffer_consume(mp3->buf, 1);
      }
    }
  }

out:
  if (err) return err;
  
  if (!frame_count) return -1;
  
  DEBUG_TRACE("Average of %d frames: %dkbps\n", frame_count, bitrate_total / frame_count);

  return bitrate_total / frame_count;
}

static int
_parse_xing(mp3info *mp3, struct mp3_frameinfo *pfi)
{
  int i;
  int xing_flags;
  unsigned char *bptr;
  
  if ( !_check_buf(mp3->infile, mp3->buf, 160 + pfi->xing_offset, BLOCK_SIZE) ) {
    return 0;
  }
  
  buffer_consume(mp3->buf, pfi->xing_offset);
  
  bptr = buffer_ptr(mp3->buf);

  if ( bptr[0] == 'X' || bptr[0] == 'I' ) {
    if (
      ( bptr[1] == 'i' && bptr[2] == 'n' && bptr[3] == 'g' )
      ||
      ( bptr[1] == 'n' && bptr[2] == 'f' && bptr[3] == 'o' )
    ) {
      DEBUG_TRACE("Found Xing/Info tag\n");
      
      // It's VBR if tag is Xing, and CBR if Info
      pfi->vbr = bptr[1] == 'i' ? VBR : CBR;

      buffer_consume(mp3->buf, 4);

      xing_flags = buffer_get_int(mp3->buf);

      if (xing_flags & XING_FRAMES) {
        pfi->xing_frames = buffer_get_int(mp3->buf);
      }

      if (xing_flags & XING_BYTES) {
        pfi->xing_bytes = buffer_get_int(mp3->buf);
      }

      if (xing_flags & XING_TOC) {
        uint8_t i;
        bptr = buffer_ptr(mp3->buf);
        for (i = 0; i < 100; i++) {
          pfi->xing_toc[i] = bptr[i];
        }
        
        pfi->xing_has_toc = 1;
        
        buffer_consume(mp3->buf, 100);
      }

      if (xing_flags & XING_QUALITY) {
        pfi->xing_quality = buffer_get_int(mp3->buf);
      }

      // LAME tag
      bptr = buffer_ptr(mp3->buf);
      if ( bptr[0] == 'L' && bptr[1] == 'A' && bptr[2] == 'M' && bptr[3] == 'E' ) {
        strncpy(pfi->lame_encoder_version, (char *)bptr, 9);
        bptr += 9;

        // revision/vbr method byte
        pfi->lame_tag_revision = bptr[0] >> 4;
        pfi->lame_vbr_method   = bptr[0] & 15;
        buffer_consume(mp3->buf, 10);

        // Determine vbr status
        switch (pfi->lame_vbr_method) {
          case 1:
          case 8:
            pfi->vbr = CBR;
            break;
          case 2:
          case 9:
            pfi->vbr = ABR;
            break;
          default:
            pfi->vbr = VBR;
        }

        pfi->lame_lowpass = buffer_get_char(mp3->buf) * 100;

        // Skip peak
        buffer_consume(mp3->buf, 4);

        // Replay Gain, code from mpg123
        pfi->lame_replay_gain[0] = 0;
        pfi->lame_replay_gain[1] = 0;

        for (i=0; i<2; i++) {
          // Originator
          unsigned char origin;
          bptr = buffer_ptr(mp3->buf);
          
          origin = (bptr[0] >> 2) & 0x7;

          if (origin != 0) {
            // Gain type
            unsigned char gt = bptr[0] >> 5;
            if (gt == 1)
              gt = 0; /* radio */
            else if (gt == 2)
              gt = 1; /* audiophile */
            else
              continue;

            pfi->lame_replay_gain[gt]
              = (( (bptr[0] & 0x4) >> 2 ) ? -0.1 : 0.1)
              * ( (bptr[0] & 0x3) | bptr[1] );
          }

          buffer_consume(mp3->buf, 2);
        }

        // Skip encoding flags
        buffer_consume(mp3->buf, 1);

        // ABR rate/VBR minimum
        pfi->lame_abr_rate = buffer_get_char(mp3->buf);

        // Encoder delay/padding
        bptr = buffer_ptr(mp3->buf);
        pfi->lame_encoder_delay = ((((int)bptr[0]) << 4) | (((int)bptr[1]) >> 4));
        pfi->lame_encoder_padding = (((((int)bptr[1]) << 8) | (((int)bptr[2]))) & 0xfff);
        // sanity check
        if (pfi->lame_encoder_delay < 0 || pfi->lame_encoder_delay > 3000) {
          pfi->lame_encoder_delay = -1;
        }
        if (pfi->lame_encoder_padding < 0 || pfi->lame_encoder_padding > 3000) {
          pfi->lame_encoder_padding = -1;
        }
        buffer_consume(mp3->buf, 3);

        // Misc
        bptr = buffer_ptr(mp3->buf);
        pfi->lame_noise_shaping = bptr[0] & 0x3;
        pfi->lame_stereo_mode   = (bptr[0] & 0x1C) >> 2;
        pfi->lame_unwise        = (bptr[0] & 0x20) >> 5;
        pfi->lame_source_freq   = (bptr[0] & 0xC0) >> 6;
        buffer_consume(mp3->buf, 1);

        // XXX MP3 Gain, can't find a test file, current
        // mp3gain doesn't write this data
/*
        bptr = buffer_ptr(mp3->buf);
        unsigned char sign = (bptr[0] & 0x80) >> 7;
        pfi->lame_mp3gain = bptr[0] & 0x7F;
        if (sign) {
          pfi->lame_mp3gain *= -1;
        }
        pfi->lame_mp3gain_db = pfi->lame_mp3gain * 1.5;
*/
        buffer_consume(mp3->buf, 1);

        // Preset/Surround
        bptr = buffer_ptr(mp3->buf);
        pfi->lame_surround = (bptr[0] & 0x38) >> 3;
        pfi->lame_preset   = ((bptr[0] << 8) | bptr[1]) & 0x7ff;
        buffer_consume(mp3->buf, 2);

        // Music Length
        pfi->lame_music_length = buffer_get_int(mp3->buf);

        // Skip CRCs
      }
    }
  }
  // Check for VBRI header from Fhg encoders
  else if ( bptr[0] == 'V' && bptr[1] == 'B' && bptr[2] == 'R' && bptr[3] == 'I' ) {
    DEBUG_TRACE("Found VBRI tag\n");
    
    // Skip tag and version ID
    buffer_consume(mp3->buf, 6);

    pfi->vbri_delay   = buffer_get_short(mp3->buf);
    pfi->vbri_quality = buffer_get_short(mp3->buf);
    pfi->vbri_bytes   = buffer_get_int(mp3->buf);
    pfi->vbri_frames  = buffer_get_int(mp3->buf);
  }
  
  return 1;
}

static int
get_mp3fileinfo(PerlIO *infile, char *file, HV *info)
{
  struct mp3_frameinfo fi;
  unsigned char *bptr;
  char id3v1taghdr[4];

  unsigned int id3_size = 0; // size of leading ID3 data

  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to first audio frame
  off_t audio_size;          // size of all audio frames

  int song_length_ms = 0;    // duration of song in ms
  int bitrate        = 0;    // actual bitrate of song

  int found;
  int err = 0;
  
  mp3info *mp3;
  Newz(0, mp3, sizeof(mp3info), mp3info);
  Newz(0, mp3->buf, sizeof(Buffer), Buffer);
  
  mp3->infile = infile;
  mp3->file   = file;
  mp3->info   = info;
  
  buffer_init(mp3->buf, BLOCK_SIZE);
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  my_hv_store( info, "file_size", newSVuv(file_size) );

  memset((void*)&fi, 0, sizeof(fi));
  
  if ( !_check_buf(mp3->infile, mp3->buf, 10, BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }
  
  bptr = buffer_ptr(mp3->buf);

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
    
    my_hv_store( info, "id3_version", newSVpvf( "ID3v2.%d.%d", bptr[3], bptr[4] ) );
    
    DEBUG_TRACE("Found ID3v2.%d.%d tag, size %d\n", bptr[3], bptr[4], id3_size);

    // Always seek past the ID3 tags
    buffer_clear(mp3->buf);
    
    PerlIO_seek(infile, id3_size, SEEK_SET);
    
    if ( !_check_buf(mp3->infile, mp3->buf, 4, BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }

    audio_offset += id3_size;
  }

  found = 0;

  // Find an MP3 frame
  while ( !found && buffer_len(mp3->buf) ) {
    bptr = buffer_ptr(mp3->buf);
    
    while ( *bptr != 0xFF ) {
      buffer_consume(mp3->buf, 1);
     
      audio_offset++;

      if ( !buffer_len(mp3->buf) ) {
        if ( !_check_buf(mp3->infile, mp3->buf, 4, BLOCK_SIZE) ) {
          PerlIO_printf(PerlIO_stderr(), "Unable to find any MP3 frames in file: %s\n", file);
          err = -1;
          goto out;
        }
      }
      
      bptr = buffer_ptr(mp3->buf);
    }
    
    DEBUG_TRACE("Found FF sync at offset %d\n", audio_offset);
    
    // Make sure we have 4 bytes
    if ( !_check_buf(mp3->infile, mp3->buf, 4, BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }

    if ( !_decode_mp3_frame( buffer_ptr(mp3->buf), &fi ) ) {
      // Found a valid frame
      DEBUG_TRACE("  valid frame\n");
      
      found = 1;
    }
    else {
      // Not a valid frame, stray 0xFF
      DEBUG_TRACE("  invalid frame\n");
      
      buffer_consume(mp3->buf, 1);
      audio_offset++;
    }
  }

  if ( !found ) {
    PerlIO_printf(PerlIO_stderr(), "Unable to find any MP3 frames in file (checked 4K): %s\n", file);
    err = -1;
    goto out;
  }

  audio_size = file_size - audio_offset;

  // now check for Xing/Info/VBRI/LAME headers
  if ( !_parse_xing(mp3, &fi) ) {
    err = -1;
    goto out;
  }

  // use LAME CBR/ABR value for bitrate if available
  if ( (fi.vbr == CBR || fi.vbr == ABR) && fi.lame_abr_rate ) {
    if (fi.lame_abr_rate >= 255) {
      // ABR rate field only codes up to 255, use preset value instead
      if (fi.lame_preset <= 320) {
        bitrate = fi.lame_preset;
        DEBUG_TRACE("bitrate from lame_preset: %d\n", bitrate);
      }
    }
    else {
      bitrate = fi.lame_abr_rate;
      DEBUG_TRACE("bitrate from lame_abr_rate: %d\n", bitrate);
    }
  }

  // Or if we have a Xing header, use it to determine bitrate
  else if (fi.xing_frames && fi.xing_bytes) {
    float mfs = (float)fi.samplerate / ( fi.mpeg_version == 0x20 || fi.mpeg_version == 0x25 ? 72000. : 144000. );
    bitrate = ( fi.xing_bytes / fi.xing_frames * mfs );
    DEBUG_TRACE("bitrate from Xing header: %d\n", bitrate);
  }

  // Or use VBRI header
  else if (fi.vbri_frames && fi.vbri_bytes) {
    float mfs = (float)fi.samplerate / ( fi.mpeg_version == 0x20 || fi.mpeg_version == 0x25 ? 72000. : 144000. );
    bitrate = ( fi.vbri_bytes / fi.vbri_frames * mfs );
    DEBUG_TRACE("bitrate from VBRI header: %d\n", bitrate);
  }

  // check if last 128 bytes is ID3v1.0 or ID3v1.1 tag
  PerlIO_seek(infile, file_size - 128, SEEK_SET);
  if (PerlIO_read(infile, id3v1taghdr, 4) == 4) {
    if (id3v1taghdr[0]=='T' && id3v1taghdr[1]=='A' && id3v1taghdr[2]=='G') {
      DEBUG_TRACE("ID3v1 tag found\n");
      audio_size -= 128;
    }
  }

  // If we don't know the bitrate from Xing/LAME/VBRI, calculate average
  if ( !bitrate ) {    
    DEBUG_TRACE("Calculating average bitrate starting from %d...\n", audio_offset);
    bitrate = _mp3_get_average_bitrate(mp3, audio_offset, audio_size);

    if (bitrate <= 0) {
      // Couldn't determine bitrate, just use
      // the bitrate from the last frame we parsed
      DEBUG_TRACE("Unable to determine bitrate, using bitrate of most recent frame (%d)\n", fi.bitrate);
      bitrate = fi.bitrate;
    }
  }

  if (!song_length_ms) {
    if (fi.xing_frames) {
      song_length_ms = (int) ((double)(fi.xing_frames * fi.samples_per_frame * 1000.)/
				  (double) fi.samplerate);
    }
    else if (fi.vbri_frames) {
      song_length_ms = (int) ((double)(fi.vbri_frames * fi.samples_per_frame * 1000.)/
				  (double) fi.samplerate);
		}
    else {
      song_length_ms = (int) ((double)audio_size * 8. /
				  (double)bitrate);
    }
  }

  my_hv_store( info, "song_length_ms", newSViv(song_length_ms) );

  my_hv_store( info, "layer", newSViv(fi.layer) );
  my_hv_store( info, "stereo", newSViv(fi.stereo) );
  my_hv_store( info, "samples_per_frame", newSViv(fi.samples_per_frame) );
  my_hv_store( info, "padding", newSViv(fi.padding) );
  my_hv_store( info, "audio_size", newSViv(audio_size) );
  my_hv_store( info, "audio_offset", newSViv(audio_offset) );
  my_hv_store( info, "bitrate", newSViv( bitrate * 1000 ) );
  my_hv_store( info, "samplerate", newSViv( fi.samplerate ) );

  if (fi.xing_frames) {
    my_hv_store( info, "xing_frames", newSViv(fi.xing_frames) );
  }

  if (fi.xing_bytes) {
    my_hv_store( info, "xing_bytes", newSViv(fi.xing_bytes) );
    
    if (fi.xing_has_toc) {
      uint8_t i;
      AV *xing_toc = newAV();

      for (i = 0; i < 100; i++) {
        av_push( xing_toc, newSVuv(fi.xing_toc[i]) );
      }

      my_hv_store( info, "xing_toc", newRV_noinc( (SV *)xing_toc ) );
    }
  }

  if (fi.xing_quality) {
    my_hv_store( info, "xing_quality", newSViv(fi.xing_quality) );
  }

  if (fi.vbri_frames) {
    my_hv_store( info, "vbri_delay", newSViv(fi.vbri_delay) );
    my_hv_store( info, "vbri_frames", newSViv(fi.vbri_frames) );
    my_hv_store( info, "vbri_bytes", newSViv(fi.vbri_bytes) );
    my_hv_store( info, "vbri_quality", newSViv(fi.vbri_quality) );
  }

  if (fi.lame_encoder_version[0]) {
    my_hv_store( info, "lame_encoder_version", newSVpvn(fi.lame_encoder_version, 9) );
    my_hv_store( info, "lame_tag_revision", newSViv(fi.lame_tag_revision) );
    my_hv_store( info, "lame_vbr_method", newSVpv( vbr_methods[fi.lame_vbr_method], 0 ) );
    my_hv_store( info, "lame_lowpass", newSViv(fi.lame_lowpass) );

    if (fi.lame_replay_gain[0]) {
      my_hv_store( info, "lame_replay_gain_radio", newSVpvf( "%.1f dB", fi.lame_replay_gain[0] ) );
    }

    if (fi.lame_replay_gain[1]) {
      my_hv_store( info, "lame_replay_gain_audiophile", newSVpvf( "%.1f dB", fi.lame_replay_gain[1] ) );
    }

    my_hv_store( info, "lame_encoder_delay", newSViv(fi.lame_encoder_delay) );
    my_hv_store( info, "lame_encoder_padding", newSViv(fi.lame_encoder_padding) );

    my_hv_store( info, "lame_noise_shaping", newSViv(fi.lame_noise_shaping) );
    my_hv_store( info, "lame_stereo_mode", newSVpv( stereo_modes[fi.lame_stereo_mode], 0 ) );
    my_hv_store( info, "lame_unwise_settings", newSViv(fi.lame_unwise) );
    my_hv_store( info, "lame_source_freq", newSVpv( source_freqs[fi.lame_source_freq], 0 ) );

/*
    my_hv_store( info, "lame_mp3gain", newSViv(fi.lame_mp3gain) );
    my_hv_store( info, "lame_mp3gain_db", newSVnv(fi.lame_mp3gain_db) );
*/

    my_hv_store( info, "lame_surround", newSVpv( surround[fi.lame_surround], 0 ) );

    if (fi.lame_preset < 8) {
      my_hv_store( info, "lame_preset", newSVpvn( "Unknown", 7 ) );
    }
    else if (fi.lame_preset <= 320) {
      my_hv_store( info, "lame_preset", newSVpvf( "ABR %d", fi.lame_preset ) );
    }
    else if (fi.lame_preset <= 500) {
      fi.lame_preset /= 10;
      fi.lame_preset -= 41;
      if ( presets_v[fi.lame_preset] ) {
        my_hv_store( info, "lame_preset", newSVpv( presets_v[fi.lame_preset], 0 ) );
      }
    }
    else if (fi.lame_preset >= 1000 && fi.lame_preset <= 1007) {
      fi.lame_preset -= 1000;
      if ( presets_old[fi.lame_preset] ) {
        my_hv_store( info, "lame_preset", newSVpv( presets_old[fi.lame_preset], 0 ) );
      }
    }

    if (fi.vbr == ABR || fi.vbr == VBR) {
      my_hv_store( info, "vbr", newSViv(1) );
    }
  }

out:
  buffer_free(mp3->buf);
  Safefree(mp3->buf);
  Safefree(mp3);

  if (err) return err;

  return 0;
}

static int
mp3_find_frame(PerlIO *infile, char *file, int offset)
{
  Buffer mp3_buf;
  unsigned char *bptr;
  unsigned int buf_size;
  struct mp3_frameinfo fi;
  int frame_offset = -1;
  off_t audio_offset;
  HV *info = newHV();
  
  buffer_init(&mp3_buf, BLOCK_SIZE);
  
  if ( (get_mp3fileinfo(infile, file, info)) != 0 ) {
    goto out;
  }
  
  audio_offset = SvIV( *(my_hv_fetch(info, "audio_offset")) );
  
  // Use Xing TOC if available
  if ( my_hv_exists(info, "xing_toc") ) {
    // Don't use Xing TOC if trying to seek to audio_offset + 1, which is special
    if ( offset != audio_offset + 1 ) {
      uint8_t percent;
      uint16_t tv;
      off_t file_size     = SvIV( *(my_hv_fetch(info, "file_size")) );
      AV *xing_toc        = (AV *)SvRV( *(my_hv_fetch(info, "xing_toc")) );
      uint32_t xing_bytes = SvIV( *(my_hv_fetch(info, "xing_bytes")) );
    
      if (offset >= file_size) {
        goto out;
      }
    
      percent = (int)((offset * 1.0 / file_size) * 100 + 0.5);
    
      if (percent > 99)
        percent = 99;
    
      tv = SvIV( *(av_fetch(xing_toc, percent, 0)) );
    
      offset = (tv / 256.0) * xing_bytes;
    
      offset += audio_offset;
    
      // Don't return offset == audio_offset, because that would be the Xing frame
      if (offset == audio_offset) {
        offset += 1;
      }
    
      DEBUG_TRACE("find_frame: using Xing TOC, percent: %d, tv: %d, new offset: %d\n", percent, tv, offset);
    }
  }
  
  PerlIO_seek(infile, offset, SEEK_SET);

  if ( !_check_buf(infile, &mp3_buf, 4, BLOCK_SIZE) ) {
    goto out;
  }
  
  bptr = (unsigned char *)buffer_ptr(&mp3_buf);
  buf_size = buffer_len(&mp3_buf);
  
  // Find 0xFF sync and verify it's a valid mp3 frame header
  while (1) {
    if (
      buf_size < 4
      ||
      ( bptr[0] == 0xFF && !_decode_mp3_frame( bptr, &fi ) )
    ) {
      break;
    }
    
    bptr++;
    buf_size--;
  }
  
  if (buf_size >= 4) {
    frame_offset = offset + BLOCK_SIZE - buf_size;
    DEBUG_TRACE("find_frame: frame_offset: %d\n", frame_offset);
  }

out:
  buffer_free(&mp3_buf);
  SvREFCNT_dec(info);

  return frame_offset;
}
