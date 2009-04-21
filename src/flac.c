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

/*
 * This file is derived from Audio::FLAC::Header by Dan Sully.
 *
 * CRC code comes from libFLAC/crc.c
 */

#include "flac.h"

/* frame header size (16 bytes) + 4608 stereo 16-bit samples (higher than 4608 is possible, but not done) */
#define FLAC_FRAME_MAX_BLOCK 18448
#define FLAC_HEADER_LEN 16

/* CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0 */
FLAC__byte const FLAC__crc8_table[256] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
	0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
	0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
	0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
	0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
	0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
	0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
	0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
	0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
	0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
	0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
	0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
	0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
	0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
	0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
	0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
	0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
	0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
	0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
	0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
	0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
	0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
	0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
	0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
	0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
	0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
	0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
	0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
	0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
	0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
	0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
	0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

FLAC__uint8 FLAC__crc8(const FLAC__byte *data, unsigned len) {
  FLAC__uint8 crc = 0;

  while(len--)
    crc = FLAC__crc8_table[crc ^ *data++];

  return crc;
}

void _cuesheet_frame_to_msf(unsigned frame, unsigned *minutes, unsigned *seconds, unsigned *frames) {

  *frames = frame % 75;
  frame /= 75;
  *seconds = frame % 60;
  frame /= 60;
  *minutes = frame;
}

