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

#include "common.h"
#include "buffer.c"

int
_check_buf(PerlIO *infile, Buffer *buf, int min_wanted, int max_wanted)
{
  int ret = 1;
  
  // Do we have enough data?
  if ( buffer_len(buf) < min_wanted ) {
    // Read more data
    uint32_t read;
    unsigned char *tmp;
    
    if (min_wanted > max_wanted) {
      max_wanted = min_wanted;
    }

    New(0, tmp, max_wanted, unsigned char);

    if ( (read = PerlIO_read(infile, tmp, max_wanted)) <= 0 ) {
      if ( PerlIO_error(infile) ) {
        warn("Error reading: %s (wanted %d)\n", strerror(errno), max_wanted);
      }
      else {
        warn("Error: Unable to read at least %d bytes from file.\n", min_wanted);
      }

      ret = 0;
      goto out;
    }

    buffer_append(buf, tmp, read);

    // Make sure we got enough
    if ( buffer_len(buf) < min_wanted ) {
      warn("Error: Unable to read at least %d bytes from file (only read %d).\n", min_wanted, read);
      ret = 0;
      goto out;
    }

    DEBUG_TRACE("Buffered %d bytes from file @ %d (min_wanted %d, max_wanted %d)\n",
      read, (int)PerlIO_tell(infile) - read, min_wanted, max_wanted
    );

out:
    Safefree(tmp);
  }

  return ret;
}

char* upcase(char *s) {
  char *p = &s[0];

  while (*p != 0) {
    *p = toUPPER(*p);
    p++;
  }

  return s;
}

void _split_vorbis_comment(char* comment, HV* tags) {
  char *half;
  char *key;
  int klen  = 0;
  SV* value = NULL;

  if (!comment) {
    DEBUG_TRACE("Empty comment, skipping...\n");
    return;
  }

  /* store the pointer location of the '=', poor man's split() */
  half = strchr(comment, '=');

  if (half == NULL) {
    DEBUG_TRACE("Comment \"%s\" missing \'=\', skipping...\n", comment);
    return;
  }

  klen  = half - comment;
  value = newSVpv(half + 1, 0);
  sv_utf8_decode(value);

  /* Is there a better way to do this? */
  New(0, key, klen + 1, char);
  Move(comment, key, klen, char);
  key[klen] = '\0';
  key = upcase(key);

  if (hv_exists(tags, key, klen)) {
    /* fetch the existing key */
    SV **entry = my_hv_fetch(tags, key);

    if (SvOK(*entry)) {

      // A normal string entry, convert to array.
      if (SvTYPE(*entry) == SVt_PV) {
        AV *ref = newAV();
        av_push(ref, newSVsv(*entry));
        av_push(ref, value);
        my_hv_store(tags, key, newRV_noinc((SV*)ref));

      } else if (SvTYPE(SvRV(*entry)) == SVt_PVAV) {
        av_push((AV *)SvRV(*entry), value);
      }
    }

  } else {
    my_hv_store(tags, key, value);
  }

  Safefree(key);
}

int32_t
skip_id3v2(PerlIO* infile) {
  unsigned char buf[10];
  uint32_t has_footer;
  int32_t  size;

  // seek to first byte of mpc data
  if (PerlIO_seek(infile, 0, SEEK_SET) < 0)
    return 0;

  PerlIO_read(infile, &buf, sizeof(buf));

  // check id3-tag
  if (memcmp(buf, "ID3", 3) != 0)
    return 0;

  // read flags
  has_footer = buf[5] & 0x10;

  if (buf[5] & 0x0F)
    return -1;

  if ((buf[6] | buf[7] | buf[8] | buf[9]) & 0x80)
    return -1;

  // read header size (syncsave: 4 * $0xxxxxxx = 28 significant bits)
  size  = buf[6] << 21;
  size += buf[7] << 14;
  size += buf[8] <<  7;
  size += buf[9]      ;
  size += 10;

  if (has_footer)
    size += 10;

  return size;
}

uint32_t
_bitrate(uint32_t audio_size, uint32_t song_length_ms)
{
  return ( (audio_size * 1.0) / song_length_ms ) * 8000;
}

