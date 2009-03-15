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
  {0, 0, {0}}
};

static taghandler taghandlers[] = {
  { "aac", 0, 0 },
  { "mp3", _get_mp3tags, _get_mp3fileinfo },
  { "ogg", 0, 0 },
  { "flc", 0, 0 },
  { "asf", 0, 0 },
  { NULL, 0 }
};

MODULE = Audio::Scan		PACKAGE = Audio::Scan		

HV *
_scan (char *klass, SV *path)
CODE:
{
  HV *data = newHV();
  HV *info = newHV();
  HV *tags = newHV();
  
  int typeindex = -1;
  int i, j;
  char *suffix = strrchr( SvPVX(path), '.' );
  
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

    if (hdl->get_fileinfo)
      hdl->get_fileinfo(SvPVX(path), info);
    
    if (hdl->get_tags)
      hdl->get_tags(SvPVX(path), tags);
  }
  
  // XXX
  //hv_store( data, "info", 4, (SV *)info, 0 );
  //hv_store( data, "tags", 4, (SV *)tags, 0 );
  
  RETVAL = tags;
}
OUTPUT:
  RETVAL