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

#define	BUFFER_MAX_CHUNK	0x100000
#define	BUFFER_MAX_LEN		0xa00000
#define	BUFFER_ALLOCSZ		0x008000

/* Initializes the buffer structure. */

void
buffer_init(Buffer *buffer)
{
	const u_int len = 4096;

	buffer->alloc = 0;
  Newx(buffer->buf, len, u_char);
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
buffer_append(Buffer *buffer, const void *data, u_int len)
{
	void *p;
	p = buffer_append_space(buffer, len);
	Copy(data, p, len, u_char);
}

static int
buffer_compact(Buffer *buffer)
{
	/*
	 * If the buffer is quite empty, but all data is at the end, move the
	 * data to the beginning.
	 */
	if (buffer->offset > MIN(buffer->alloc, BUFFER_MAX_CHUNK)) {
		Move(buffer->buf + buffer->offset, buffer->buf,
			buffer->end - buffer->offset, u_char);
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
buffer_append_space(Buffer *buffer, u_int len)
{
	u_int newlen;
	void *p;

	if (len > BUFFER_MAX_CHUNK)
		croak("buffer_append_space: len %u not supported", len);

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
		croak("buffer_append_space: alloc %u not supported",
		    newlen);
	Renew(buffer->buf, newlen, u_char);
	buffer->alloc = newlen;
	goto restart;
	/* NOTREACHED */
}

/*
 * Check whether an allocation of 'len' will fit in the buffer
 * This must follow the same math as buffer_append_space
 */
int
buffer_check_alloc(Buffer *buffer, u_int len)
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

u_int
buffer_len(Buffer *buffer)
{
	return buffer->end - buffer->offset;
}

/* Gets data from the beginning of the buffer. */

int
buffer_get_ret(Buffer *buffer, void *buf, u_int len)
{
	if (len > buffer->end - buffer->offset) {
		warn("buffer_get_ret: trying to get more bytes %d than in buffer %d",
		    len, buffer->end - buffer->offset);
		return (-1);
	}
	Copy(buffer->buf + buffer->offset, buf, len, char);
	buffer->offset += len;
	return (0);
}

void
buffer_get(Buffer *buffer, void *buf, u_int len)
{
	if (buffer_get_ret(buffer, buf, len) == -1)
		croak("buffer_get: buffer error");
}

/* Consumes the given number of bytes from the beginning of the buffer. */

int
buffer_consume_ret(Buffer *buffer, u_int bytes)
{
	if (bytes > buffer->end - buffer->offset) {
		warn("buffer_consume_ret: trying to get more bytes than in buffer");
		return (-1);
	}
	buffer->offset += bytes;
	return (0);
}

void
buffer_consume(Buffer *buffer, u_int bytes)
{
	if (buffer_consume_ret(buffer, bytes) == -1)
		croak("buffer_consume: buffer error");
}

/* Consumes the given number of bytes from the end of the buffer. */

int
buffer_consume_end_ret(Buffer *buffer, u_int bytes)
{
	if (bytes > buffer->end - buffer->offset)
		return (-1);
	buffer->end -= bytes;
	return (0);
}

void
buffer_consume_end(Buffer *buffer, u_int bytes)
{
	if (buffer_consume_end_ret(buffer, bytes) == -1)
		croak("buffer_consume_end: trying to get more bytes than in buffer");
}

/* Returns a pointer to the first used byte in the buffer. */

void *
buffer_ptr(Buffer *buffer)
{
	return buffer->buf + buffer->offset;
}

/* Dumps the contents of the buffer to stderr. */

void
buffer_dump(Buffer *buffer)
{
	u_int i;
	u_char *ucp = buffer->buf;

	for (i = buffer->offset; i < buffer->end; i++) {
		fprintf(stderr, "%02x ", ucp[i]);
		if ((i-buffer->offset)%16==15)
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

u_int32_t
get_u32le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  u_int32_t v;

  v  = (u_int32_t)p[3] << 24;
  v |= (u_int32_t)p[2] << 16;
  v |= (u_int32_t)p[1] << 8;
  v |= (u_int32_t)p[0];

  return (v);
}

int
buffer_get_int_le_ret(u_int *ret, Buffer *buffer)
{
	u_char buf[4];

	if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
		return (-1);
	*ret = get_u32le(buf);
	return (0);
}

u_int
buffer_get_int_le(Buffer *buffer)
{
	u_int ret;

	if (buffer_get_int_le_ret(&ret, buffer) == -1)
		croak("buffer_get_int: buffer error");

	return (ret);
}