void _read_metadata(char *path, HV *info, HV *tags, FLAC__StreamMetadata *block, unsigned block_number) {

  unsigned i;
  int storePicture = 0;

  HV *pictureContainer = newHV();
  AV *allpicturesContainer = NULL;

  switch (block->type) {

    case FLAC__METADATA_TYPE_STREAMINFO:
    {
      float totalSeconds;

      my_hv_store(info, "minimum_blocksize", newSVuv(block->data.stream_info.min_blocksize));
      my_hv_store(info, "maximum_blocksize", newSVuv(block->data.stream_info.max_blocksize));

      my_hv_store(info, "minimum_framesize", newSVuv(block->data.stream_info.min_framesize));
      my_hv_store(info, "maximum_framesize", newSVuv(block->data.stream_info.max_framesize));

      my_hv_store(info, "samplerate", newSVuv(block->data.stream_info.sample_rate));
      my_hv_store(info, "channels", newSVuv(block->data.stream_info.channels));
      my_hv_store(info, "bits_per_sample", newSVuv(block->data.stream_info.bits_per_sample));
      my_hv_store(info, "total_samples", newSVnv(block->data.stream_info.total_samples));

      if (block->data.stream_info.md5sum != NULL) {

        /* Initialize an SV with the first element,
           and then append to it. If we don't do it this way, we get a "use of
           uninitialized element" in subroutine warning. */
        SV *md5 = newSVpvf("%02x", (unsigned)block->data.stream_info.md5sum[0]);

        for (i = 1; i < 16; i++) {
          sv_catpvf(md5, "%02x", (unsigned)block->data.stream_info.md5sum[i]);
        }

        my_hv_store(info, "md5", md5);
      }

      /* Store some other metadata for backwards compatability with the original Audio::FLAC */
      /* needs to be higher resolution */
      totalSeconds = block->data.stream_info.total_samples / (float)block->data.stream_info.sample_rate;

      if (totalSeconds <= 0) {
        PerlIO_printf(PerlIO_stderr(), "File: %s - %s\n%s\n",
          path,
          "totalSeconds is 0 - we couldn't find either TOTALSAMPLES or SAMPLERATE!",
          "setting totalSeconds to 1 to avoid divide by zero error!"
        );

        totalSeconds = 1;
      }

      my_hv_store(info, "song_length_ms", newSViv(totalSeconds * 1000));
      my_hv_store(info, "frames", newSVnv((totalSeconds - (int)totalSeconds) * 75));

      break;
    }

    case FLAC__METADATA_TYPE_PADDING:
    case FLAC__METADATA_TYPE_SEEKTABLE:
      /* Don't handle these yet. */
      break;

    case FLAC__METADATA_TYPE_APPLICATION:
    {
      if (block->data.application.id[0]) {
        HV *app   = newHV();
        SV *tmpId = newSVpvf("%02x", (unsigned)block->data.application.id[0]);
        SV *appId;

        for (i = 1; i < 4; i++) {
          sv_catpvf(tmpId, "%02x", (unsigned)block->data.application.id[i]);
        }

        // Be compatible with the pure perl version
        appId = newSVpvf("%ld", strtol(SvPV_nolen(tmpId), NULL, 16));

        if (block->data.application.data != 0) {
          my_hv_store_ent(app, appId, newSVpvn((char*)block->data.application.data, block->length));
        }

        my_hv_store(tags, "APPLICATION",  newRV_noinc((SV*) app));

        SvREFCNT_dec(tmpId);
        SvREFCNT_dec(appId);
      }

      break;
    }

    case FLAC__METADATA_TYPE_VORBIS_COMMENT:
    {
      if (block->data.vorbis_comment.vendor_string.entry) {
        my_hv_store(tags, "VENDOR", newSVpv((char*)block->data.vorbis_comment.vendor_string.entry, 0));
      }

      for (i = 0; i < block->data.vorbis_comment.num_comments; i++) {
        _split_vorbis_comment((char*)block->data.vorbis_comment.comments[i].entry, tags);
      }

      break;
    }

    case FLAC__METADATA_TYPE_CUESHEET:
    {
      AV *cueArray = newAV();

      /*
       * buffer for decimal representations of uint64_t values
       *
       * newSVpvf() and sv_catpvf() can't handle 64-bit values
       * in some cases, so we need to do the conversion "manually"
       * with sprintf() and the PRIu64 format macro for portability
       *
       * see http://bugs.debian.org/462249
       *
       * maximum string length: ceil(log10(2**64)) == 20 (+trailing \0)
       */
      char decimal[21];

      /* A lot of this comes from flac/src/share/grabbag/cuesheet.c */
      const FLAC__StreamMetadata_CueSheet *cs;
      unsigned track_num, index_num;

      cs = &block->data.cue_sheet;

      if (*(cs->media_catalog_number)) {
        av_push(cueArray, newSVpvf("CATALOG %s\n", cs->media_catalog_number));
      }

      av_push(cueArray, newSVpvf("FILE \"%s\" FLAC\n", path));

      for (track_num = 0; track_num < cs->num_tracks-1; track_num++) {

        const FLAC__StreamMetadata_CueSheet_Track *track = cs->tracks + track_num;

        av_push(cueArray, newSVpvf("  TRACK %02u %s\n",
          (unsigned)track->number, track->type == 0? "AUDIO" : "DATA"
        ));

        if (track->pre_emphasis) {
          av_push(cueArray, newSVpv("    FLAGS PRE\n", 0));
        }

        if (*(track->isrc)) {
          av_push(cueArray, newSVpvf("    ISRC %s\n", track->isrc));
        }

        for (index_num = 0; index_num < track->num_indices; index_num++) {

          const FLAC__StreamMetadata_CueSheet_Index *index = track->indices + index_num;

          SV *indexSV = newSVpvf("    INDEX %02u ", (unsigned)index->number);

          if (cs->is_cd) {

            unsigned logical_frame = (unsigned)((track->offset + index->offset) / (44100 / 75));
            unsigned m, s, f;

            _cuesheet_frame_to_msf(logical_frame, &m, &s, &f);

            sv_catpvf(indexSV, "%02u:%02u:%02u\n", m, s, f);

          } else {
            sprintf(decimal, "%"PRIu64, track->offset + index->offset);
            sv_catpvf(indexSV, "%s\n", decimal);
          }


          av_push(cueArray, indexSV);
        }
      }

      sprintf(decimal, "%"PRIu64, cs->lead_in);
      av_push(cueArray, newSVpvf("REM FLAC__lead-in %s\n", decimal));
      sprintf(decimal, "%"PRIu64, cs->tracks[track_num].offset);
      av_push(cueArray, newSVpvf("REM FLAC__lead-out %u %s\n",
        (unsigned)cs->tracks[track_num].number, decimal)
      );

      my_hv_store(tags, "CUESHEET",  newRV_noinc((SV*) cueArray));

      break;
    }

/* The PICTURE metadata block came about in FLAC 1.1.3 */
#ifdef FLAC_API_VERSION_CURRENT
    case FLAC__METADATA_TYPE_PICTURE:
    {
      HV *picture = newHV();
      SV *type;

      my_hv_store(picture, "mime_type", newSVpv(block->data.picture.mime_type, 0));
      my_hv_store(picture, "description", newSVpv((const char*)block->data.picture.description, 0));
      my_hv_store(picture, "width", newSViv(block->data.picture.width));
      my_hv_store(picture, "height", newSViv(block->data.picture.height));
      my_hv_store(picture, "depth", newSViv(block->data.picture.depth));
      my_hv_store(picture, "color_index", newSViv(block->data.picture.colors));
      my_hv_store(picture, "image_data", newSVpv((const char*)block->data.picture.data, block->data.picture.data_length));
      my_hv_store(picture, "picture_type", newSViv(block->data.picture.type));

      type = newSViv(block->data.picture.type);

      my_hv_store_ent(
        pictureContainer,
        type,
        newRV_noinc((SV*) picture)
      );

      SvREFCNT_dec(type);

      storePicture = 1;

      /* update allpictures */
      if (my_hv_exists(tags, "ALLPICTURES")) {
        allpicturesContainer = (AV *) SvRV(*my_hv_fetch(tags, "ALLPICTURES"));
      } else {
        allpicturesContainer = newAV();

        /* store the 'allpictures' array */
        my_hv_store(tags, "ALLPICTURES", newRV_noinc((SV*) allpicturesContainer));
      }

      av_push(allpicturesContainer, (SV*) newRV((SV*) picture));

      break;
    }
#endif

    /* XXX- Just ignore for now */
    default:
      break;
  }

  /* store the 'picture' hash */
  if (storePicture && hv_scalar(pictureContainer)) {
    my_hv_store(tags, "PICTURE", newRV_noinc((SV*) pictureContainer));
  } else {
    SvREFCNT_dec((SV*) pictureContainer);
  }
}

