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

// Use Safefree for libid3tag free calls on Windows
#ifdef _MSC_VER
#define free(ptr) Safefree(ptr)
#endif

#define BLOCK_SIZE     4096
#define WANTED_FOR_AVG 32768

#define XING_FRAMES  0x01
#define XING_BYTES   0x02
#define XING_TOC     0x04
#define XING_QUALITY 0x08

#define CBR 1
#define ABR 2
#define VBR 3

struct mp3_frameinfo {
  short mpeg_version;
  unsigned char layer;
  unsigned short bitrate;
  unsigned int samplerate;
  unsigned char stereo;
  unsigned char vbr;

  unsigned short frame_length;
  unsigned char crc_protected;
  unsigned short samples_per_frame;
  unsigned char padding;

  // Xing header
  unsigned int xing_offset;
  unsigned int xing_frames;
  unsigned int xing_bytes;
  unsigned short xing_quality;

  // LAME header
  char lame_encoder_version[9];
  unsigned char lame_tag_revision;
  unsigned char lame_vbr_method;
  unsigned int lame_lowpass;
  float lame_replay_gain[2];
  unsigned short lame_abr_rate;
  short lame_encoder_delay;
  short lame_encoder_padding;
  unsigned char lame_noise_shaping;
  unsigned char lame_stereo_mode;
  unsigned char lame_unwise;
  unsigned char lame_source_freq;
  int lame_mp3gain;
  float lame_mp3gain_db;
  unsigned char lame_surround;
  unsigned short lame_preset;
  unsigned int lame_music_length;

  // VBRI header
  unsigned short vbri_delay;
  unsigned short vbri_quality;
  unsigned int vbri_bytes;
  unsigned int vbri_frames;
};

// LAME lookup tables
const char *stereo_modes[] = {
  "Mono",
  "Stereo",
  "Dual",
  "Joint",
  "Force",
  "Auto",
  "Intensity",
  "Undefined"
};

const char *source_freqs[] = {
  "<= 32 kHz",
  "44.1 kHz",
  "48 kHz",
  "> 48 kHz"
};

const char *surround[] = {
  "None",
  "DPL encoding",
  "DPL2 encoding",
  "Ambisonic encoding",
  "Reserved"
};

const char *vbr_methods[] = {
  "Unknown",
  "Constant Bitrate",
  "Average Bitrate",
  "Variable Bitrate method1 (old/rh)",
  "Variable Bitrate method2 (mtrh)",
  "Variable Bitrate method3 (mt)",
  "Variable Bitrate method4",
  NULL,
  "Constant Bitrate (2 pass)",
  "Average Bitrate (2 pass)",
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  "Reserved"
};

const char *presets_v[] = {
  "V9",
  "V8",
  "V7",
  "V6",
  "V5",
  "V4",
  "V3",
  "V2",
  "V1",
  "V0"
};

const char *presets_old[] = {
  "r3mix",
  "standard",
  "extreme",
  "insane",
  "standard/fast",
  "extreme/fast",
  "medium",
  "medium/fast"
};

static int get_mp3tags(PerlIO *infile, char *file, HV *info, HV *tags);
static int get_mp3fileinfo(PerlIO *infile, char *file, HV *info);
static int _decode_mp3_frame(unsigned char *frame, struct mp3_frameinfo *pfi);
static int mp3_find_frame(PerlIO *infile, char *file, int offset);
static int _has_ape(PerlIO *infile);

// bitrate_tbl[layer_index][bitrate_index]
static int bitrate_tbl[5][16] = {
  { 0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0 }, /* MPEG1, L1 */
  { 0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0 },    /* MPEG1, L2 */
  { 0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0 },     /* MPEG1, L3 */
  { 0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0 },    /* MPEG2/2.5, L1 */
  { 0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0 }          /* MPEG2/2.5, L2/L3 */
};

// sample_rate[sample_index][samplerate_index]
static int sample_rate_tbl[3][4] = {
  { 44100, 48000, 32000, 0 },  /* MPEG 1 */
  { 22050, 24000, 16000, 0 },  /* MPEG 2 */
  { 11025, 12000, 8000, 0 }    /* MPEG 2.5 */
};
