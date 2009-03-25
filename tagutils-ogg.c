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
  int i;
  SV **tag       = NULL;
  SV **separator = NULL;
  OggVorbis_File vf;

  if (ov_fopen(file, &vf) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Can't open OggVorbis file: %s\n", file);
    return -1;
  }

  vorbis_comment *vc = ov_comment(&vf, -1);

  for (i = 0; i < vc->comments; i++) {
    _split_vorbis_comment(vc->user_comments[i], tags, tag, separator);
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
  my_hv_store( info, "VERSION", newSViv(vi->version) );
  my_hv_store( info, "VENDOR", newSVpv(ov_comment(&vf, -1)->vendor, 0) );

  my_hv_store( info, "BITRATE_UPPER", newSViv(vi->bitrate_upper) );
  my_hv_store( info, "BITRATE_NOMINAL", newSViv(vi->bitrate_nominal) );
  my_hv_store( info, "BITRATE_LOWER", newSViv(vi->bitrate_lower) );
  my_hv_store( info, "BITRATE_WINDOW", newSViv(vi->bitrate_window) );
  my_hv_store( info, "LENGTH", newSVnv(ov_time_total(&vf, -1)) );

  ov_clear(&vf);

  return 0;
}