off_t
_file_size(PerlIO *infile)
{
#ifdef _MSC_VER
  // Win32 doesn't work right with fstat
  off_t file_size;
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  return file_size;
#else
  struct stat buf;
  
  if ( !fstat( PerlIO_fileno(infile), &buf ) ) {
    return buf.st_size;
  }
  
  warn("Unable to stat: %s\n", strerror(errno));
  
  return 0;
#endif
}

int
_env_true(const char *name)
{
  char *value;
  
  value = getenv(name);
  
  if ( value == NULL || value[0] == '0' ) {
    return 0;
  }
  
  return 1;
}

// from http://jeremie.com/frolic/base64/
int
_decode_base64(char *s)
{
  char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int bit_offset, byte_offset, idx, i, n;
  unsigned char *d = (unsigned char *)s;
  char *p;

  n = i = 0;
  
  while (*s && (p=strchr(b64,*s))) {
    idx = (int)(p - b64);
    byte_offset = (i*6)/8;
    bit_offset = (i*6)%8;
    d[byte_offset] &= ~((1<<(8-bit_offset))-1);
    
    if (bit_offset < 3) {
      d[byte_offset] |= (idx << (2-bit_offset));
      n = byte_offset+1;
    }
    else {
      d[byte_offset] |= (idx >> (bit_offset-2));
      d[byte_offset+1] = 0;
      d[byte_offset+1] |= (idx << (8-(bit_offset-2))) & 0xFF;
      n = byte_offset+2;
    }
    s++;
    i++;
  }
  
  /* null terminate */
  d[n] = 0;
  
  return n;
}

HV *
_decode_flac_picture(PerlIO *infile, Buffer *buf, uint32_t *pic_length)
{
  uint32_t mime_length;
  uint32_t desc_length;
  SV *desc;
  HV *picture = newHV();
  
  // Check we have enough for picture_type and mime_length
  if ( !_check_buf(infile, buf, 8, DEFAULT_BLOCK_SIZE) ) {
    return NULL;
  }
    
  my_hv_store( picture, "picture_type", newSVuv( buffer_get_int(buf) ) );
  
  mime_length = buffer_get_int(buf);
  DEBUG_TRACE("  mime_length: %d\n", mime_length);
  if (mime_length > buffer_len(buf)) {
    return NULL;
  }
  
  // Check we have enough for mime_type and desc_length
  if ( !_check_buf(infile, buf, mime_length + 4, DEFAULT_BLOCK_SIZE) ) {
    return NULL;
  }
  
  my_hv_store( picture, "mime_type", newSVpvn( buffer_ptr(buf), mime_length ) );
  buffer_consume(buf, mime_length);
  
  desc_length = buffer_get_int(buf);
  DEBUG_TRACE("  desc_length: %d\n", mime_length);
  if (desc_length > buffer_len(buf)) {
    return NULL;
  }
  
  // Check we have enough for desc_length, width, height, depth, color_index, pic_length
  if ( !_check_buf(infile, buf, desc_length + 20, DEFAULT_BLOCK_SIZE) ) {
    return NULL;
  }
  
  desc = newSVpvn( buffer_ptr(buf), desc_length );
  sv_utf8_decode(desc); // XXX needs test with utf8 desc
  my_hv_store( picture, "description", desc );
  buffer_consume(buf, desc_length);
  
  my_hv_store( picture, "width", newSVuv( buffer_get_int(buf) ) );
  my_hv_store( picture, "height", newSVuv( buffer_get_int(buf) ) );
  my_hv_store( picture, "depth", newSVuv( buffer_get_int(buf) ) );
  my_hv_store( picture, "color_index", newSVuv( buffer_get_int(buf) ) );
  
  *pic_length = buffer_get_int(buf);
  DEBUG_TRACE("  pic_length: %d\n", *pic_length);
  
  if ( _env_true("AUDIO_SCAN_NO_ARTWORK") ) {
    my_hv_store( picture, "image_data", newSVuv(*pic_length) );
  }
  else {
    if ( !_check_buf(infile, buf, *pic_length, *pic_length) ) {
      return NULL;
    }
    
    my_hv_store( picture, "image_data", newSVpvn( buffer_ptr(buf), *pic_length ) );
  }
  
  return picture;
}
