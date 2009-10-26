// Derived from:

/* $OpenBSD: buffer.c,v 1.31 2006/08/03 03:34:41 deraadt Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for manipulating fifo buffers (that can grow if needed).
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "buffer.h"

#define  BUFFER_MAX_CHUNK  0x1400000
#define  BUFFER_MAX_LEN    0x1400000
#define  BUFFER_ALLOCSZ    0x008000

#define UnsignedToFloat(u) (((double)((long)(u - 2147483647L - 1))) + 2147483648.0)

/* Initializes the buffer structure. */

void
buffer_init(Buffer *buffer, uint32_t len)
{
  if (!len)
    len = 4096;

  buffer->alloc = 0;
  New(0, buffer->buf, (int)len, u_char);
  buffer->alloc = len;
  buffer->offset = 0;
  buffer->end = 0;
}

/* Frees any memory used for the buffer. */

void
buffer_free(Buffer *buffer)
{
  if (buffer->alloc > 0) {
    memset(buffer->buf, 0, buffer->alloc);
    buffer->alloc = 0;
    Safefree(buffer->buf);
  }
}

/*
 * Clears any data from the buffer, making it empty.  This does not actually
 * zero the memory.
 */

void
buffer_clear(Buffer *buffer)
{
  buffer->offset = 0;
  buffer->end = 0;
}

/* Appends data to the buffer, expanding it if necessary. */

void
buffer_append(Buffer *buffer, const void *data, uint32_t len)
{
  void *p;
  p = buffer_append_space(buffer, len);
  Copy(data, p, (int)len, u_char);
}

static int
buffer_compact(Buffer *buffer)
{
  /*
   * If the buffer is quite empty, but all data is at the end, move the
   * data to the beginning.
   */
  if (buffer->offset > MIN(buffer->alloc, BUFFER_MAX_CHUNK)) {
    Move(buffer->buf + buffer->offset, buffer->buf, (int)(buffer->end - buffer->offset), u_char);
    buffer->end -= buffer->offset;
    buffer->offset = 0;
    return (1);
  }

  return (0);
}

/*
 * Appends space to the buffer, expanding the buffer if necessary. This does
 * not actually copy the data into the buffer, but instead returns a pointer
 * to the allocated region.
 */

void *
buffer_append_space(Buffer *buffer, uint32_t len)
{
  uint32_t newlen;
  void *p;

  if (len > BUFFER_MAX_CHUNK)
    croak("buffer_append_space: len %u too large (max %u)", len, BUFFER_MAX_CHUNK);

  /* If the buffer is empty, start using it from the beginning. */
  if (buffer->offset == buffer->end) {
    buffer->offset = 0;
    buffer->end = 0;
  }

restart:
  /* If there is enough space to store all data, store it now. */
  if (buffer->end + len < buffer->alloc) {
    p = buffer->buf + buffer->end;
    buffer->end += len;
    return p;
  }

  /* Compact data back to the start of the buffer if necessary */
  if (buffer_compact(buffer))
    goto restart;

  /* Increase the size of the buffer and retry. */
  newlen = roundup(buffer->alloc + len, BUFFER_ALLOCSZ);
  if (newlen > BUFFER_MAX_LEN)
    croak("buffer_append_space: alloc %u too large (max %u)",
        newlen, BUFFER_MAX_LEN);
  Renew(buffer->buf, (int)newlen, u_char);
  buffer->alloc = newlen;
  goto restart;
  /* NOTREACHED */
}

/*
 * Check whether an allocation of 'len' will fit in the buffer
 * This must follow the same math as buffer_append_space
 */
int
buffer_check_alloc(Buffer *buffer, uint32_t len)
{
  if (buffer->offset == buffer->end) {
    buffer->offset = 0;
    buffer->end = 0;
  }
 restart:
  if (buffer->end + len < buffer->alloc)
    return (1);
  if (buffer_compact(buffer))
    goto restart;
  if (roundup(buffer->alloc + len, BUFFER_ALLOCSZ) <= BUFFER_MAX_LEN)
    return (1);
  return (0);
}

/* Returns the number of bytes of data in the buffer. */

uint32_t
buffer_len(Buffer *buffer)
{
  return buffer->end - buffer->offset;
}

/* Gets data from the beginning of the buffer. */

int
buffer_get_ret(Buffer *buffer, void *buf, uint32_t len)
{
  if (len > buffer->end - buffer->offset) {
    warn("buffer_get_ret: trying to get more bytes %d than in buffer %d", len, buffer->end - buffer->offset);
    return (-1);
  }

  Copy(buffer->buf + buffer->offset, buf, (int)len, char);
  buffer->offset += len;
  return (0);
}

