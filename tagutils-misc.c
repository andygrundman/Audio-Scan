//=========================================================================
// FILENAME	: tagutils-misc.c
// DESCRIPTION	: Misc routines for supporting tagutils
//=========================================================================
// Copyright (c) 2008- NETGEAR, Inc. All Rights Reserved.
//=========================================================================

/* This program is free software; you can redistribute it and/or modify
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

/**************************************************************************
 * Language
 **************************************************************************/

#include <iconv.h>

#define MAX_ICONV_BUF 1024

typedef enum {
  ICONV_OK,
  ICONV_TRYNEXT,
  ICONV_FATAL
} iconv_result;

static iconv_result
do_iconv(const char* to_ces, const char* from_ces,
	 char *inbuf,  size_t inbytesleft,
	 char *outbuf_orig, size_t outbytesleft_orig) {
  size_t rc;
  iconv_result ret = ICONV_OK;

  size_t outbytesleft = outbytesleft_orig - 1;
  char* outbuf = outbuf_orig;

  iconv_t cd  = iconv_open(to_ces, from_ces);
  if (cd == (iconv_t)-1) {
    return ICONV_FATAL;
  }
  rc = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  if (rc == (size_t)-1) {
    if (errno == E2BIG) {
      ret = ICONV_FATAL;
    }
    else {
      ret = ICONV_TRYNEXT;
      memset(outbuf_orig, '\0', outbytesleft_orig);
    }
  }
  iconv_close(cd);

  return ret;
}

#define N_LANG_ALT 8
struct {
  char *lang;
  char *cpnames[N_LANG_ALT];
} iconv_map[] = {
  {"JA", {"ISO-8859-1", "CP932", "ISO8859-1", "CP950", "CP936", 0}},
  {"ZH_CN", {"ISO-8859-1", "CP936", "CP950", "CP932", 0}},
  {"ZH_TW", {"ISO-8859-1", "CP950", "CP936", "CP932", 0}},
  {0, {0}}
};
static int lang_index = -1;

static int
_lang2cp(char *lang) {
  int cp;
  if (!lang || lang[0]=='\0')
    return -1;
  for (cp=0; iconv_map[cp].lang; cp++) {
    if (!strcasecmp(iconv_map[cp].lang, lang))
      return cp;
  }
  return -1;
}
