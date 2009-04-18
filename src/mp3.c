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
_varint(unsigned char *buf, int length)
{
  int i, b, number = 0;
  
  if (buf) {
    for ( i = 0; i < length; i++ ) { 
      b = length - 1 - i;                                                       
      number = number | (unsigned int)( buf[i] & 0xff ) << ( 8*b );
    }
    return number;
  }
  else {
    return 0;
  }
}

static int
get_mp3tags(PerlIO *infile, char *file, HV *info, HV *tags)
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

  pid3file = id3_file_fdopen( PerlIO_fileno(infile), ID3_FILE_MODE_READONLY );
  if (!pid3file) {
    PerlIO_printf(PerlIO_stderr(), "libid3tag cannot open %s\n", file);
    return -1;
  }

  pid3tag = id3_file_tag(pid3file);

  if (!pid3tag) {
    err = errno;
    id3_file_close(pid3file);
    errno = err;
    PerlIO_printf(PerlIO_stderr(), "libid3tag cannot get ID3 tag for %s\n", file);
    return -1;
  }

  index = 0;
  while ((pid3frame = id3_tag_findframe(pid3tag, "", index))) {
    key = NULL;
    value = NULL;
    utf8_key = NULL;
    utf8_value = NULL;

    DEBUG_TRACE("%s (%d fields)\n", pid3frame->id, pid3frame->nfields);

    // Special handling for TXXX/WXXX frames
    if ( !strcmp(pid3frame->id, "TXXX") || !strcmp(pid3frame->id, "WXXX") ) {
      DEBUG_TRACE("  type %d / %d\n", pid3frame->fields[1].type, pid3frame->fields[2].type);
      
      key = id3_field_getstring(&pid3frame->fields[1]);
      if (key) {
        // Get the key
        utf8_key = (char *)id3_ucs4_utf8duplicate(key);
        if ( strlen(utf8_key) ) {
          SV *ktmp = newSVpv( upcase(utf8_key), 0 );
          sv_utf8_decode(ktmp);

          // Get the value
          switch (pid3frame->fields[2].type) {
            case ID3_FIELD_TYPE_LATIN1:
              my_hv_store_ent( tags, ktmp, newSVpv( (char *)id3_field_getlatin1(&pid3frame->fields[2]), 0 ) );
              break;
          
            case ID3_FIELD_TYPE_STRING:
              value = id3_field_getstring(&pid3frame->fields[2]);
              if (value) {
                SV *tmp;
                utf8_value = (char *)id3_ucs4_utf8duplicate(value);
                tmp = newSVpv( utf8_value, 0 );
                sv_utf8_decode(tmp);
                my_hv_store_ent( tags, ktmp, tmp );
                free(utf8_value);
              }
              else {
                my_hv_store_ent( tags, ktmp, &PL_sv_undef );
              }
              break;
            
            default:
              break;
          }
          
          // Don't leak
          SvREFCNT_dec(ktmp);
        }

        free(utf8_key);
      }
    }

    // Special handling for TCON genre frame, lookup the genre string
    else if ( !strcmp(pid3frame->id, "TCON") ) {
      char *genre_string;

      value = id3_field_getstrings(&pid3frame->fields[1], 0);
      if (value) {
        utf8_value = (char *)id3_ucs4_utf8duplicate(value);

        if ( isdigit(utf8_value[0]) ) {
          // Convert to genre string
          genre_string = (char *)id3_ucs4_utf8duplicate( id3_genre_name(value) );
          my_hv_store( tags, pid3frame->id, newSVpv( genre_string, 0 ) );
          free(genre_string);
        }
        else if ( utf8_value[0] == '(' && isdigit(utf8_value[1]) ) {
          // handle '(26)Ambient'
          int genre_num = (int)strtol( (char *)&utf8_value[1], NULL, 0 );
          if (genre_num > 0 && genre_num < 148) {
            genre_string = (char *)id3_ucs4_utf8duplicate( id3_genre_index(genre_num) );
            my_hv_store( tags, pid3frame->id, newSVpv( genre_string, 0 ) );
            free(genre_string);
          }
        }
        else {
          SV *tmp = newSVpv( utf8_value, 0 );
          sv_utf8_decode(tmp);
          my_hv_store( tags, pid3frame->id, tmp );
        }

        free(utf8_value);
      }
    }

    // All other frames
    else {
      DEBUG_TRACE("  type %d\n", pid3frame->fields[0].type);
      
      // For some reason libid3tag marks some frames as obsolete, when
      // they should at least be passed-through as unknown frames
      if ( !strcmp(pid3frame->id, "ZOBS") ) {
        char *frameid = pid3frame->fields[0].immediate.value;
        
        DEBUG_TRACE("  ZOBS frame %s\n", frameid);
        
        // Special case, TYE(R), TDA(T), TIM(E) are already converted to TDRC
          if (
               !strcmp(frameid, "TYER") 
            || !strcmp(frameid, "YTYE")
            || !strcmp(frameid, "TDAT") 
            || !strcmp(frameid, "YTDA")
            || !strcmp(frameid, "TIME") 
            || !strcmp(frameid, "YTIM")
          ) {
            index++;
            continue;
        }
        
        // Convert this frame into the real frame with 1 field of binary data
        pid3frame->id[0] = frameid[0];
        pid3frame->id[1] = frameid[1];
        pid3frame->id[2] = frameid[2];
        pid3frame->id[3] = frameid[3];
        
        pid3frame->nfields = 1;
        pid3frame->fields[0] = pid3frame->fields[1];
      }
      
      // 1- and 2-field frames where the first field is TEXTENCODING are mapped to plain hash entries
      // This covers the following frames:
      // MCDI - ID3_FIELD_TYPE_BINARYDATA (untested)
      // PCNT - ID3_FIELD_TYPE_INT32PLUS
      // SEEK - ID3_FIELD_TYPE_INT32 (untested)
      // T* (text) - ID3_FIELD_TYPE_TEXTENCODING, ID3_FIELD_TYPE_STRINGLIST
      // W* (url) - ID3_FIELD_TYPE_LATIN1
      // unknown - ID3_FIELD_TYPE_BINARYDATA
      
      if ( 
           pid3frame->nfields == 1 
        || ( pid3frame->nfields == 2 && (pid3frame->fields[0].type == ID3_FIELD_TYPE_TEXTENCODING) )
      ) {
        int i = pid3frame->nfields - 1;
        
        switch (pid3frame->fields[i].type) {
          case ID3_FIELD_TYPE_LATIN1:
            my_hv_store( tags, pid3frame->id, newSVpv( (char *)id3_field_getlatin1(&pid3frame->fields[i]), 0 ) );
            break;
          
          case ID3_FIELD_TYPE_STRINGLIST:
            nstrings = id3_field_getnstrings(&pid3frame->fields[i]);
            if (nstrings > 1) {
              // Store multiple strings as arrayref
              AV *atmp = newAV();
              int j;
              
              for ( j = 0; j < nstrings; j++ ) {
                value = id3_field_getstrings(&pid3frame->fields[i], j);
                if (value) {
                  SV *tmp;
                  utf8_value = (char *)id3_ucs4_utf8duplicate(value);
                  tmp = newSVpv( utf8_value, 0 );
                  sv_utf8_decode(tmp);
                  av_push( atmp, tmp );
                  free(utf8_value);
                }
                else {
                  av_push( atmp, &PL_sv_undef );
                }
              }
              
              my_hv_store( tags, pid3frame->id, newRV_noinc( (SV *)atmp ) );
            }
            else {
              // Remember if TRCK tag is found for ID3v1.1
              if ( !strcmp(pid3frame->id, "TRCK") ) {
                trck_found = 1;
              }
              
              value = id3_field_getstrings(&pid3frame->fields[i], 0);  
              if (value) {
                SV *tmp;
                utf8_value = (char *)id3_ucs4_utf8duplicate(value);
                tmp = newSVpv( utf8_value, 0 );
                sv_utf8_decode(tmp);
                my_hv_store( tags, pid3frame->id, tmp );
                free(utf8_value);
              }
              else {
                my_hv_store( tags, pid3frame->id, &PL_sv_undef );
              }
            }
            break;
          
          case ID3_FIELD_TYPE_INT32:
            my_hv_store( tags, pid3frame->id, newSViv( pid3frame->fields[i].number.value ) );
            break;
          
          case ID3_FIELD_TYPE_INT32PLUS:
            my_hv_store( 
              tags,
              pid3frame->id,
              newSViv( _varint( pid3frame->fields[i].binary.data, pid3frame->fields[i].binary.length ) )
            );
            break;
          
          case ID3_FIELD_TYPE_BINARYDATA:
            if ( !strcmp(pid3frame->id, "XHD3" ) ) {
              // Ignore XHD3 frame from stupid new mp3HD format
            }
            // Special handling for RVA(D)
            else if ( !strcmp( pid3frame->id, "RVAD" ) || !strcmp( pid3frame->id, "YRVA" ) ) {
              unsigned char *rva = (unsigned char*)pid3frame->fields[0].binary.data;
              int8_t sign_r = rva[0] & 0x01 ? 1 : -1;
              int8_t sign_l = rva[0] & 0x02 ? 1 : -1;
              uint8_t bytes = rva[1] / 8;
              float vol[2];
              float peak[2];
              uint8_t i;
              AV *framedata = newAV();

              rva += 2;

              vol[0] = _varint( rva, bytes ) * sign_r / 256.;
              vol[1] = _varint( rva + bytes, bytes ) * sign_l / 256.;

              peak[0] = _varint( rva + (bytes * 2), bytes );
              peak[1] = _varint( rva + (bytes * 3), bytes );

              // iTunes uses a range of -255 to 255
    					// to be -100% (silent) to 100% (+6dB)
              for (i = 0; i < 2; i++) {
                if ( vol[i] == -255 ) {
                  vol[i] = -96.0;
                }
                else {
                  vol[i] = 20.0 * log( ( vol[i] + 255 ) / 255 ) / log(10);
                }

                av_push( framedata, newSVpvf( "%f dB", vol[i] ) );
                av_push( framedata, newSVpvf( "%f", peak[i] ) );
              }

              my_hv_store( tags, pid3frame->id, newRV_noinc( (SV *)framedata ) );
            }
            else {
              char *data = (char*)pid3frame->fields[0].binary.data;
              unsigned int len = pid3frame->fields[0].binary.length;
              SV *bin;
                            
              // Consume leading and trailing padding nulls on binary data, these are left over
              // from unknown text frames from i.e. iTunes
              while ( len && !data[0] ) {
                data++;
                len--;
              }
              
              while ( len && !data[len - 1] ) {
                len--;
              }
              
              bin = newSVpvn( data, len );
              my_hv_store( tags, pid3frame->id, bin );
            }
          
          default:
            break;
        }
      }
      
      // 2+ field frames are mapped to arrayrefs
      // This covers the following frames:
      // UFID, ETCO, MLLT, SYTC, USLT, SYLT, COMM, RVA2, EQU2, RVRB,
      // APIC, GEOB, POPM, AENC, LINK, POSS, USER, OWNE, COMR, ENCR,
      // GRID, PRIV, SIGN, ASPI
      else {        
        int i;
        SV *tmp;
        AV *framedata = newAV();
        
        for ( i = 0; i < pid3frame->nfields; i++ ) { 
          DEBUG_TRACE("  frame %d, type %d\n", i, pid3frame->fields[i].type);
          
          switch (pid3frame->fields[i].type) {
            case ID3_FIELD_TYPE_LATIN1:
              av_push( framedata, newSVpv( (char *)id3_field_getlatin1(&pid3frame->fields[i]), 0 ) );
              break;
            
            // ID3_FIELD_TYPE_LATIN1FULL - not used
            
            case ID3_FIELD_TYPE_LATIN1LIST: // XXX untested, LINK frame
              nstrings = id3_field_getnstrings(&pid3frame->fields[i]);
              if (nstrings > 1) {
                // XXX, turn into an arrayref
                PerlIO_printf(PerlIO_stderr(), "LATIN1LIST, %d strings\n", nstrings );
              }
              else {
                av_push( framedata, newSVpv( (char*)pid3frame->fields[i].latin1list.strings[0], 0 ) );
              }
              break;
            
            case ID3_FIELD_TYPE_STRING:
              utf8_value = (char *)id3_ucs4_utf8duplicate( id3_field_getstring(&pid3frame->fields[i]) );
              tmp = newSVpv( utf8_value, 0 );
              sv_utf8_decode(tmp);
              av_push( framedata, tmp );
              free(utf8_value);
              break;
            
            case ID3_FIELD_TYPE_STRINGFULL:
              utf8_value = (char *)id3_ucs4_utf8duplicate( id3_field_getfullstring(&pid3frame->fields[i]) );
              tmp = newSVpv( utf8_value, 0 );
              sv_utf8_decode(tmp);
              av_push( framedata, tmp );
              free(utf8_value);
              break;
            
            // ID3_FIELD_TYPE_STRINGLIST - only used for text frames, handled above
          
            case ID3_FIELD_TYPE_LANGUAGE:
            case ID3_FIELD_TYPE_FRAMEID: // XXX untested, LINK frame
            case ID3_FIELD_TYPE_DATE:    // XXX untested, OWNE/COMR
              av_push( framedata, newSVpv( pid3frame->fields[i].immediate.value, 0 ) );
              break;
            
            case ID3_FIELD_TYPE_INT8:
            case ID3_FIELD_TYPE_INT16:
            case ID3_FIELD_TYPE_INT24:
            case ID3_FIELD_TYPE_INT32:
            case ID3_FIELD_TYPE_TEXTENCODING:
              av_push( framedata, newSViv( pid3frame->fields[i].number.value ) );
              break;
              
            case ID3_FIELD_TYPE_INT32PLUS:
              av_push( framedata, newSViv( _varint( pid3frame->fields[i].binary.data, pid3frame->fields[i].binary.length ) ) );
              break;

            case ID3_FIELD_TYPE_BINARYDATA:
              // Special handling for RVA2 tags, expand to correct fields
              if ( !strcmp( pid3frame->id, "RVA2" ) ) {
                unsigned char *rva = (unsigned char*)pid3frame->fields[i].binary.data;
                float adj = 0.0;
                int adj_fp;
                
                // Channel
                av_push( framedata, newSViv(rva[0]) );
                rva++;
                
                // Adjustment
                adj_fp = *(signed char *)(rva) << 8;
                adj_fp |= *(unsigned char *)(rva+1);
                adj = adj_fp / 512.0;
                av_push( framedata, newSVpvf( "%f dB", adj ) );
                rva += 2;
                
                // Ignore peak, nobody seems to support this
                av_push( framedata, newSViv(0) );
              }
              else {
                SV *bin = newSVpvn( (char*)pid3frame->fields[i].binary.data, pid3frame->fields[i].binary.length );
                av_push( framedata, bin );
              }

            default:
              break;
          }
        }
        
        // If tag already exists, move it into an arrayref
        if ( my_hv_exists( tags, pid3frame->id ) ) {
          SV **entry = my_hv_fetch( tags, pid3frame->id );
          if (entry != NULL) {
            if ( SvTYPE( SvRV(*entry) ) == SVt_PV ) {
              // A normal string entry, convert to array
              // XXX untested
              AV *ref = newAV();
              av_push( ref, *entry );
              av_push( ref, newRV_noinc( (SV *)framedata ) );
              my_hv_store( tags, pid3frame->id, newRV_noinc( (SV *)ref ) );
            }
            else if ( SvTYPE( SvRV(*entry) ) == SVt_PVAV ) {
              // If type of first item is array, add new item to entry
              SV **first = av_fetch( (AV *)SvRV(*entry), 0, 0 );
              if ( first == NULL || ( SvTYPE(*first) == SVt_RV && SvTYPE( SvRV(*first) ) == SVt_PVAV ) ) {
                av_push( (AV *)SvRV(*entry), newRV_noinc( (SV *)framedata ) );
              }
              else {
                AV *ref = newAV();
                av_push( ref, SvREFCNT_inc(*entry) );
                av_push( ref, newRV_noinc( (SV *)framedata ) );
                my_hv_store( tags, pid3frame->id, newRV_noinc( (SV *)ref ) );
              }
            }
          }
        }
        else {
          my_hv_store( tags, pid3frame->id, newRV_noinc( (SV *)framedata ) );
        }
      }
    }

    index++;
  }

  // Update id3_version field if we found a v1 tag
  if ( pid3tag->options & ID3_TAG_OPTION_ID3V1 && !my_hv_fetch( info, "id3_version" ) ) {
    if (trck_found == 1) {
      my_hv_store( info, "id3_version", newSVpv( "ID3v1.1", 0 ) );
    }
    else {
      my_hv_store( info, "id3_version", newSVpv( "ID3v1", 0 ) );
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
static short _mp3_get_average_bitrate(PerlIO *infile)
{
  struct mp3_frameinfo fi;
  int frame_count   = 0;
  int bitrate_total = 0;
  int err = 0;

  unsigned char *buf;
  unsigned char *buf_ptr;
  unsigned int buf_size = 0;
  
  Newxz(buf, WANTED_FOR_AVG, char);
  buf_ptr = buf;

  // Seek to middle of file
  PerlIO_seek(infile, 0, SEEK_END);
  PerlIO_seek(infile, PerlIO_tell(infile) >> 1, SEEK_SET);

  if ((buf_size = PerlIO_read(infile, buf, WANTED_FOR_AVG)) == 0) {
    if (PerlIO_error(infile)) {
      PerlIO_printf(PerlIO_stderr(), "Error reading: %s\n", strerror(errno));
    }
    else {
      PerlIO_printf(PerlIO_stderr(), "File too small. Probably corrupted.\n");
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
  if (buf_ptr) Safefree(buf_ptr);

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
get_mp3fileinfo(PerlIO *infile, char *file, HV *info)
{
  struct mp3_frameinfo fi;

  unsigned char *buf;
  unsigned char *buf_ptr;
  char id3v1taghdr[4];

  unsigned int id3_size = 0; // size of leading ID3 data
  unsigned int buf_size = 0; // amount of data left in buf

  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to first audio frame
  off_t audio_size;          // size of all audio frames

  int song_length_ms = 0;    // duration of song in ms
  int bitrate        = 0;    // actual bitrate of song

  int found;
  int err = 0;
  
  Newxz(buf, BLOCK_SIZE, char);
  buf_ptr = buf;

  memset((void*)&fi, 0, sizeof(fi));

  PerlIO_seek(infile,0,SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile,0,SEEK_SET);

  if ((buf_size = PerlIO_read(infile, buf, BLOCK_SIZE)) == 0) {
    if (PerlIO_error(infile)) {
      PerlIO_printf(PerlIO_stderr(), "Error reading: %s\n", strerror(errno));
    }
    else {
      PerlIO_printf(PerlIO_stderr(), "File too small. Probably corrupted.\n");
    }

    err = -1;
    goto out;
  }

  if (
    (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') &&
    buf[3] < 0xff && buf[4] < 0xff &&
    buf[6] < 0x80 && buf[7] < 0x80 && buf[8] < 0x80 && buf[9] < 0x80
  ) {
    /* found an ID3 header... */
    id3_size = 10 + (buf[6]<<21) + (buf[7]<<14) + (buf[8]<<7) + buf[9];

    if (buf[5] & 0x10) {
      // footer present
      id3_size += 10;
    }
    
    my_hv_store( info, "id3_version", newSVpvf( "ID3v2.%d.%d", buf[3], buf[4] ) );

    // Always seek past the ID3 tags
    PerlIO_seek(infile, id3_size, SEEK_SET);
    buf_size = PerlIO_read(infile, buf, BLOCK_SIZE);

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
        PerlIO_printf(PerlIO_stderr(), "Unable to find any MP3 frames in file (checked 4K): %s\n", file);
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
    PerlIO_printf(PerlIO_stderr(), "Unable to find any MP3 frames in file (checked 4K): %s\n", file);
    err = -1;
    goto out;
  }

  audio_size = file_size - audio_offset;

  // check if last 128 bytes is ID3v1.0 or ID3v1.1 tag
  PerlIO_seek(infile, file_size - 128, SEEK_SET);
  if (PerlIO_read(infile, id3v1taghdr, 4) == 4) {
    if (id3v1taghdr[0]=='T' && id3v1taghdr[1]=='A' && id3v1taghdr[2]=='G') {
      audio_size -= 128;
    }
  }

  if ( _decode_mp3_frame(buf, &fi) ) {
    PerlIO_printf(PerlIO_stderr(), "Could not find sync frame: %s\n", file);
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
    bitrate = ( fi.xing_bytes / fi.xing_frames * mfs );
  }

  // Or use VBRI header
  else if (fi.vbri_frames && fi.vbri_bytes) {
    float mfs = (float)fi.samplerate / ( fi.mpeg_version == 0x25 ? 72000. : 144000. );
    bitrate = ( fi.vbri_bytes / fi.vbri_frames * mfs );
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
    else if (fi.lame_preset <= 1007) {
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
  if (buf_ptr) Safefree(buf_ptr);

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
  
  PerlIO_seek(infile, offset, SEEK_SET);
  
  buffer_init(&mp3_buf, BLOCK_SIZE);
  
  if ( !_check_buf(infile, &mp3_buf, BLOCK_SIZE, BLOCK_SIZE) ) {
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
  }

out:
  buffer_free(&mp3_buf);

  return frame_offset;
}
