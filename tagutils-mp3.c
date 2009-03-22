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

static int
get_mp3tags(char *file, HV *info, HV *tags)
{
  struct id3_file *pid3file;
  struct id3_tag *pid3tag;
  struct id3_frame *pid3frame;
  int err;
  int index;
  unsigned int nstrings;
  unsigned char trck_found = 0; // whether we found a track tag, used to determine ID3v1 from ID3v1.1
  
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
    
    // Special handling for TCON genre frame, lookup the genre string
    else if ( !strcmp(pid3frame->id, "TCON") ) {
      char *genre_string;
      
      utf8_value = (char *)id3_ucs4_utf8duplicate( id3_field_getstrings(&pid3frame->fields[1], 0) );
      
      if ( isdigit(utf8_value[0]) ) {
        // Convert to genre string
        genre_string = (char *)id3_ucs4_utf8duplicate( id3_genre_name( id3_field_getstrings(&pid3frame->fields[1], 0) ) );
        hv_store( tags, pid3frame->id, strlen(pid3frame->id), newSVpv( genre_string, 0 ), 0 );
        free(genre_string);
      }
      // XXX support '(23) Ambient'
      else {
        hv_store( tags, pid3frame->id, strlen(pid3frame->id), newSVpv( utf8_value, 0 ), 0 );
      }
      
      free(utf8_value);
    }
    
    // Ignore ZOBS (obsolete) frames
    else if ( !strcmp(pid3frame->id, "ZOBS") ) {
      
    }
    
    // All other frames
    else {
      if (pid3frame->nfields == 1) {
        // unknown frames (i.e. XSOP) that are just available as binary data
        bin = newSVpvn( pid3frame->fields[0].binary.data, pid3frame->fields[0].binary.length );
        hv_store( tags, pid3frame->id, strlen(pid3frame->id), bin, 0 );
      }
      else if (pid3frame->nfields == 2) {
        // Remember if TRCK tag is found for ID3v1.1
        if ( !strcmp(pid3frame->id, "TRCK") ) {
          trck_found = 1;
        }
        
        //fprintf(stderr, "  type %d\n", pid3frame->fields[1].type);
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
        // special handling for COMM
        if ( !strcmp(pid3frame->id, "COMM") ) {
          switch (pid3frame->fields[3].type) {
            case ID3_FIELD_TYPE_STRINGFULL:
              utf8_value = (char *)id3_ucs4_utf8duplicate( id3_field_getfullstring(&pid3frame->fields[3]) );
              hv_store( tags, pid3frame->id, strlen(pid3frame->id), newSVpv( utf8_value, 0 ), 0 );
              free(utf8_value);
              break;
              
            default:
              fprintf(stderr, "Unsupported COMM type %d\n", pid3frame->fields[3].type);
              break;
          }
        }
        
        // special handling for APIC
        else if ( !strcmp(pid3frame->id, "APIC") ) {
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
  
  // Update id3_version field if we found a v1 tag
  if ( pid3tag->options & ID3_TAG_OPTION_ID3V1 ) {
    if (trck_found == 1) {
      hv_store( info, "id3_version", 11, newSVpv( "ID3v1.1", 0 ), 0 );
    }
    else {
      hv_store( info, "id3_version", 11, newSVpv( "ID3v1", 0 ), 0 );
    }
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
// average bitrate from a large chunk of the middle of the file
static short _mp3_get_average_bitrate(FILE *infile)
{
  struct mp3_frameinfo fi;
  int frame_count   = 0;
  int bitrate_total = 0;
  int err = 0;
  
  unsigned char *buf = malloc(WANTED_FOR_AVG);
  unsigned char *buf_ptr = buf;
  unsigned int buf_size = 0;
  
  // Seek to middle of file
  fseek(infile, 0, SEEK_END);
  fseek(infile, ftell(infile) >> 1, SEEK_SET);
  
  if ((buf_size = fread(buf, 1, WANTED_FOR_AVG, infile)) == 0) {
    if (ferror(infile)) {
      fprintf(stderr, "Error reading: %s\n", strerror(errno));
    }
    else {
      fprintf(stderr, "File too small. Probably corrupted.\n");
    }
    err = -1;
    goto out;
  }
  
  while (buf_size >= 4) {
    while ( *buf != 0xFF ) {
      buf++;
      buf_size--;
      
      if ( !buf_size ) {
        err = -1;
        goto out;
      }
    }

    if ( !_decode_mp3_frame(buf, &fi) ) {
      // Found a valid frame
      frame_count++;
      bitrate_total += fi.bitrate;
      
      if (fi.frame_length > buf_size) {
        break;
      }
      
      buf += fi.frame_length;
      buf_size -= fi.frame_length;
    }
    else {
      // Not a valid frame, stray 0xFF
      buf++;
      buf_size--;
    }
  }
  
out:
  
  fclose(infile);
  free(buf_ptr);
  
  if (err) return err;
  
  return bitrate_total / frame_count;
}

static void
_parse_xing(unsigned char *buf, struct mp3_frameinfo *pfi)
{
  int i;
  int xing_flags;
    
  buf += pfi->xing_offset;

  if ( buf[0] == 'X' || buf[0] == 'I' ) {
    if (
      ( buf[1] == 'i' && buf[2] == 'n' && buf[3] == 'g' )
      ||
      ( buf[1] == 'n' && buf[2] == 'f' && buf[3] == 'o' )
    ) {
      // It's VBR if tag is Xing, and CBR if Info
      pfi->vbr = buf[1] == 'i' ? VBR : CBR;

      buf += 4;

      xing_flags = GET_INT32BE(buf);

      if (xing_flags & XING_FRAMES) {
        pfi->xing_frames = GET_INT32BE(buf);
      }

      if (xing_flags & XING_BYTES) {
        pfi->xing_bytes = GET_INT32BE(buf);
      }

      if (xing_flags & XING_TOC) {
        // skip it
        buf += 100;
      }

      if (xing_flags & XING_QUALITY) {
        pfi->xing_quality = GET_INT32BE(buf);
      }

      // LAME tag
      if ( buf[0] == 'L' && buf[1] == 'A' && buf[2] == 'M' && buf[3] == 'E' ) {
        strncpy(pfi->lame_encoder_version, (char *)buf, 9);
        buf += 9;

        // revision/vbr method byte
        pfi->lame_tag_revision = buf[0] >> 4;
        pfi->lame_vbr_method   = buf[0] & 15;
        buf++;

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

        pfi->lame_lowpass = buf[0] * 100;
        buf++;

        // Skip peak
        buf += 4;

        // Replay Gain, code from mpg123
        pfi->lame_replay_gain[0] = 0;
        pfi->lame_replay_gain[1] = 0;

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

            pfi->lame_replay_gain[gt] 
              = (( (buf[0] & 0x4) >> 2 ) ? -0.1 : 0.1)
              * ( (buf[0] & 0x3) | buf[1] );
          }

          buf += 2;
        }

        // Skip encoding flags
        buf++;

        // ABR rate/VBR minimum
        pfi->lame_abr_rate = (int)buf[0];
        buf++;

        // Encoder delay/padding
        pfi->lame_encoder_delay = ((((int)buf[0]) << 4) | (((int)buf[1]) >> 4));
        pfi->lame_encoder_padding = (((((int)buf[1]) << 8) | (((int)buf[2]))) & 0xfff);
        // sanity check
        if (pfi->lame_encoder_delay < 0 || pfi->lame_encoder_delay > 3000) {
          pfi->lame_encoder_delay = -1;
        }
        if (pfi->lame_encoder_padding < 0 || pfi->lame_encoder_padding > 3000) {
          pfi->lame_encoder_padding = -1;
        }
        buf += 3;

        // Misc
        pfi->lame_noise_shaping = buf[0] & 0x3;
        pfi->lame_stereo_mode   = (buf[0] & 0x1C) >> 2;
        pfi->lame_unwise        = (buf[0] & 0x20) >> 5;
        pfi->lame_source_freq   = (buf[0] & 0xC0) >> 6;
        buf++;

        // XXX MP3 Gain, can't find a test file, current
        // mp3gain doesn't write this data
/*
        unsigned char sign = (buf[0] & 0x80) >> 7;
        pfi->lame_mp3gain = buf[0] & 0x7F;
        if (sign) {
          pfi->lame_mp3gain *= -1;
        }
        pfi->lame_mp3gain_db = pfi->lame_mp3gain * 1.5;
*/
        buf++;

        // Preset/Surround
        pfi->lame_surround = (buf[0] & 0x38) >> 3;
        pfi->lame_preset   = ((buf[0] << 8) | buf[1]) & 0x7ff;
        buf += 2;

        // Music Length
        pfi->lame_music_length = GET_INT32BE(buf);

        // Skip CRCs
      }
    }
  }
  // Check for VBRI header from Fhg encoders
  else if ( buf[0] == 'V' && buf[1] == 'B' && buf[2] == 'R' && buf[3] == 'I' ) {
    // Skip tag and version ID
    buf += 6;
        
    pfi->vbri_delay   = GET_INT16BE(buf);    
    pfi->vbri_quality = GET_INT16BE(buf);    
    pfi->vbri_bytes   = GET_INT32BE(buf);    
    pfi->vbri_frames  = GET_INT32BE(buf);
  }
}

static int
get_mp3fileinfo(char *file, HV *info)
{
  FILE *infile;
  struct mp3_frameinfo fi;
  
  unsigned char *buf = malloc(BLOCK_SIZE);
  unsigned char *buf_ptr = buf;
  char id3v1taghdr[4];
  
  unsigned int id3_size = 0; // size of leading ID3 data
  unsigned int buf_size = 0; // amount of data left in buf
  
  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to first audio frame
  off_t audio_size;          // size of all audio frames
  
  int song_length_ms = 0;    // duration of song in ms
  short bitrate      = 0;    // actual bitrate of song
  
  int found;
  int err = 0;

  if (!(infile=fopen(file, "rb"))) {
    fprintf(stderr, "Could not open %s for reading\n",file);
    err = -1;
    goto out;
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
    
    err = -1;
    goto out;
  }
  
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
        
    // Always seek past the ID3 tags
    fseek(infile, id3_size, SEEK_SET);
    buf_size = fread(buf, 1, BLOCK_SIZE, infile);
    
    audio_offset += id3_size;
  }
  
  found = 0;
  
  // Find an MP3 frame
  while ( !found && buf_size ) {
    while ( *buf != 0xFF ) {
      buf++;
      buf_size--;
      audio_offset++;
      
      if ( !buf_size ) {
        fprintf(stderr, "Unable to find any MP3 frames in file (checked 4K): %s\n", file);
        err = -1;
        goto out;
      }
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
  
  if ( !found ) {
    fprintf(stderr, "Unable to find any MP3 frames in file (checked 4K): %s\n", file);
    err = -1;
    goto out;
  }
  
  audio_size = file_size - audio_offset;
  
  // check if last 128 bytes is ID3v1.0 or ID3v1.1 tag
  fseek(infile, file_size - 128, SEEK_SET);
  if (fread(id3v1taghdr, 1, 4, infile) == 4) {
    if (id3v1taghdr[0]=='T' && id3v1taghdr[1]=='A' && id3v1taghdr[2]=='G') {
      audio_size -= 128;
    }
  }

  if ( _decode_mp3_frame(buf, &fi) ) {
    fprintf(stderr, "Could not find sync frame: %s\n", file);
    goto out;
  }
  
  // now check for Xing/Info/VBRI/LAME headers
  _parse_xing(buf, &fi);

  // use LAME CBR/ABR value for bitrate if available
  if ( (fi.vbr == CBR || fi.vbr == ABR) && fi.lame_abr_rate ) {
    if (fi.lame_abr_rate >= 255) {
      // ABR rate field only codes up to 255, use preset value instead
      if (fi.lame_preset <= 320) {
        bitrate = fi.lame_preset;
      }
    }
    else {
      bitrate = fi.lame_abr_rate;
    }
  }
  
  // Or if we have a Xing header, use it to determine bitrate
  else if (fi.xing_frames && fi.xing_bytes) {
    float mfs = (float)fi.samplerate / ( fi.mpeg_version == 0x25 ? 72000. : 144000. );
    bitrate = (short)( fi.xing_bytes / fi.xing_frames * mfs );
  }
  
  // Or use VBRI header
  else if (fi.vbri_frames && fi.vbri_bytes) {
    float mfs = (float)fi.samplerate / ( fi.mpeg_version == 0x25 ? 72000. : 144000. );
    bitrate = (short)( fi.vbri_bytes / fi.vbri_frames * mfs );
  }
  
  // If we don't know the bitrate from Xing/LAME/VBRI, calculate average
  if ( !bitrate ) {
    if (audio_size >= WANTED_FOR_AVG) {
      bitrate = _mp3_get_average_bitrate(infile);
    }
    
    if (bitrate <= 0) {
      // Couldn't determine bitrate, just use
      // the bitrate from the last frame we parsed
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
      song_length_ms = (int) ((double) (file_size - audio_offset) * 8. /
				  (double)bitrate);
    }
  }

  hv_store( info, "song_length_ms", 14, newSViv(song_length_ms), 0 );
  
  hv_store( info, "layer", 5, newSViv(fi.layer), 0 );
  hv_store( info, "stereo", 6, newSViv(fi.stereo), 0 );
  hv_store( info, "samples_per_frame", 17, newSViv(fi.samples_per_frame), 0 );
  hv_store( info, "padding", 7, newSViv(fi.padding), 0 );
  hv_store( info, "audio_size", 10, newSViv(audio_size), 0 );
  hv_store( info, "audio_offset", 12, newSViv(audio_offset), 0 );
  hv_store( info, "bitrate", 7, newSViv( bitrate ), 0 );
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
  
  if (fi.vbri_frames) {
    hv_store( info, "vbri_delay", 10, newSViv(fi.vbri_delay), 0 );
    hv_store( info, "vbri_frames", 11, newSViv(fi.vbri_frames), 0 );
    hv_store( info, "vbri_bytes", 10, newSViv(fi.vbri_bytes), 0 );
    hv_store( info, "vbri_quality", 12, newSViv(fi.vbri_quality), 0 );
  }    
  
  if (fi.lame_encoder_version[0]) {
    hv_store( info, "lame_encoder_version", 20, newSVpvn(fi.lame_encoder_version, 9), 0 );
    hv_store( info, "lame_tag_revision", 17, newSViv(fi.lame_tag_revision), 0 );
    hv_store( info, "lame_vbr_method", 15, newSVpv( vbr_methods[fi.lame_vbr_method], 0 ), 0 );
    hv_store( info, "lame_lowpass", 12, newSViv(fi.lame_lowpass), 0 );
    
    if (fi.lame_replay_gain[0]) {
      char tmp[8];
      sprintf(tmp, "%.1f dB", fi.lame_replay_gain[0]);
      hv_store( info, "lame_replay_gain_radio", 22, newSVpv( tmp, 0 ), 0 );
    }
    
    if (fi.lame_replay_gain[1]) {
      char tmp[8];
      sprintf(tmp, "%.1f dB", fi.lame_replay_gain[1]);
      hv_store( info, "lame_replay_gain_audiophile", 27, newSVpv( tmp, 0 ), 0 );
    }
    
    hv_store( info, "lame_encoder_delay", 18, newSViv(fi.lame_encoder_delay), 0 );
    hv_store( info, "lame_encoder_padding", 20, newSViv(fi.lame_encoder_padding), 0 );
    
    hv_store( info, "lame_noise_shaping", 18, newSViv(fi.lame_noise_shaping), 0 );
    hv_store( info, "lame_stereo_mode", 16, newSVpv( stereo_modes[fi.lame_stereo_mode], 0 ), 0 );
    hv_store( info, "lame_unwise_settings", 20, newSViv(fi.lame_unwise), 0 );
    hv_store( info, "lame_source_freq", 16, newSVpv( source_freqs[fi.lame_source_freq], 0 ), 0 );
    
/*
    hv_store( info, "lame_mp3gain", 12, newSViv(fi.lame_mp3gain), 0 );
    hv_store( info, "lame_mp3gain_db", 15, newSVnv(fi.lame_mp3gain_db), 0 );
*/
  
    hv_store( info, "lame_surround", 13, newSVpv( surround[fi.lame_surround], 0 ), 0 );
    
    if (fi.lame_preset < 8) {
      hv_store( info, "lame_preset", 11, newSVpvn( "Unknown", 7 ), 0 );
    }
    else if (fi.lame_preset <= 320) {
      char tmp[8];
      sprintf(tmp, "ABR %d", fi.lame_preset);
      hv_store( info, "lame_preset", 11, newSVpv( tmp, 0 ), 0 );
    }
    else if (fi.lame_preset <= 500) {
      fi.lame_preset /= 10;
      fi.lame_preset -= 41;
      if ( presets_v[fi.lame_preset] ) {
        hv_store( info, "lame_preset", 11, newSVpv( presets_v[fi.lame_preset], 0 ), 0 );
      }
    }
    else if (fi.lame_preset <= 1007) {
      fi.lame_preset -= 1000;
      if ( presets_old[fi.lame_preset] ) {
        hv_store( info, "lame_preset", 11, newSVpv( presets_old[fi.lame_preset], 0 ), 0 );
      }
    }
    
    if (fi.vbr == ABR || fi.vbr == VBR) {
      hv_store( info, "vbr", 3, newSViv(1), 0 );
    }
  }

out:
  if (infile) fclose(infile);
  free(buf_ptr);
  
  if (err) return err;

  return 0;
}
