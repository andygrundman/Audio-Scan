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

void _split_vorbis_comment(char* comment, HV* tags, SV** tag, SV** separator) {
  char *half;
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
  } else {
    klen  = half - comment;
    value = newSVpv(half + 1, 0);
    //sv_utf8_decode(value);
    //sv_utf8_upgrade(value);
  }

  if (hv_exists(tags, comment, klen)) {
    /* fetch the existing key */
    tag = hv_fetch(tags, comment, klen, 0);

    /* fetch the multi-value separator or default and append to the key */
    if (my_hv_exists(tags, "separator")) {
      separator = my_hv_fetch(tags, "separator");
      sv_catsv(*tag, *separator);
    } else {
      sv_catpv(*tag, "/");
    }

    /* concatenate with the new key */
    sv_catsv(*tag, value);
  } else {
    hv_store(tags, comment, klen, value, 0);
  }
}
