#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "tagutils-misc.c"
#include "tagutils-mp3.c"

struct _types {
  char *type;
  char *suffix[7];
};

typedef struct {
  char*	type;
  int (*get_tags)(char* file, HV *tags);
  int (*get_fileinfo)(char* file, HV *tags);
} taghandler;

struct _types audio_types[] = {
  {"aac", {"mp4", "mp4", "m4a", "m4p", 0}},
  {"mp3", {"mp3", "mp2", 0}},
  {"ogg", {"ogg", "oga", 0}},
  {"flc", {"flc", "flac", "fla", 0}},
  {"asf", {"wma", 0}},
  {0, {0, 0}}
};

static taghandler taghandlers[] = {
  { "aac", 0, 0 },
  { "mp3", _get_mp3tags, _get_mp3fileinfo },
  { "ogg", 0, 0 },
  { "flc", 0, 0 },
  { "asf", 0, 0 },
  { NULL, 0, 0 }
};

MODULE = Audio::Scan		PACKAGE = Audio::Scan		

HV *
_scan (char *klass, SV *path)
CODE:
{
  RETVAL = newHV();  
  int typeindex = -1;
  int i, j;
  char *suffix = strrchr( SvPVX(path), '.' );
  
  // don't leak
  sv_2mortal( (SV*)RETVAL );
  
  if ( !suffix ) {
    XSRETURN_UNDEF;
  }
  
  suffix++;
  for (i=0; typeindex==-1 && audio_types[i].type; i++) {
    for (j=0; typeindex==-1 && audio_types[i].suffix[j]; j++) {
      if (!strcasecmp(audio_types[i].suffix[j], suffix)) {
        typeindex = i;
        break;
      }
    }
  }
  
  if (typeindex > 0) {
    taghandler *hdl;

    // dispatch to appropriate tag handler
    for (hdl = taghandlers; hdl->type; ++hdl)
      if (!strcmp(hdl->type, audio_types[typeindex].type))
        break;

    if (hdl->get_fileinfo) {
      HV *info = newHV();
      hdl->get_fileinfo(SvPVX(path), info);
      hv_store( RETVAL, "info", 4, newRV_noinc( (SV *)info ), 0 );
    }
    
    if (hdl->get_tags) {
      HV *tags = newHV();
      hdl->get_tags(SvPVX(path), tags);
      hv_store( RETVAL, "tags", 4, newRV_noinc( (SV *)tags ), 0 );
    }
  }
}
OUTPUT:
  RETVAL