/* From src/metaflac/operations.c */
void print_error_with_chain_status(FLAC__Metadata_Chain *chain, const char *format, ...) {

  const FLAC__Metadata_ChainStatus status = FLAC__metadata_chain_status(chain);
  va_list args;

  FLAC__ASSERT(0 != format);

  va_start(args, format);
  (void) PerlIO_vprintf(PerlIO_stderr(), format, args);
  va_end(args);

  PerlIO_printf(PerlIO_stderr(), "status = \"%s\"\n", FLAC__Metadata_ChainStatusString[status]);

  if (status == FLAC__METADATA_CHAIN_STATUS_ERROR_OPENING_FILE) {

    PerlIO_printf(PerlIO_stderr(), "The FLAC file could not be opened. Most likely the file does not exist or is not readable.");

  } else if (status == FLAC__METADATA_CHAIN_STATUS_NOT_A_FLAC_FILE) {

    PerlIO_printf(PerlIO_stderr(), "The file does not appear to be a FLAC file.");

  } else if (status == FLAC__METADATA_CHAIN_STATUS_NOT_WRITABLE) {

    PerlIO_printf(PerlIO_stderr(), "The FLAC file does not have write permissions.");

  } else if (status == FLAC__METADATA_CHAIN_STATUS_BAD_METADATA) {

    PerlIO_printf(PerlIO_stderr(), "The metadata to be writted does not conform to the FLAC metadata specifications.");

  } else if (status == FLAC__METADATA_CHAIN_STATUS_READ_ERROR) {

    PerlIO_printf(PerlIO_stderr(), "There was an error while reading the FLAC file.");

  } else if (status == FLAC__METADATA_CHAIN_STATUS_WRITE_ERROR) {

    PerlIO_printf(PerlIO_stderr(), "There was an error while writing FLAC file; most probably the disk is full.");

  } else if (status == FLAC__METADATA_CHAIN_STATUS_UNLINK_ERROR) {

    PerlIO_printf(PerlIO_stderr(), "There was an error removing the temporary FLAC file.");
  }
}

