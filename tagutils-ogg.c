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

#include "tagutils-ogg.h"

static int
get_oggtags(char *file, HV *info, HV *tags)
{
  OggVorbis_File vf;

  if (ov_fopen(file, &vf) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Can't open OggVorbis file: %s\n", file);
    return -1;
  }

  SV **tag       = NULL;
  SV **separator = NULL;
  int i = 0;

  vorbis_comment *vc = ov_comment(&vf, -1);
  char **comments    = vc->user_comments;
  int num_comments   = vc->comments;
  int *comment_lens  = vc->comment_lengths;

  for (i = 0; i < num_comments; i++) {

    if (!comments[i] || !comment_lens[i]) {
      PerlIO_printf(PerlIO_stderr(), "Empty comment, skipping...\n");
      continue;
    }

    /* store the pointer location of the '=', poor man's split() */
    char *key  = comments[i];
    char *half = strchr(key, '=');
    int  klen  = 0;
    SV* value  = NULL;

    if (half == NULL) {
      PerlIO_printf(PerlIO_stderr(), "Comment \"%s\" missing \'=\', skipping...\n", key);
      continue;
    } else {
      klen  = half - key;
      value = newSVpv(half + 1, 0);
      //sv_utf8_decode(value);
      //sv_utf8_upgrade(value);
    }

    if (hv_exists(tags, key, klen)) {
      /* fetch the existing key */
      tag = hv_fetch(tags, key, klen, 0);

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
      hv_store(tags, key, klen, value, 0);
    }
  }

  ov_clear(&vf);

  return 0;
}

static int
get_ogginfo(char *file, HV *info)
{
  OggVorbis_File vf;

  if (ov_fopen(file, &vf) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Can't open OggVorbis file: %s\n", file);
    return -1;
  }

  vorbis_info *vi = ov_info(&vf, -1);

  my_hv_store( info, "CHANNELS", newSViv(vi->channels) );
  my_hv_store( info, "SAMPLERATE", newSViv(vi->rate) );
  my_hv_store( info, "VENDOR", newSVpv(ov_comment(&vf, -1)->vendor, 0) );

  ov_clear(&vf);

  return 0;
}
