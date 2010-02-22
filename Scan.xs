#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "common.c"
#include "ape.c"
#include "id3.c"

#include "aac.c"
#include "asf.c"
#include "mac.c"
#include "mp3.c"
#include "mp4.c"
#include "mpc.c"
#include "ogg.c"
#include "wav.c"
#include "flac.c"
#include "wavpack.c"

#define FILTER_TYPE_INFO 0x01
#define FILTER_TYPE_TAGS 0x02

struct _types {
  char *type;
  char *suffix[15];
};

typedef struct {
  char*	type;
  int (*get_tags)(PerlIO *infile, char *file, HV *info, HV *tags);
  int (*get_fileinfo)(PerlIO *infile, char *file, HV *tags);
  int (*find_frame)(PerlIO *infile, char *file, int offset);
} taghandler;

struct _types audio_types[] = {
  {"mp4", {"mp4", "m4a", "m4b", "m4p", "m4v", "m4r", "k3g", "skm", "3gp", "3g2", "mov", 0}},
  {"aac", {"aac", 0}},
  {"mp3", {"mp3", "mp2", 0}},
  {"ogg", {"ogg", "oga", 0}},
  {"mpc", {"mpc", "mp+", "mpp", 0}},
  {"ape", {"ape", "apl", 0}},
  {"flc", {"flc", "flac", "fla", 0}},
  {"asf", {"wma", "asf", "wmv", 0}},
  {"wav", {"wav", "aif", "aiff", 0}},
  {"wvp", {"wv", 0}},
  {0, {0, 0}}
};

static taghandler taghandlers[] = {
  { "mp4", get_mp4tags, 0, mp4_find_frame },
  { "aac", get_aacinfo, 0, 0 },
  { "mp3", get_mp3tags, get_mp3fileinfo, mp3_find_frame },
  { "ogg", get_ogg_metadata, 0, ogg_find_frame },
  { "mpc", get_ape_metadata, get_mpcfileinfo, 0 },
  { "ape", get_ape_metadata, get_macfileinfo, 0 },
  { "flc", get_flac_metadata, 0, flac_find_frame },
  { "asf", get_asf_metadata, 0, asf_find_frame },
  { "wav", get_wav_metadata, 0, 0 },
  { "wvp", get_ape_metadata, get_wavpack_info, 0 },
  { NULL, 0, 0, 0 }
};

