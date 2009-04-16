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
 */

#include "flac.h"

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
