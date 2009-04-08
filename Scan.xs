#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "common.c"
#include "mp3.c"
#include "ogg.c"
#include "asf.c"

#ifdef HAVE_FLAC
#include "flac.c"
#endif

#define FILTER_TYPE_INFO 0x01
#define FILTER_TYPE_TAGS 0x02

struct _types {
  char *type;
  char *suffix[7];
};

typedef struct {
  char*	type;
  int (*get_tags)(char* file, HV *info, HV *tags);
  int (*get_fileinfo)(char* file, HV *tags);
} taghandler;

struct _types audio_types[] = {
  {"aac", {"mp4", "mp4", "m4a", "m4p", 0}},
  {"mp3", {"mp3", "mp2", 0}},
  {"ogg", {"ogg", "oga", 0}},
#ifdef HAVE_FLAC
  {"flc", {"flc", "flac", "fla", 0}},
#endif
  {"asf", {"wma", "asf", "wmv", 0}},
  {0, {0, 0}}
};

static taghandler taghandlers[] = {
  { "aac", 0, 0 },
  { "mp3", get_mp3tags, get_mp3fileinfo },
  { "ogg", get_ogg_metadata, 0 },
#ifdef HAVE_FLAC
  { "flc", get_flac_metadata, 0 },
#endif
  { "asf", get_asf_metadata, 0 },
  { NULL, 0, 0 }
};

MODULE = Audio::Scan		PACKAGE = Audio::Scan

int
has_flac(void)
CODE:
{
#ifdef HAVE_FLAC
  RETVAL = 1;
#else
  RETVAL = 0;
#endif
}
OUTPUT:
  RETVAL

HV *
scan (char * /*klass*/, SV *path, ...)
CODE:
{
  int typeindex = -1;
  int i, j;
  int filter = FILTER_TYPE_INFO | FILTER_TYPE_TAGS;
  char *suffix = strrchr( SvPVX(path), '.' );

  // Check for filter to only run one of the scan types
  if ( items == 3 && SvOK(ST(2)) ) {
    filter = SvIV(ST(2));
  }

  RETVAL = newHV();

  // don't leak
  sv_2mortal( (SV*)RETVAL );

  if ( !suffix ) {
    XSRETURN_UNDEF;
  }

  suffix++;
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

  if (typeindex > 0) {
    taghandler *hdl;
    HV *info = newHV();

    // dispatch to appropriate tag handler
    for (hdl = taghandlers; hdl->type; ++hdl)
      if (!strcmp(hdl->type, audio_types[typeindex].type))
        break;
      
    // Ignore filter if a file type has only one function (FLAC/Ogg)
    if ( !hdl->get_fileinfo ) {
      filter = FILTER_TYPE_INFO | FILTER_TYPE_TAGS;
    }

    if ( hdl->get_fileinfo && (filter & FILTER_TYPE_INFO) ) {
      hdl->get_fileinfo(SvPVX(path), info);
    }

    if ( hdl->get_tags && (filter & FILTER_TYPE_TAGS) ) {
      HV *tags = newHV();
      hdl->get_tags(SvPVX(path), info, tags);
      hv_store( RETVAL, "tags", 4, newRV_noinc( (SV *)tags ), 0 );
    }

    // Info may be used in tag function, i.e. to find tag version
    hv_store( RETVAL, "info", 4, newRV_noinc( (SV *)info ), 0 );
  }
  else {
    croak("Audio::Scan unsupported file type: %s", SvPVX(path));
  }
}
OUTPUT:
  RETVAL