static taghandler *
_get_taghandler(char *suffix)
{
  int typeindex = -1;
  int i, j;
  taghandler *hdl = NULL;
  
  for (i=0; typeindex==-1 && audio_types[i].type; i++) {
    for (j=0; typeindex==-1 && audio_types[i].suffix[j]; j++) {
#ifdef _MSC_VER
      if (!stricmp(audio_types[i].suffix[j], suffix)) {
#else
      if (!strcasecmp(audio_types[i].suffix[j], suffix)) {
#endif
        typeindex = i;
        break;
      }
    }
  }
    
  if (typeindex > -1) {
    for (hdl = taghandlers; hdl->type; ++hdl)
      if (!strcmp(hdl->type, audio_types[typeindex].type))
        break;
  }
  
  return hdl;
}

HV *
_scan( char *suffix, PerlIO *infile, SV *path, uint8_t filter )
{
  taghandler *hdl;
  HV *out = newHV();
  
  // don't leak
  sv_2mortal( (SV*)out );
  
  hdl = _get_taghandler(suffix);
  
  if (hdl) {
    HV *info = newHV();

    // Ignore filter if a file type has only one function (FLAC/Ogg)
    if ( !hdl->get_fileinfo ) {
      filter = FILTER_TYPE_INFO | FILTER_TYPE_TAGS;
    }

    if ( hdl->get_fileinfo && (filter & FILTER_TYPE_INFO) ) {
      hdl->get_fileinfo(infile, SvPVX(path), info);
    }

    if ( hdl->get_tags && (filter & FILTER_TYPE_TAGS) ) {
      HV *tags = newHV();
      hdl->get_tags(infile, SvPVX(path), info, tags);
      hv_store( out, "tags", 4, newRV_noinc( (SV *)tags ), 0 );
    }

    // Info may be used in tag function, i.e. to find tag version
    hv_store( out, "info", 4, newRV_noinc( (SV *)info ), 0 );
  }
  else {
    croak("Audio::Scan unsupported file type: %s (%s)", suffix, SvPVX(path));
  }
  
  return out;
}

int
_find_frame( char *suffix, PerlIO *infile, SV *path, int offset )
{
  int frame = -1;
  taghandler *hdl = _get_taghandler(suffix);
  
  if (hdl && hdl->find_frame) {
    frame = hdl->find_frame(infile, SvPVX(path), offset);
  }
  
  return frame;
}

MODULE = Audio::Scan		PACKAGE = Audio::Scan

int
has_flac(void)
CODE:
{
  RETVAL = 1;
}
OUTPUT:
  RETVAL

HV *
scan (char * /*klass*/, SV *path, ...)
CODE:
{
  PerlIO *infile;
  int filter = FILTER_TYPE_INFO | FILTER_TYPE_TAGS;
  char *suffix = strrchr( SvPVX(path), '.' );

  // Check for filter to only run one of the scan types
  if ( items == 3 && SvOK(ST(2)) ) {
    filter = SvIV(ST(2));
  }

  if ( !suffix ) {
    XSRETURN_UNDEF;
  }

  suffix++;
  
  if ( !(infile = PerlIO_open(SvPVX(path), "rb")) ) {
    PerlIO_printf(PerlIO_stderr(), "Could not open %s for reading: %s\n", SvPVX(path), strerror(errno));
    XSRETURN_UNDEF;
  }
  
  RETVAL = _scan( suffix, infile, path, filter );
  
  PerlIO_close(infile);
}
OUTPUT:
  RETVAL

HV *
scan_fh(char *, SV *type, SV *sfh, ...)
CODE:
{
  uint8_t filter = FILTER_TYPE_INFO | FILTER_TYPE_TAGS;
  char *suffix = SvPVX(type);
  
  PerlIO *fh = IoIFP(sv_2io(sfh));
  
  // Check for filter to only run one of the scan types
  if ( items == 4 && SvOK(ST(3)) ) {
    filter = SvIV(ST(3));
  }

  RETVAL = _scan( suffix, fh, newSVpv("(filehandle)", 0), filter );
}
OUTPUT:
  RETVAL

int
find_frame(char *, SV *path, int offset)
CODE:
{
  PerlIO *infile;
  char *suffix = strrchr( SvPVX(path), '.' );
  
  if ( !suffix ) {
    RETVAL = -1;
    return;
  }
  
  suffix++;
  
  if ( !(infile = PerlIO_open(SvPVX(path), "rb")) ) {
    PerlIO_printf(PerlIO_stderr(), "Could not open %s for reading\n", SvPVX(path));
    RETVAL = -1;
    return;
  }
  
  RETVAL = _find_frame( suffix, infile, path, offset );
  
  PerlIO_close(infile);
}
OUTPUT:
  RETVAL

int
find_frame_fh(char *, SV *type, SV *sfh, int offset)
CODE:
{
  char *suffix = SvPVX(type);
  
  PerlIO *fh = IoIFP(sv_2io(sfh));
  
  RETVAL = _find_frame( suffix, fh, newSVpv("(filehandle)", 0), offset );
}
OUTPUT:
  RETVAL

int
is_supported(char *, SV *path)
CODE:
{
  char *suffix = strrchr( SvPVX(path), '.' );

  if (suffix != NULL && *suffix == '.' && _get_taghandler(suffix + 1)) {
    RETVAL = 1;
  }
  else {
    RETVAL = 0;
  }
}
OUTPUT:
  RETVAL

SV *
type_for(char *, SV *suffix)
CODE:
{
  taghandler *hdl = NULL;
  char *suff = SvPVX(suffix);

  if (suff == NULL || *suff == '\0') {
    RETVAL = newSV(0);
  }
  else {
    hdl = _get_taghandler(suff);
    if (hdl == NULL) {
      RETVAL = newSV(0);
    }
    else {
      RETVAL = newSVpv(hdl->type, 0);
    }
  }
}
OUTPUT:
  RETVAL

AV *
get_types(void)
CODE:
{
  int i;

  RETVAL = newAV();
  sv_2mortal((SV*)RETVAL);
  for (i = 0; audio_types[i].type; i++) {
    av_push(RETVAL, newSVpv(audio_types[i].type, 0));
  }
}
OUTPUT:
  RETVAL

AV *
extensions_for(char *, SV *type)
CODE:
{
  int i, j;
  char *t = SvPVX(type);

  RETVAL = newAV();
  sv_2mortal((SV*)RETVAL);
  for (i = 0; audio_types[i].type; i++) {
#ifdef _MSC_VER
    if (!stricmp(audio_types[i].type, t)) {
#else
    if (!strcasecmp(audio_types[i].type, t)) {
#endif

      for (j = 0; audio_types[i].suffix[j]; j++) {
        av_push(RETVAL, newSVpv(audio_types[i].suffix[j], 0));
      }
      break;

    }
  }
}
OUTPUT:
  RETVAL
