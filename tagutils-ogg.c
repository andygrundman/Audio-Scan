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
get_ogginfo(char *file, HV *info, HV *tags)
{
  PerlIO *infile;

  unsigned char *buf = malloc(BLOCK_SIZE);
  unsigned char *buf_ptr = buf;

  unsigned int id3_size = 0; // size of leading ID3 data
  unsigned int buf_size = 0; // amount of data left in buf

  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to first audio frame
  off_t audio_size;          // size of all audio frames

  unsigned char channels;
  unsigned int blocksize_0;
  unsigned int avg_buf_size;
  unsigned int samplerate;
  unsigned int bitrate_nominal;
  uint64_t granule_pos;

  int i;
  int err = 0;

  if (!(infile = PerlIO_open(file, "rb"))) {
    PerlIO_printf(PerlIO_stderr(), "Could not open %s for reading\n", file);
    err = -1;
    goto out;
  }

  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);

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

  // Skip ID3 tags if any
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

    PerlIO_seek(infile, id3_size, SEEK_SET);
    buf_size = PerlIO_read(infile, buf, BLOCK_SIZE);

    audio_offset += id3_size;
  }

  // check that the first four bytes are 'OggS'
  if ( buf[0] != 'O' || buf[1] != 'g' || buf[2] != 'g' || buf[3] != 'S' ) {
    PerlIO_printf(PerlIO_stderr(), "Not an Ogg file (bad header): %s\n", file);
    goto out;
  }

  buf += 4;

  // check the stream structure version (1 byte, should be 0x00)
  if ( buf[0] != 0x00 ) {
    PerlIO_printf(PerlIO_stderr(), "Not an Ogg file (bad stream structure version): %s\n", file);
    goto out;
  }

  buf += 1;

  // check the header type flag
  // This is a bitfield, so technically we should check all of the bits
  // that could potentially be set. However, the only value this should
  // possibly have at the beginning of a proper Ogg-Vorbis file is 0x02
  if ( buf[0] != 0x02 ) {
    PerlIO_printf(PerlIO_stderr(), "Not an Ogg file (bad header type flag): %s\n", file);
    goto out;
  }

  buf += 21;

  // read the number of page segments and skip
  buf += (int)buf[0] + 1;

  // check packet type. Should be 0x01 (for indentification header)
  if ( buf[0] != 0x01 ) {
    PerlIO_printf(PerlIO_stderr(), "Not a Vorbis file (bad identification header): %s\n", file);
    goto out;
  }

  buf += 1;

  // Verify 'vorbis' string
  if ( strncmp( (char *)buf, "vorbis", 6) ) {
    PerlIO_printf(PerlIO_stderr(), "Not a Vorbis file (bad vorbis header): %s\n", file);
    goto out;
  }

  buf += 6;

  my_hv_store( info, "version", newSViv( GET_INT32LE(buf) ) );

  channels = *buf++;
  my_hv_store( info, "stereo", newSViv( channels == 2 ? 1 : 0 ) );

  samplerate = GET_INT32LE(buf);
  my_hv_store( info, "samplerate", newSViv(samplerate) );
  my_hv_store( info, "bitrate_upper", newSViv( GET_INT32LE(buf) ) );

  bitrate_nominal = GET_INT32LE(buf);
  my_hv_store( info, "bitrate_nominal", newSViv(bitrate_nominal) );
  my_hv_store( info, "bitrate_lower", newSViv( GET_INT32LE(buf) ) );

  blocksize_0 = 2 << ((buf[0] & 0xF0) >> 4);
  my_hv_store( info, "blocksize_0", newSViv( blocksize_0 ) );
  my_hv_store( info, "blocksize_1", newSViv( 2 << (buf[0] & 0x0F) ) );
  buf++;

  free(buf_ptr);

  // XXX: original code used blocksize_0 * 2, is that correct?
  if ( file_size > blocksize_0 ) {
    avg_buf_size = blocksize_0;
  }
  else {
    avg_buf_size = file_size;
  }

  buf = malloc(avg_buf_size);
  buf_ptr = buf;

  // Calculate average bitrate
  PerlIO_seek(infile, file_size - avg_buf_size, SEEK_SET);

  if ((buf_size = PerlIO_read(infile, buf, avg_buf_size)) == 0) {
    if (PerlIO_error(infile)) {
      PerlIO_printf(PerlIO_stderr(), "Error reading: %s\n", strerror(errno));
    }
    else {
      PerlIO_printf(PerlIO_stderr(), "File too small. Probably corrupted.\n");
    }

    err = -1;
    goto out;
  }

  // Find sync
  while ( buf_size >= 14 && (buf[0] != 'O' || buf[1] != 'g' || buf[2] != 'g' || buf[3] != 'S') ) {
    buf++;
    buf_size--;

    if ( buf_size < 14 ) {
      // Give up, use less accurate bitrate for length
      my_hv_store( info, "song_length_ms", newSVpvf( "%d", ((file_size * 8) / bitrate_nominal) * 1000) );
      my_hv_store( info, "bitrate_average", newSViv(bitrate_nominal) );

      goto out;
    }
  }

  buf += 6;

  // Get absolute granule value
  granule_pos = (uint64_t)GET_INT32LE(buf);
  granule_pos |= (uint64_t)GET_INT32LE(buf) << 32;

  if ( granule_pos && samplerate ) {
    int length = (int)((granule_pos / samplerate) * 1000);
    my_hv_store( info, "song_length_ms", newSViv(length) );
    my_hv_store( info, "bitrate_average", newSVpvf( "%d", ( file_size * 8 ) / ( length / 1000 ) ) );
  }
  else {
    // Use nominal bitrate
    my_hv_store( info, "song_length_ms", newSVpvf( "%d", ((file_size * 8) / bitrate_nominal) * 1000) );
    my_hv_store( info, "bitrate_average", newSViv(bitrate_nominal) );
  }

out:
  if (infile) PerlIO_close(infile);
  if (buf_ptr) free(buf_ptr);

  if (err) return err;

  return 0;
}

// Using the Vorbis API is sloooow
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
