//=========================================================================
// FILENAME	: tagutils-mp3.c
// DESCRIPTION	: MP3 metadata reader
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
//=========================================================================

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
 
#include "tagutils-mp3.h"

static unsigned char*
_get_utf8_text(const id3_ucs4_t* native_text) {
  unsigned char *utf8_text = NULL;
  char *in, *in8, *iconv_buf;
  iconv_result rc;
  int i, n;

  in = (char*) id3_ucs4_latin1duplicate(native_text);
  if (!in) {
    goto out;
  }

  in8 = (char*) id3_ucs4_utf8duplicate(native_text);
  if (!in8) {
    free(in);
    goto out;
  }

  iconv_buf = (char*) calloc(MAX_ICONV_BUF, sizeof(char));
  if (!iconv_buf) {
    free(in); free(in8);
    goto out;
  }

  i = lang_index;
  // (1) try utf8 -> default
  rc = do_iconv(iconv_map[i].cpnames[0], "UTF-8", in8, strlen(in8), iconv_buf, MAX_ICONV_BUF);
  if (rc == ICONV_OK) {
    utf8_text = (unsigned char*) in8;
    free(iconv_buf);
  }
  else if (rc == ICONV_TRYNEXT) {
    // (2) try default -> utf8
    rc = do_iconv("UTF-8", iconv_map[i].cpnames[0], in, strlen(in), iconv_buf, MAX_ICONV_BUF);
    if (rc == ICONV_OK) {
      utf8_text = (unsigned char*) iconv_buf;
    }
    else if (rc == ICONV_TRYNEXT) {
      // (3) try other encodes
      for (n=1; n<N_LANG_ALT && iconv_map[i].cpnames[n]; n++) {
	      rc = do_iconv("UTF-8", iconv_map[i].cpnames[n], in, strlen(in), iconv_buf, MAX_ICONV_BUF);
	      if (rc == ICONV_OK) {
	        utf8_text = (unsigned char*) iconv_buf;
	        break;
	      }
      }
      if (!utf8_text) {
	      // cannot iconv
	      utf8_text = (unsigned char*) id3_ucs4_utf8duplicate(native_text);
	      free(iconv_buf);
      }
    }
    free(in8);
  }
  free(in);

 out:
  if(!utf8_text) {
    utf8_text = (unsigned char*) strdup("UNKNOWN");
  }

  return utf8_text;
}