static int
get_flac_metadata(PerlIO *infile, char *file, HV *info, HV *tags)
{
  FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

  if (chain == 0) {
    PerlIO_printf(PerlIO_stderr(), "Out of memory allocating chain. Cannot open %s\n", file);
    return -1;
  }

  if (!FLAC__metadata_chain_read(chain, file)) {
    print_error_with_chain_status(chain, "%s: ERROR: reading metadata", file);
    FLAC__metadata_chain_delete(chain);
    return -1;
  }

  {
    FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
    FLAC__StreamMetadata *block = 0;
    FLAC__bool ok = true;
    unsigned block_number = 0;

    if (iterator == 0) {
      PerlIO_printf(PerlIO_stderr(), "Out of memory allocating iterator. Cannot open %s\n", file);
      FLAC__metadata_chain_delete(chain);
      return -1;
    }

    FLAC__metadata_iterator_init(iterator, chain);

    do {
      block = FLAC__metadata_iterator_get_block(iterator);
      ok &= (0 != block);

      if (!ok) {
        PerlIO_printf(PerlIO_stderr(), "%s: ERROR: couldn't get block from chain.\n", file);
      } else {
        _read_metadata(file, info, tags, block, block_number);
      }

      block_number++;

    } while (ok && FLAC__metadata_iterator_next(iterator));

    FLAC__metadata_iterator_delete(iterator);
  }

  FLAC__metadata_chain_delete(chain);

  {
    /* Find the offset of the start pos for audio blocks (ie: after metadata) */
    unsigned int  is_last = 0;
    unsigned char buf[4];
    long len;
    struct stat st;
    float totalMS;

    if (PerlIO_read(infile, &buf, 4) == -1) {
      PerlIO_printf(PerlIO_stderr(), "Couldn't read magic fLaC header! %s\n", strerror(errno));
      return -1;
    }

    if (memcmp(buf, ID3HEADERFLAG, 3) == 0) {

      unsigned id3size = 0;
      int c = 0;

      /* How big is the ID3 header? Skip the next two bytes */
      if (PerlIO_read(infile, &buf, 2) == -1) {
        PerlIO_printf(PerlIO_stderr(), "Couldn't read ID3 header length! %s\n", strerror(errno));
        return -1;
      }

      /* The size of the ID3 tag is a 'synchsafe' 4-byte uint */
      for (c = 0; c < 4; c++) {

        if (PerlIO_read(infile, &buf, 1) == -1 || buf[0] & 0x80) {
          PerlIO_printf(PerlIO_stderr(), "Couldn't read ID3 header length (syncsafe)! %s\n", strerror(errno));
          return -1;
        }

        id3size <<= 7;
        id3size |= (buf[0] & 0x7f);
      }

      if (PerlIO_seek(infile, id3size, SEEK_CUR) < 0) {
        PerlIO_printf(PerlIO_stderr(), "Couldn't seek past ID3 header!\n");
        return -1;
      }

      if (PerlIO_read(infile, &buf, 4) == -1) {
        PerlIO_printf(PerlIO_stderr(), "Couldn't read magic fLaC header! %s\n", strerror(errno));
        return -1;
      }
    }

    if (memcmp(buf, FLACHEADERFLAG, 4)) {
      PerlIO_printf(PerlIO_stderr(), "Couldn't read magic fLaC header - got gibberish instead!\n");
      return -1;
    }

    while (!is_last) {

      if (PerlIO_read(infile, &buf, 4) == -1) {
        PerlIO_printf(PerlIO_stderr(), "Couldn't read 4 bytes of the metadata block!\n");
        return -1;
      }

      is_last = (unsigned int)(buf[0] & 0x80);

      len = (long)((buf[1] << 16) | (buf[2] << 8) | (buf[3]));

      PerlIO_seek(infile, len, SEEK_CUR);
    }

    len = PerlIO_tell(infile);

    my_hv_store(info, "audio_offset", newSVnv(len));

    /* Now calculate the bit rate and file size */
    if (my_hv_exists(info, "song_length_ms")) {

      totalMS = (float)SvIV(*(my_hv_fetch(info, "song_length_ms")));

      /* Find the file size */
      if (stat(file, &st) == 0) {
        my_hv_store(info, "file_size", newSViv(st.st_size));
      } else {
        PerlIO_printf(PerlIO_stderr(), "Couldn't stat file: [%s], might be more problems ahead!", file);
      }

      my_hv_store(info, "bitrate", newSVnv(8 * (st.st_size - len) / (totalMS / 1000) ));
    }
  }

  return 0;
}