void
buffer_get(Buffer *buffer, void *buf, uint32_t len)
{
  if (buffer_get_ret(buffer, buf, len) == -1)
    croak("buffer_get: buffer error");
}

/* Consumes the given number of bytes from the beginning of the buffer. */

int
buffer_consume_ret(Buffer *buffer, uint32_t bytes)
{
  if (bytes > buffer->end - buffer->offset) {
    warn("buffer_consume_ret: trying to get more bytes %d than in buffer %d", bytes, buffer->end - buffer->offset);
    return (-1);
  }

  buffer->offset += bytes;
  return (0);
}

void
buffer_consume(Buffer *buffer, uint32_t bytes)
{
  if (buffer_consume_ret(buffer, bytes) == -1)
    croak("buffer_consume: buffer error");
}

/* Consumes the given number of bytes from the end of the buffer. */

int
buffer_consume_end_ret(Buffer *buffer, uint32_t bytes)
{
  if (bytes > buffer->end - buffer->offset)
    return (-1);

  buffer->end -= bytes;
  return (0);
}

void
buffer_consume_end(Buffer *buffer, uint32_t bytes)
{
  if (buffer_consume_end_ret(buffer, bytes) == -1)
    croak("buffer_consume_end: trying to get more bytes %d than in buffer %d", bytes, buffer->end - buffer->offset);
}

/* Returns a pointer to the first used byte in the buffer. */

void *
buffer_ptr(Buffer *buffer)
{
  return buffer->buf + buffer->offset;
}

/* Dumps the contents of the buffer to stderr. */

void
buffer_dump(Buffer *buffer, uint32_t len)
{
  uint32_t i;
  u_char *ucp = buffer->buf;
  
  if (!len) {
    len = buffer->end - buffer->offset;
  }

  for (i = buffer->offset; i < buffer->offset + len; i++) {
    fprintf(stderr, "%02x ", ucp[i]);

    if ((i-buffer->offset) % 16 == 15)
      fprintf(stderr, "\r\n");
  }

  fprintf(stderr, "\r\n");
}

// Useful functions from bufaux.c

/*
 * Returns a character from the buffer (0 - 255).
 */
int
buffer_get_char_ret(char *ret, Buffer *buffer)
{
  if (buffer_get_ret(buffer, ret, 1) == -1) {
    warn("buffer_get_char_ret: buffer_get_ret failed");
    return (-1);
  }

  return (0);
}

int
buffer_get_char(Buffer *buffer)
{
  char ch;

  if (buffer_get_char_ret(&ch, buffer) == -1)
    croak("buffer_get_char: buffer error");
  return (u_char) ch;
}

uint32_t
get_u32le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint32_t v;

  v  = (uint32_t)p[3] << 24;
  v |= (uint32_t)p[2] << 16;
  v |= (uint32_t)p[1] << 8;
  v |= (uint32_t)p[0];

  return (v);
}

int
buffer_get_int_le_ret(uint32_t *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_u32le(buf);
  return (0);
}

uint32_t
buffer_get_int_le(Buffer *buffer)
{
  uint32_t ret;

  if (buffer_get_int_le_ret(&ret, buffer) == -1)
    croak("buffer_get_int_le: buffer error");

  return (ret);
}

uint32_t
get_u32(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint32_t v;

  v  = (uint32_t)p[0] << 24;
  v |= (uint32_t)p[1] << 16;
  v |= (uint32_t)p[2] << 8;
  v |= (uint32_t)p[3];

  return (v);
}

int
buffer_get_int_ret(uint32_t *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_u32(buf);
  return (0);
}

uint32_t
buffer_get_int(Buffer *buffer)
{
  uint32_t ret;

  if (buffer_get_int_ret(&ret, buffer) == -1)
    croak("buffer_get_int: buffer error");

  return (ret);
}

uint32_t
get_u24(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint32_t v;

  v  = (uint32_t)p[0] << 16;
  v |= (uint32_t)p[1] << 8;
  v |= (uint32_t)p[2];

  return (v);
}

int
buffer_get_int24_ret(uint32_t *ret, Buffer *buffer)
{
  u_char buf[3];

  if (buffer_get_ret(buffer, (char *) buf, 3) == -1)
    return (-1);
  *ret = get_u24(buf);
  return (0);
}

