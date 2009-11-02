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

    if ( (read = PerlIO_read(infile, tmp, max_wanted)) == 0 ) {
      if ( PerlIO_error(infile) ) {
        PerlIO_printf(PerlIO_stderr(), "Error reading: %s (wanted %d)\n", strerror(errno), max_wanted);
      }
      else {
        PerlIO_printf(PerlIO_stderr(), "Error: Unable to read %d bytes from file.\n", max_wanted);
      }

      ret = 0;
      goto out;
    }

    buffer_append(buf, tmp, read);

    // Make sure we got enough
    if ( buffer_len(buf) < min_wanted ) {
      PerlIO_printf(PerlIO_stderr(), "Error: Unable to read at least %d bytes from file (only read %d).\n", min_wanted, read);
      ret = 0;
      goto out;
    }

    DEBUG_TRACE("Buffered %d bytes from file (min_wanted %d, max_wanted %d)\n", read, min_wanted, max_wanted);

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
    PerlIO_printf(PerlIO_stderr(), "Empty comment, skipping...\n");
    return;
  }

  /* store the pointer location of the '=', poor man's split() */
  half = strchr(comment, '=');

  if (half == NULL) {
    PerlIO_printf(PerlIO_stderr(), "Comment \"%s\" missing \'=\', skipping...\n", comment);
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
  
  PerlIO_printf(PerlIO_stderr(), "Unable to stat: %s\n", strerror(errno));
  
  return 0;
#endif
}