bool
_is_flac_header(unsigned char *buf)
{
  FLAC__uint32 sync1, sync2, block_size, sample_rate, channel, sample_size, padding;

  sync1       = buf[0];
  sync2       = buf[1] >> 2;
  block_size  = buf[2] >> 4;
  sample_rate = buf[2] & 0x0F;
  channel     = buf[3] >> 4;
  sample_size = (buf[3] >> 1) & 0x07;
  padding     = buf[3] & 0x1;

  if (sync1 != 0xFF || sync2 != 0x3E || sample_rate == 0xF || channel > 0xC || sample_size == 0x3 || sample_size == 0x7 || padding) {
    return false;
  }

  /*
  fprintf(stderr, "Found FLAC header: block_size: %d, sample_rate: %d, channel: %d, sample_size: %d, padding: %d\n",
    block_size, sample_rate, channel, sample_size, padding
  );
  */

  size_t len = 4;

  if (!(buf[4] & 0x80)) {
    len += 1;
  } else if(buf[4] & 0xC0 && !(buf[4] & 0x20)) {
    len += 2;
  } else if(buf[4] & 0xE0 && !(buf[4] & 0x10)) {
    len += 3;
  } else if(buf[4] & 0xF0 && !(buf[4] & 0x08)) {
    len += 4;
  } else if(buf[4] & 0xF8 && !(buf[4] & 0x04)) {
    len += 5;
  } else if(buf[4] & 0xFC && !(buf[4] & 0x02)) {
    len += 6;
  } else if(buf[4] & 0xFE && !(buf[4] & 0x01)) {
    len += 7;
  }

  // Block size can be stored at the end of the header
  if (block_size == 0x6) {
    len += 1;
  } else if (block_size == 0x7) {
    len += 2;
  }

  // Sample rate can be stored at the end of the header
  if (sample_rate == 0xC) {
    len += 1;
  } else if (sample_rate == 0xD || sample_rate == 0xE) {
    len += 2;
  } else if (block_size == 0xD || block_size == 0xE) {
    // XXX: I'm not sure why this is part of the sample_rate if block, bug?
    len += 2;
  }

  FLAC__byte crc8 = buf[len];

  if (FLAC__crc8(buf, len) != crc8) {
    fprintf(stderr, "CRC FAILED\n");
    return false;
  }

  return true;
}

static int
flac_find_frame(PerlIO *infile, char *file, int offset)
{
  Buffer flac_buf;
  unsigned char *bptr;
  unsigned int buf_size;
  int frame_offset = -1;
  FLAC__uint64 pos = 0;

  PerlIO_seek(infile, offset, SEEK_SET);

  buffer_init(&flac_buf, FLAC_FRAME_MAX_BLOCK);

  if (!_check_buf(infile, &flac_buf, FLAC_FRAME_MAX_BLOCK, FLAC_FRAME_MAX_BLOCK)) {
    goto out;
  }

  bptr = (unsigned char *)buffer_ptr(&flac_buf);
  buf_size = buffer_len(&flac_buf);

  for (pos = 0; pos != (buf_size - FLAC_HEADER_LEN); pos += 1) {

    if (bptr[pos] != 0xFF)
      continue;

    if (!_is_flac_header(&bptr[pos]))
      continue;

    frame_offset = offset + pos;
    break;
  }

out:
  buffer_free(&flac_buf);

  return frame_offset;
}