uint32_t
buffer_get_int24(Buffer *buffer)
{
  uint32_t ret;

  if (buffer_get_int24_ret(&ret, buffer) == -1)
    croak("buffer_get_int24: buffer error");

  return (ret);
}

uint64_t
get_u64le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint64_t v;

  v  = (uint64_t)p[7] << 56;
  v |= (uint64_t)p[6] << 48;
  v |= (uint64_t)p[5] << 40;
  v |= (uint64_t)p[4] << 32;
  v |= (uint64_t)p[3] << 24;
  v |= (uint64_t)p[2] << 16;
  v |= (uint64_t)p[1] << 8;
  v |= (uint64_t)p[0];

  return (v);
}

int
buffer_get_int64_le_ret(uint64_t *ret, Buffer *buffer)
{
  u_char buf[8];

  if (buffer_get_ret(buffer, (char *) buf, 8) == -1)
    return (-1);
  *ret = get_u64le(buf);
  return (0);
}

uint64_t
buffer_get_int64_le(Buffer *buffer)
{
  uint64_t ret;

  if (buffer_get_int64_le_ret(&ret, buffer) == -1)
    croak("buffer_get_int64_le: buffer error");

  return (ret);
}

uint64_t
get_u64(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint64_t v;

  v  = (uint64_t)p[0] << 56;
  v |= (uint64_t)p[1] << 48;
  v |= (uint64_t)p[2] << 40;
  v |= (uint64_t)p[3] << 32;
  v |= (uint64_t)p[4] << 24;
  v |= (uint64_t)p[5] << 16;
  v |= (uint64_t)p[6] << 8;
  v |= (uint64_t)p[7];

  return (v);
}

int
buffer_get_int64_ret(uint64_t *ret, Buffer *buffer)
{
  u_char buf[8];

  if (buffer_get_ret(buffer, (char *) buf, 8) == -1)
    return (-1);
  *ret = get_u64(buf);
  return (0);
}

uint64_t
buffer_get_int64(Buffer *buffer)
{
  uint64_t ret;

  if (buffer_get_int64_ret(&ret, buffer) == -1)
    croak("buffer_get_int64_le: buffer error");

  return (ret);
}

uint16_t
get_u16le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint16_t v;

  v  = (uint16_t)p[1] << 8;
  v |= (uint16_t)p[0];

  return (v);
}

int
buffer_get_short_le_ret(uint16_t *ret, Buffer *buffer)
{
  u_char buf[2];

  if (buffer_get_ret(buffer, (char *) buf, 2) == -1)
    return (-1);
  *ret = get_u16le(buf);
  return (0);
}

uint16_t
buffer_get_short_le(Buffer *buffer)
{
  uint16_t ret;

  if (buffer_get_short_le_ret(&ret, buffer) == -1)
    croak("buffer_get_short_le: buffer error");

  return (ret);
}

uint16_t
get_u16(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint16_t v;

  v  = (uint16_t)p[0] << 8;
  v |= (uint16_t)p[1];

  return (v);
}

int
buffer_get_short_ret(uint16_t *ret, Buffer *buffer)
{
  u_char buf[2];

  if (buffer_get_ret(buffer, (char *) buf, 2) == -1)
    return (-1);
  *ret = get_u16(buf);
  return (0);
}

uint16_t
buffer_get_short(Buffer *buffer)
{
  uint16_t ret;

  if (buffer_get_short_ret(&ret, buffer) == -1)
    croak("buffer_get_short: buffer error");

  return (ret);
}

/*
 * Stores a character in the buffer.
 */
void
buffer_put_char(Buffer *buffer, int value)
{
  char ch = value;

  buffer_append(buffer, &ch, 1);
}

// XXX supports U+0000 ~ U+FFFF only.
void
buffer_get_utf16le_as_utf8(Buffer *buffer, Buffer *utf8, uint32_t len)
{
  int i = 0;
  
  // Sanity check length
  if ( len % 2 ) {
    croak("buffer_get_utf16le_as_utf8: bad length %d", len);
  }
  
  buffer_init(utf8, len);
  
  for (i = 0; i < len; i += 2) {
    uint16_t wc = buffer_get_short_le(buffer);

    if (wc < 0x80) {
      buffer_put_char(utf8, wc & 0xff);      
    }
    else if (wc < 0x800) {
      buffer_put_char(utf8, 0xc0 | (wc>>6));
      buffer_put_char(utf8, 0x80 | (wc & 0x3f));
    }
    else {
      buffer_put_char(utf8, 0xe0 | (wc>>12));
      buffer_put_char(utf8, 0x80 | ((wc>>6) & 0x3f));
      buffer_put_char(utf8, 0x80 | (wc & 0x3f));
    }
  }
  
  // Add null if one wasn't provided
  if ( (utf8->buf + utf8->end - 1)[0] != 0 ) {
    buffer_put_char(utf8, 0);
  }
}
      