// _get_mp3tags
static int
_get_mp3tags(char *file, HV *tags)
{
  struct id3_file *pid3file;
  struct id3_tag *pid3tag;
  struct id3_frame *pid3frame;
  int err;
  int index;
  unsigned int nstrings;
  
  id3_ucs4_t const *key;
  id3_ucs4_t const *value;
  char *utf8_key;
  char *utf8_value;
  SV *bin;
  
  int got_numeric_genre;
  char *tmp;

  pid3file = id3_file_open(file, ID3_FILE_MODE_READONLY);
  if (!pid3file) {
    fprintf(stderr, "Cannot open %s\n", file);
    return -1;
  }

  pid3tag = id3_file_tag(pid3file);

  if (!pid3tag) {
    err = errno;
    id3_file_close(pid3file);
    errno = err;
    fprintf(stderr, "Cannot get ID3 tag for %s\n", file);
    return -1;
  }

  index = 0;
  while ((pid3frame = id3_tag_findframe(pid3tag, "", index))) {
    key = NULL;
    value = NULL;
    utf8_key = NULL;
    utf8_value = NULL;
    got_numeric_genre = 0;
    
    //fprintf(stderr, "%s (%d fields)\n", pid3frame->id, pid3frame->nfields);
    
    // Special handling for TXXX frames
    if ( !strcmp(pid3frame->id, "TXXX") ) {
      key = id3_field_getstring(&pid3frame->fields[1]);
      if (key) {
        // Get the key
        utf8_key = (char *)id3_ucs4_utf8duplicate(key);
        hv_store( tags, utf8_key, strlen(utf8_key), NULL, 0 );
        
        // Get the value
        value = id3_field_getstring(&pid3frame->fields[2]);
        if (!value) {
          hv_delete( tags, utf8_key, strlen(utf8_key), 0 );
        }
        else {
          utf8_value = (char *)id3_ucs4_utf8duplicate(value);
          hv_store( tags, utf8_key, strlen(utf8_key), newSVpv( utf8_value, 0 ), 0 );
          free(utf8_value);
        }
        
        free(utf8_key);
      }
    }
    
    // All other frames
    else {
      if (pid3frame->nfields == 1) {
        // unknown frames (i.e. XSOP) that are just available as binary data
        bin = newSVpvn( pid3frame->fields[0].binary.data, pid3frame->fields[0].binary.length );
        hv_store( tags, pid3frame->id, strlen(pid3frame->id), bin, 0 );
      }
      else if (pid3frame->nfields == 2) {
        switch (pid3frame->fields[1].type) {
          case ID3_FIELD_TYPE_STRINGLIST:
            nstrings = id3_field_getnstrings(&pid3frame->fields[1]);
            if (nstrings > 1) {
              // XXX, how to handle this?
              fprintf(stderr, "STRINGLIST, %d strings\n", nstrings );
            }
            else {
              utf8_value = (char *)id3_ucs4_utf8duplicate( id3_field_getstrings(&pid3frame->fields[1], 0) );
              hv_store( tags, pid3frame->id, strlen(pid3frame->id), newSVpv( utf8_value, 0 ), 0 );
              free(utf8_value);
            }
            break;
        
          case ID3_FIELD_TYPE_STRING:
            utf8_value = (char *)id3_ucs4_utf8duplicate( id3_field_getfullstring(&pid3frame->fields[1]) );
            hv_store( tags, pid3frame->id, strlen(pid3frame->id), newSVpv( utf8_value, 0 ), 0 );
            free(utf8_value);
            break;
      
          case ID3_FIELD_TYPE_LATIN1:
            hv_store( tags, pid3frame->id, strlen(pid3frame->id), newSVpv( (char *)id3_field_getlatin1(&pid3frame->fields[1]), 0 ), 0 );
            break;
          
          case ID3_FIELD_TYPE_BINARYDATA:
            bin = newSVpvn( pid3frame->fields[1].binary.data, pid3frame->fields[1].binary.length );
            hv_store( tags, pid3frame->id, strlen(pid3frame->id), bin, 0 );
            break;
      
          default:
            fprintf(stderr, "Unknown type: %d\n", pid3frame->fields[1].type);
            break;
        }
      }
      else {
        // special handling for APIC
        if ( !strcmp(pid3frame->id, "APIC") ) {
          // XXX: also save other info
          bin = newSVpvn( pid3frame->fields[4].binary.data, pid3frame->fields[4].binary.length );
          hv_store( tags, pid3frame->id, strlen(pid3frame->id), bin, 0 );
        }
        else {
          // XXX
          fprintf(stderr, "  > 2 fields\n");
        }
      }
    }

    index++;
  }

  id3_file_close(pid3file);
  return 0;
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
  pfi->layer = 4 - ((frame[1] & 0x6) >> 1);
  
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

  if ((frame[3] & 0xC0 >> 6) == 3)
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

  pfi->crc_protected = frame[1] & 0xFE;

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
//    read from midle of file, and estimate
static void _mp3_get_average_bitrate(FILE *infile, struct mp3_frameinfo *pfi)
{
  off_t file_size;
  unsigned char frame_buffer[2900];
  unsigned char header[4];
  int index = 0;
  int found = 0;
  off_t pos;
  struct mp3_frameinfo fi;
  int frame_count=0;
  int bitrate_total=0;

  fseek(infile,0,SEEK_END);
  file_size=ftell(infile);

  pos = file_size>>1;

  /* now, find the first frame */
  fseek(infile,pos,SEEK_SET);
  if (fread(frame_buffer, 1, sizeof(frame_buffer), infile) != sizeof(frame_buffer))
    return;

  while (!found) {
    while ((frame_buffer[index] != 0xFF) && (index < (sizeof(frame_buffer)-4)))
      index++;

    if (index >= (sizeof(frame_buffer)-4)) { // max mp3 framesize = 2880
      fprintf(stderr, "Could not find frame... quitting\n");
      return;
    }

    if (!_decode_mp3_frame(&frame_buffer[index],&fi)) {
      /* see if next frame is valid */
      fseek(infile,pos + index + fi.frame_length, SEEK_SET);
      if (fread(header, 1, sizeof(header), infile) != sizeof(header)) {
        fprintf(stderr, "Could not read frame header\n");
        return;
      }

      if (!_decode_mp3_frame(header, &fi))
	      found=1;
    }

    if (!found)
      index++;
  }

  pos += index;

  // got first frame
  while (frame_count < 10) {
    fseek(infile,pos,SEEK_SET);
    if (fread(header,1,sizeof(header),infile) != sizeof(header)) {
      fprintf(stderr, "Could not read frame header\n");
      return;
    }
    if (_decode_mp3_frame(header,&fi)) {
      fprintf(stderr, "Invalid frame header while averaging\n");
      return;
    }

    bitrate_total += fi.bitrate;
    frame_count++;
    pos += fi.frame_length;
  }

  pfi->bitrate = bitrate_total/frame_count;

  return;
}

// _get_mp3fileinfo
static int
_get_mp3fileinfo(char *file, HV *info)
{
  FILE *infile;
  struct mp3_frameinfo fi;
  
  unsigned char *buf = malloc(BLOCK_SIZE);
  unsigned char *buf_ptr = buf;
  
  unsigned int id3_size = 0; // size of leading ID3 data
  unsigned int buf_size = 0; // amount of data left in buf
  
  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to first audio frame
  off_t audio_size;          // size of all audio frames
  
  int song_length_ms = 0;    // duration of song in ms
  
  int i;
  int xing_flags;
  int found;

  char frame_buffer[4];

  char id3v1taghdr[4];

  if (!(infile=fopen(file, "rb"))) {
    fprintf(stderr, "Could not open %s for reading\n",file);
    return -1;
  }

  memset((void*)&fi, 0, sizeof(fi));

  fseek(infile,0,SEEK_END);
  file_size = ftell(infile);
  fseek(infile,0,SEEK_SET);

  if ((buf_size = fread(buf, 1, BLOCK_SIZE, infile)) == 0) {
    if (ferror(infile)) {
      fprintf(stderr, "Error reading: %s\n", strerror(errno));
    }
    else {
      fprintf(stderr, "File too small. Probably corrupted.\n");
    }
    fclose(infile);
    free(buf_ptr);
    return -1;
  }

  found = 0;
  
  if (
    (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') &&
    buf[3] < 0xff && buf[4] < 0xff &&
    buf[6] < 0x80 && buf[7] < 0x80 && buf[8] < 0x80 && buf[9] < 0x80
  ) {
    char tagversion[16];

    /* found an ID3 header... */
    id3_size = 10 + (buf[6]<<21) + (buf[7]<<14) + (buf[8]<<7) + buf[9];
    
    if (buf[5] & 0x10) {
      // footer present
      id3_size += 10;
    }
    
    snprintf(tagversion, sizeof(tagversion), "ID3v2.%d.%d", buf[3], buf[4]);
    hv_store( info, "id3_version", 11, newSVpv( tagversion, 0 ), 0 );
	    
	  if ( buf_size <= id3_size ) {
	    // ID3 is larger than the amount we read
      fseek(infile, id3_size, SEEK_SET);
      buf_size = fread(buf, 1, BLOCK_SIZE, infile);
    }
    else {
	    buf += id3_size;
    }
    
    audio_offset += id3_size;
  }

  /* Here we start the brute-force header seeking.  Sure wish there
   * weren't so many crappy mp3 files out there
   */
  while (!found) {
    if ( buf_size < 4 ) {
      // Not enough data for a header, read more
      if ((buf_size += fread(buf, 1, BLOCK_SIZE, infile)) == 0) {
        if (ferror(infile)) {
          fprintf(stderr, "Error reading: %s\n", strerror(errno));
        }
        else {
          fprintf(stderr, "Unable to find MP3 frame in file.\n");
        }
        fclose(infile);
        free(buf_ptr);
        return -1;
      }
    }
    
    while ( *buf != 0xFF ) {
      buf++;
      buf_size--;
      audio_offset++;
    }

    if ( !_decode_mp3_frame(buf, &fi) ) {
      // Found a valid frame
      found = 1;
    }
    else {
      // Not a valid frame, stray 0xFF
      buf++;
      buf_size--;
      audio_offset++;
    }
  }
  
  audio_size = file_size - audio_offset;
  
  // check if last 128 bytes is ID3v1.0 ID3v1.1 tag
  fseek(infile, file_size - 128, SEEK_SET);
  if (fread(id3v1taghdr, 1, 4, infile) == 4) {
    if (id3v1taghdr[0]=='T' && id3v1taghdr[1]=='A' && id3v1taghdr[2]=='G') {
      audio_size -= 128;
    }
  }

  if (_decode_mp3_frame(buf, &fi)) {
    fclose(infile);
    fprintf(stderr, "Could not find sync frame: %s\n", file);
    free(buf_ptr);
    return 0;
  }

  /* now check for Xing/Info/VBRI headers */
  buf += fi.xing_offset;
  
  if ( buf[0] == 'X' || buf[0] == 'I' ) {
    if (
      ( buf[1] == 'i' && buf[2] == 'n' && buf[3] == 'g' )
      ||
      ( buf[1] == 'n' && buf[2] == 'f' && buf[3] == 'o' )
    ) {
      buf += 4;
    
      xing_flags = GET_INT32BE(buf);
    
      if (xing_flags & XING_FRAMES) {
        fi.xing_frames = GET_INT32BE(buf);
      }
    
      if (xing_flags & XING_BYTES) {
        fi.xing_bytes = GET_INT32BE(buf);
      }
    
      if (xing_flags & XING_TOC) {
        // skip it
        buf += 100;
      }
    
      if (xing_flags & XING_QUALITY) {
        fi.xing_quality = GET_INT32BE(buf);
      }
    
      // LAME tag
      if ( buf[0] == 'L' && buf[1] == 'A' && buf[2] == 'M' && buf[3] == 'E' ) {
        strncpy(fi.lame_encoder_version, buf, 9);
        buf += 9;
      
        // revision/vbr method byte
        fi.lame_tag_revision = buf[0] >> 4;
        fi.lame_vbr_method   = buf[0] & 15;
        buf++;
      
        fi.lame_lowpass = buf[0] * 100;
        buf++;
        
        // Skip peak
        buf += 4;
        
        // Replay Gain, code from mpg123
        fi.lame_replay_gain[0] = 0;
        fi.lame_replay_gain[1] = 0;
        
        for (i=0; i<2; i++) {
          // Originator
          unsigned char origin = (buf[0] >> 2) & 0x7;
          
          if (origin != 0) {
            // Gain type
            unsigned char gt = buf[0] >> 5;
            if (gt == 1)
              gt = 0; /* radio */
            else if (gt == 2)
              gt = 1; /* audiophile */
            else
              continue;
            
            // XXX: this may be wrong, eyeD3 gives different results
            fi.lame_replay_gain[gt] = ((buf[0] & 0x2) ? -0.1 : 0.1) * (MAKE_SHORT(buf) & 0x1f);
          }
          
          buf += 2;
        }
        
        // Skip encoding flags
        buf++;
        
        // Skip ABR rate
        buf++;
        
        // Encoder delay/padding
        fi.lame_encoder_delay = ((((int)buf[0]) << 4) | (((int)buf[1]) >> 4));
        fi.lame_encoder_padding = (((((int)buf[1]) << 8) | (((int)buf[2]))) & 0xfff);
      
        // XXX more?
      }
    }
  }
  // Check for VBRI header from Fhg encoders
  else if ( buf[0] == 'V' && buf[1] == 'B' && buf[2] == 'R' && buf[3] == 'I' ) {
    // XXX
    fprintf(stderr, "found VBRI\n");
  }
  
  // XXX: use LAME ABR value for bitrate if available
  
  // If we have a Xing header, use it to determine bitrate
  if (fi.xing_frames && fi.xing_bytes) {
    float mfs = (float)fi.samplerate / ( fi.mpeg_version == 0x25 ? 72000. : 144000. );
    fi.bitrate = (int)( fi.xing_bytes / fi.xing_frames * mfs );
  }
  
/*
  // XXX: Unless we know bitrate from LAME tag
  _mp3_get_average_bitrate(infile, &fi);
*/

  if (!song_length_ms) {
    if (fi.xing_frames) {
      song_length_ms = (int) ((double)(fi.xing_frames * fi.samples_per_frame * 1000.)/
				  (double) fi.samplerate);
    }
    else {
      song_length_ms = (int) ((double) (file_size - audio_offset) * 8. /
				  (double) fi.bitrate);
    }
  }

  hv_store( info, "song_length_ms", 14, newSViv(song_length_ms), 0 );
  
  hv_store( info, "layer", 5, newSViv(fi.layer), 0 );
  hv_store( info, "stereo", 6, newSViv(fi.stereo), 0 );
  hv_store( info, "samples_per_frame", 17, newSViv(fi.samples_per_frame), 0 );
  hv_store( info, "padding", 7, newSViv(fi.padding), 0 );
  hv_store( info, "audio_size", 10, newSViv(audio_size), 0 );
  hv_store( info, "audio_offset", 12, newSViv(audio_offset), 0 );
  hv_store( info, "bitrate", 7, newSViv( fi.bitrate ), 0 );
  hv_store( info, "samplerate", 10, newSViv( fi.samplerate ), 0 );
  
  if (fi.xing_frames) {
    hv_store( info, "xing_frames", 11, newSViv(fi.xing_frames), 0 );
  }
  
  if (fi.xing_bytes) {
    hv_store( info, "xing_bytes", 10, newSViv(fi.xing_bytes), 0 );
  }
  
  if (fi.xing_quality) {
    hv_store( info, "xing_quality", 12, newSViv(fi.xing_quality), 0 );
  }
  
  if (fi.lame_encoder_version[0]) {
    hv_store( info, "lame_encoder_version", 20, newSVpvn(fi.lame_encoder_version, 9), 0 );
    hv_store( info, "lame_tag_revision", 17, newSViv(fi.lame_tag_revision), 0 );
    hv_store( info, "lame_vbr_method", 15, newSViv(fi.lame_vbr_method), 0 );
    hv_store( info, "lame_lowpass", 12, newSViv(fi.lame_lowpass), 0 );
    hv_store( info, "lame_replay_gain_radio", 22, newSVnv( fi.lame_replay_gain[0] ), 0 );
    hv_store( info, "lame_replay_gain_audiophile", 27, newSVnv(fi.lame_replay_gain[1]), 0 );
    hv_store( info, "lame_encoder_delay", 18, newSViv(fi.lame_encoder_delay), 0 );
    hv_store( info, "lame_encoder_padding", 20, newSViv(fi.lame_encoder_padding), 0 );
  }

  fclose(infile);
  
  free(buf_ptr);

  return 0;
}
