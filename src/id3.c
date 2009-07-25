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

#include "id3.h"
 
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
parse_id3(PerlIO *infile, char *file, HV *info, HV *tags, uint32_t seek)
{
  struct id3_file *pid3file;
  struct id3_tag *pid3tag;
  struct id3_frame *pid3frame;
  enum id3_file_mode mode = ID3_FILE_MODE_READONLY;
  int err = 0;
  int index;
  unsigned int nstrings;
  unsigned char trck_found = 0; // whether we found a track tag, used to determine ID3v1 from ID3v1.1

  id3_ucs4_t const *key;
  id3_ucs4_t const *value;
  char *utf8_key;
  char *utf8_value;
  
  if (seek) {
    mode = ID3_FILE_MODE_READONLY_NOSEEK;
  }

  pid3file = id3_file_fdopen( PerlIO_fileno(infile), mode, seek );
  if (!pid3file) {
    PerlIO_printf(PerlIO_stderr(), "libid3tag cannot open %s\n", file);
    err = -1;
    goto out;
  }

  pid3tag = id3_file_tag(pid3file);

  if (!pid3tag) {
    err = errno;
    id3_file_close(pid3file);
    errno = err;
    PerlIO_printf(PerlIO_stderr(), "libid3tag cannot get ID3 tag for %s\n", file);
    goto out;
  }
  
  DEBUG_TRACE("Found %d ID3 frames\n", pid3tag->nframes);

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
                const id3_byte_t *rva;
                id3_length_t len;
                float adj = 0.0;
                int adj_fp;
                uint8_t peakbits;
                float peak = 0.0;
                
                rva = id3_field_getbinarydata(&pid3frame->fields[i], &len);
                
                // Channel
                av_push( framedata, newSViv(rva[0]) );
                rva++;
                
                // Adjustment
                adj_fp = *(signed char *)(rva) << 8;
                adj_fp |= *(unsigned char *)(rva+1);
                adj = adj_fp / 512.0;
                av_push( framedata, newSVpvf( "%f dB", adj ) );
                rva += 2;
                
                // Peak
                // Based on code from mp3gain
                peakbits = rva[0];
                if (4 + (peakbits + 7) / 8 <= len) {
                  DEBUG_TRACE("  peakbits: %d\n", peakbits);
                  if (peakbits > 0)
                    peak += (float)rva[1];
                  if (peakbits > 8)
                    peak += (float)rva[2] / 256.0;
                  if (peakbits > 16)
                    peak += (float)rva[3] / 65536.0;
                  
                  if (peakbits > 0)
                    peak /= (float)(1 << ((peakbits - 1) & 7));                    
                }
                
                av_push( framedata, newSVpvf( "%f dB", peak ) );
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

out:
  return err;
}
