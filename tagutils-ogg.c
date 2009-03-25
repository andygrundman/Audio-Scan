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
get_ogg_metadata(char *file, HV *info, HV *tags)
{
  int i;
  struct stat st;
  int bitrate_average;
  OggVorbis_File vf;

  size_t filesize   = 0;
  double total_time = 0;

  if (stat(file, &st) == 0) {
    filesize = st.st_size;
    my_hv_store(info, "SIZE", newSViv(filesize));
  } else {
    PerlIO_printf(PerlIO_stderr(), "Couldn't stat file: [%s], might be more problems ahead!", file);
  }

  if (ov_fopen(file, &vf) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Can't open OggVorbis file: %s\n", file);
    return -1;
  }

  vorbis_comment *vc = ov_comment(&vf, -1);
  vorbis_info *vi    = ov_info(&vf, -1);
  total_time         = ov_time_total(&vf, -1);

  for (i = 0; i < vc->comments; i++) {
    _split_vorbis_comment(vc->user_comments[i], tags);
  }

  my_hv_store(info, "CHANNELS", newSViv(vi->channels));
  my_hv_store(info, "RATE", newSViv(vi->rate));
  my_hv_store(info, "VERSION", newSViv(vi->version));
  my_hv_store(info, "STEREO", newSViv(vi->channels == 2 ? 1 : 0));
  my_hv_store(info, "VENDOR", newSVpv(ov_comment(&vf, -1)->vendor, 0));
  my_hv_store(info, "SECS", newSVnv(total_time));

  bitrate_average = (int)((filesize * 8) / total_time);

  my_hv_store(info, "BITRATE", newSViv( bitrate_average ? bitrate_average : vi->bitrate_nominal ));

  if (vi->bitrate_upper && vi->bitrate_lower && (vi->bitrate_upper != vi->bitrate_lower)) {
    my_hv_store(info, "VBR_SCALE", newSViv(1));
  } else {
    my_hv_store(info, "VBR_SCALE", newSViv(0));
  }

  my_hv_store(info, "OFFSET", newSViv(0));

  ov_clear(&vf);

  return 0;
}
