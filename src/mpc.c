/*
 * Original Copyright:
 *
  Copyright (c) 2005, The Musepack Development Team
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the following
  disclaimer in the documentation and/or other materials provided
  with the distribution.

  * Neither the name of the The Musepack Development Team nor the
  names of its contributors may be used to endorse or promote
  products derived from this software without specific prior
  written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mpc.h"

#define MPC_HEADER_SIZE 32

// profile is 0...15, where 7...13 is used
static const char *
_mpc_profile_string(uint32_t profile)
{
  static const char na[] = "n.a.";
  static const char *names[] = {
    na, "'Unstable/Experimental'", na, na,
    na, "'quality 0'", "'quality 1'", "'Telephone'",
    "'Thumb'", "'Radio'", "'Standard'", "'Xtreme'",
    "'Insane'", "'BrainDead'", "'quality 9'", "'quality 10'"
  };

  return profile >= sizeof(names) / sizeof(*names) ? na : names[profile];
}

// not yet implemented
static int32_t
_mpc_read_header_sv8(mpc_streaminfo * si, PerlIO *infile)
{
  (void) si;
  (void) infile;
  return 0;
}

static int32_t
_mpc_read_header_sv7(mpc_streaminfo * si, uint32_t header[8])
{
  const int32_t samplefreqs[4] = { 44100, 48000, 37800, 32000 };

  uint16_t estimated_peak_title = 0;

  // Update (si->stream_version);
  if (si->stream_version > 0x71) {
    return 0;
  }

  si->bitrate            = 0;
  si->frames             = header[1];
  si->is                 = 0;
  si->ms                 = (header[2] >> 30) & 0x0001;
  si->max_band           = (header[2] >> 24) & 0x003F;
  si->block_size         = 1;
  si->profile            = (header[2] << 8) >> 28;
  si->profile_name       = _mpc_profile_string(si->profile);
  si->sample_freq        = samplefreqs[(header[2] >> 16) & 0x0003];
  estimated_peak_title   = (uint16_t) (header[2] & 0xFFFF);   // read the ReplayGain data
  si->gain_title         = (uint16_t) ((header[3] >> 16) & 0xFFFF);
  si->peak_title         = (uint16_t) (header[3] & 0xFFFF);
  si->gain_album         = (uint16_t) ((header[4] >> 16) & 0xFFFF);
  si->peak_album         = (uint16_t) (header[4] & 0xFFFF);
  si->is_true_gapless    = (header[5] >> 31) & 0x0001; // true gapless: used?
  si->last_frame_samples = (header[5] >> 20) & 0x07FF;  // true gapless: valid samples for last frame
  si->encoder_version    = (header[6] >> 24) & 0x00FF;
  si->channels           = 2;

  if (si->encoder_version == 0) {

    sprintf(si->encoder, "Buschmann 1.7.0...9, Klemm 0.90...1.05");

  } else {
    switch (si->encoder_version % 10) {
    case 0:
      sprintf(si->encoder, "Release %u.%u", si->encoder_version / 100, si->encoder_version / 10 % 10);
      break;
    case 2:
    case 4:
    case 6:
    case 8:
      sprintf(si->encoder, "Beta %u.%02u", si->encoder_version / 100, si->encoder_version % 100);
      break;
    default:
      sprintf(si->encoder, "--Alpha-- %u.%02u", si->encoder_version / 100, si->encoder_version % 100);
      break;
    }
  }

  return 0;
}

static int32_t
_mpc_read_header_sv6(mpc_streaminfo * si, uint32_t header[8])
{
  si->bitrate             = (header[0] >> 23) & 0x01FF;   // read the file-header (SV6 and below)
  si->is                  = (header[0] >> 22) & 0x0001;
  si->ms                  = (header[0] >> 21) & 0x0001;
  si->stream_version      = (header[0] >> 11) & 0x03FF;
  si->max_band            = (header[0] >> 6) & 0x001F;
  si->block_size          = (header[0]) & 0x003F;
  si->profile             = 0;
  si->profile_name        = _mpc_profile_string((uint32_t) (-1));
  si->last_frame_samples  = 0;
  si->is_true_gapless     = 0;
  si->encoder_version     = 0;
  si->encoder[0]          = '\0';
  si->sample_freq         = 44100;     // AB: used by all files up to SV7
  si->channels            = 2;

  // Not supported
  si->gain_title          = 0;
  si->peak_title          = 0;
  si->gain_album          = 0;
  si->peak_album          = 0;

  if (si->stream_version >= 5) {
    si->frames = header[1]; // 32 bit
  } else {
    si->frames = (header[1] >> 16); // 16 bit
  }

  if (si->stream_version == 7)
    return -1;  // are there any unsupported parameters used?

  if (si->bitrate != 0)
    return -2;

  if (si->is != 0)
    return -3;

  if (si->block_size != 1)
    return -4;

  if (si->stream_version < 6) // Bugfix: last frame was invalid for up to SV5
    si->frames -= 1;

  if (si->stream_version < 4 || si->stream_version > 7)
    return -5;

  return 0;
}

static int
get_mpcfileinfo(PerlIO *infile, char *file, HV *info)
{
  uint32_t header[8];
  int32_t ret = 0;

  mpc_streaminfo *si;

  Newxz(si, sizeof(mpc_streaminfo), mpc_streaminfo);

  // get header position
  if ((si->header_position = skip_id3v2(infile)) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Musepack: [Couldn't skip ID3v2]: %s\n", file);
    Safefree(si);
    return -1;
  }

  // seek to first byte of mpc data
  if (PerlIO_seek(infile, si->header_position, SEEK_SET) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Musepack: [Couldn't seek to offset %d]: %s\n", si->header_position, file);
    Safefree(si);
    return -1;
  }

  if (PerlIO_read(infile, &header, MPC_HEADER_SIZE) != MPC_HEADER_SIZE) {
    PerlIO_printf(PerlIO_stderr(), "Musepack: [Couldn't read]: %s\n", file);
    Safefree(si);
    return -1;
  }

  if (PerlIO_seek(infile, si->header_position + 6 * 4, SEEK_SET) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Musepack: [Couldn't seek to offset %d + (6*4)]: %s\n", si->header_position, file);
    Safefree(si);
    return -1;
  }

  si->tag_offset = PerlIO_tell(infile);

  if (PerlIO_seek(infile, 0, SEEK_END) < 0) {
    PerlIO_printf(PerlIO_stderr(), "Musepack: [Couldn't seek to end of file.]: %s\n", file);
    Safefree(si);
    return -1;
  }

  si->total_file_length = PerlIO_tell(infile);

  if (memcmp(header, "MP+", 3) == 0) {

    si->stream_version = header[0] >> 24;

    if ((si->stream_version & 15) == 7) {
      ret = _mpc_read_header_sv7(si, header);
    }

  } else if (memcmp(header, "MPCK", 4) == 0) {
    ret = _mpc_read_header_sv8(si, infile);
  } else {
    ret = _mpc_read_header_sv6(si, header);
  }

  // estimation, exact value needs too much time
  si->pcm_samples = 1152 * si->frames - 576;

  if (si->pcm_samples > 0) {
    si->average_bitrate = (si->tag_offset - si->header_position) * 8.0 * si->sample_freq / si->pcm_samples;
  } else {
    si->average_bitrate = 0;
  }

  if (ret == 0) {
    double total_seconds = (double)( (si->pcm_samples * 1.0) / si->sample_freq);

    my_hv_store(info, "samplerate", newSViv(si->sample_freq));
    my_hv_store(info, "channels", newSViv(si->channels));
    my_hv_store(info, "song_length_ms", newSVuv(total_seconds * 1000));
    my_hv_store(info, "bitrate", newSVuv(8 * (double)(si->total_file_length - si->tag_offset) / total_seconds));
    
    my_hv_store(info, "audio_offset", newSVuv(si->tag_offset));
    my_hv_store(info, "file_size", newSVuv(si->total_file_length));
    my_hv_store(info, "encoder", newSVpv(si->encoder, 0));
    my_hv_store(info, "profile", newSVpv(si->profile_name, 0));
  }

  Safefree(si);

  return ret;
}
