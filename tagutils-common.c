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

#include "tagutils-common.h"
#include "buffer.c"

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
  Newx(key, klen + 1, char);
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
