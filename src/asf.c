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

#include "asf.h"

static void
print_guid(GUID guid)
{
  PerlIO_printf(
    PerlIO_stderr(), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
    guid.l, guid.w[0], guid.w[1],
    guid.b[0], guid.b[1], guid.b[2], guid.b[3],
    guid.b[4], guid.b[5], guid.b[6], guid.b[7]
  );
}

static int
get_asf_metadata(char *file, HV *info, HV *tags)
{
  PerlIO *infile;
  
  Buffer asf_buf;
  ASF_Object hdr;
  ASF_Object tmp;
  
  off_t file_size;           // total file size
  off_t audio_offset = 0;    // offset to audio
  
  int err = 0;
  
  if (!(infile = PerlIO_open(file, "rb"))) {
    PerlIO_printf(PerlIO_stderr(), "Could not open %s for reading\n", file);
    err = -1;
    goto out;
  }
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  buffer_init(&asf_buf);
  
  if ( !_check_buf(infile, &asf_buf, ASF_BLOCK_SIZE, ASF_BLOCK_SIZE) ) {
    err = -1;
    goto out;
  }
  
  buffer_get(&asf_buf, &hdr.ID, 16);
  
  if ( !IsEqualGUID(&hdr.ID, &ASF_HeaderObject) ) {
    PerlIO_printf(PerlIO_stderr(), "Invalid ASF header: %s\n", file);
    err = -1;
    goto out;
  }
  
  hdr.size        = buffer_get_int64_le(&asf_buf);
  hdr.num_objects = buffer_get_int_le(&asf_buf);
  hdr.reserved1   = buffer_get_char(&asf_buf);
  hdr.reserved2   = buffer_get_char(&asf_buf);
  
  PerlIO_printf(PerlIO_stderr(), "header size: %d\n", hdr.size);
  
  if ( hdr.reserved2 != 0x02 ) {
    PerlIO_printf(PerlIO_stderr(), "Invalid ASF header: %s\n", file);
    err = -1;
    goto out;
  }
  
  while (hdr.num_objects) {
    if ( !_check_buf(infile, &asf_buf, 24, ASF_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    buffer_get(&asf_buf, &tmp.ID, 16);
    
    tmp.size = buffer_get_int64_le(&asf_buf);
    
    print_guid(tmp.ID);
    PerlIO_printf(PerlIO_stderr(), "size: %lu\n", tmp.size);
    
    if ( !_check_buf(infile, &asf_buf, tmp.size - 24, ASF_BLOCK_SIZE) ) {
      err = -1;
      goto out;
    }
    
    // XXX
    buffer_consume(&asf_buf, tmp.size - 24);
    
    hdr.num_objects--;
  }
  
out:
  if (infile) PerlIO_close(infile);

  buffer_free(&asf_buf);

  if (err) return err;

  return 0;
}