void
buffer_get_guid(Buffer *buffer, GUID *g)
{
  g->Data1 = buffer_get_int_le(buffer);
  g->Data2 = buffer_get_short_le(buffer);
  g->Data3 = buffer_get_short_le(buffer);
  
  buffer_get(buffer, g->Data4, 8);
}

int
buffer_get_float32_le_ret(float *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_f32le(buf);
  return (0);
}

float
buffer_get_float32_le(Buffer *buffer)
{
  float ret;

  if (buffer_get_float32_le_ret(&ret, buffer) == -1)
    croak("buffer_get_float32_le_ret: buffer error");

  return (ret);
}

// From libsndfile
float
get_f32le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  float v;
  int exponent, mantissa, negative;
  
  negative = p[3] & 0x80;
  exponent = ((p[3] & 0x7F) << 1) | ((p[2] & 0x80) ? 1 : 0);
  mantissa = ((p[2] & 0x7F) << 16) | (p[1] << 8) | (p[0]);
  
  if ( !(exponent || mantissa) ) {
    return 0.0;
  }
  
  mantissa |= 0x800000;
  exponent = exponent ? exponent - 127 : 0;
  
  v = mantissa ? ((float)mantissa) / ((float)0x800000) : 0.0;
  
  if (negative) {
    v *= -1;
  }
  
  if (exponent > 0) {
    v *= pow(2.0, exponent);
  }
  else if (exponent < 0) {
    v /= pow(2.0, abs(exponent));
  }

  return (v);
}

int
buffer_get_float32_ret(float *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_f32(buf);
  return (0);
}

float
buffer_get_float32(Buffer *buffer)
{
  float ret;

  if (buffer_get_float32_ret(&ret, buffer) == -1)
    croak("buffer_get_float32_ret: buffer error");

  return (ret);
}

// From libsndfile
float
get_f32(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  float v;
  int exponent, mantissa, negative;
  
  negative = p[0] & 0x80;
  exponent = ((p[0] & 0x7F) << 1) | ((p[1] & 0x80) ? 1 : 0);
  mantissa = ((p[1] & 0x7F) << 16) | (p[2] << 8) | (p[3]);
  
  if ( !(exponent || mantissa) ) {
    return 0.0;
  }
  
  mantissa |= 0x800000;
  exponent = exponent ? exponent - 127 : 0;
  
  v = mantissa ? ((float)mantissa) / ((float)0x800000) : 0.0;
  
  if (negative) {
    v *= -1;
  }
  
  if (exponent > 0) {
    v *= pow(2.0, exponent);
  }
  else if (exponent < 0) {
    v /= pow(2.0, abs(exponent));
  }

  return (v);
}

// http://www.onicos.com/staff/iz/formats/aiff.html
// http://www.onicos.com/staff/iz/formats/ieee.c
double
buffer_get_ieee_float(Buffer *buffer)
{
  double f;
  int expon;
  unsigned long hiMant, loMant;
  
  unsigned char *bptr = buffer_ptr(buffer);
  
  expon  = ((bptr[0] & 0x7F) << 8) | (bptr[1] & 0xFF);
  hiMant = ((unsigned long)(bptr[2] & 0xFF) << 24)
      |    ((unsigned long)(bptr[3] & 0xFF) << 16)
      |    ((unsigned long)(bptr[4] & 0xFF) << 8)
      |    ((unsigned long)(bptr[5] & 0xFF));
  loMant = ((unsigned long)(bptr[6] & 0xFF) << 24)
      |    ((unsigned long)(bptr[7] & 0xFF) << 16)
      |    ((unsigned long)(bptr[8] & 0xFF) << 8)
      |    ((unsigned long)(bptr[9] & 0xFF));

  if (expon == 0 && hiMant == 0 && loMant == 0) {
    f = 0;
  }
  else {
    if (expon == 0x7FFF) {    /* Infinity or NaN */
      f = HUGE_VAL;
    }
    else {
      expon -= 16383;
      f  = ldexp(UnsignedToFloat(hiMant), expon-=31);
      f += ldexp(UnsignedToFloat(loMant), expon-=32);
    }
  }
  
  buffer_consume(buffer, 10);

  if (bptr[0] & 0x80)
    return -f;
  else
    return f;
}
