/*
 * Copyright (C) 2009 Red Hat Inc, Steven Rostedt <srostedt@redhat.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License (not later!)
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "event-parse.h"
#include "event-utils.h"

/*
 * The TRACE_SEQ_POISON is to catch the use of using
 * a trace_seq structure after it was destroyed.
 */
#define TRACE_SEQ_POISON	((void *)0xdeadbeef)
#define TRACE_SEQ_CHECK(s)						\
do {									\
	if ((s)->buffer == TRACE_SEQ_POISON)			\
		die("Usage of trace_seq after it was destroyed");	\
} while (0)

/**
 * trace_seq_init - initialize the trace_seq structure
 * @s: a pointer to the trace_seq structure to initialize
 */
void trace_seq_init(struct trace_seq *s)
{
	s->len = 0;
	s->readpos = 0;
	s->buffer_size = TRACE_SEQ_BUF_SIZE;
	s->buffer = malloc_or_die(s->buffer_size);
}

/**
 * trace_seq_destroy - free up memory of a trace_seq
 * @s: a pointer to the trace_seq to free the buffer
 *
 * Only frees the buffer, not the trace_seq struct itself.
 */
void trace_seq_destroy(struct trace_seq *s)
{
	if (!s)
		return;
	TRACE_SEQ_CHECK(s);
	free(s->buffer);
	s->buffer = TRACE_SEQ_POISON;
}

static void expand_buffer(struct trace_seq *s)
{
	s->buffer_size += TRACE_SEQ_BUF_SIZE;
	s->buffer = realloc(s->buffer, s->buffer_size);
	if (!s->buffer)
		die("Can't allocate trace_seq buffer memory");
}

/**
 * trace_seq_printf - sequence printing of trace information
 * @s: trace sequence descriptor
 * @fmt: printf format string
 *
 * It returns 0 if the trace oversizes the buffer's free
 * space, 1 otherwise.
 *
 * The tracer may use either sequence operations or its own
 * copy to user routines. To simplify formating of a trace
 * trace_seq_printf is used to store strings into a special
 * buffer (@s). Then the output may be either used by
 * the sequencer or pulled into another buffer.
 */
int
trace_seq_printf(struct trace_seq *s, const char *fmt, ...)
{
	va_list ap;
	int len;
	int ret;

	TRACE_SEQ_CHECK(s);

 try_again:
	len = (s->buffer_size - 1) - s->len;

	va_start(ap, fmt);
	ret = vsnprintf(s->buffer + s->len, len, fmt, ap);
	va_end(ap);

	if (ret >= len) {
		expand_buffer(s);
		goto try_again;
	}

	s->len += ret;

	return 1;
}

/**
 * trace_seq_vprintf - sequence printing of trace information
 * @s: trace sequence descriptor
 * @fmt: printf format string
 *
 * The tracer may use either sequence operations or its own
 * copy to user routines. To simplify formating of a trace
 * trace_seq_printf is used to store strings into a special
 * buffer (@s). Then the output may be either used by
 * the sequencer or pulled into another buffer.
 */
int
trace_seq_vprintf(struct trace_seq *s, const char *fmt, va_list args)
{
	int len;
	int ret;

	TRACE_SEQ_CHECK(s);

 try_again:
	len = (s->buffer_size - 1) - s->len;

	ret = vsnprintf(s->buffer + s->len, len, fmt, args);

	if (ret >= len) {
		expand_buffer(s);
		goto try_again;
	}

	s->len += ret;

	return len;
}

/**
 * trace_seq_puts - trace sequence printing of simple string
 * @s: trace sequence descriptor
 * @str: simple string to record
 *
 * The tracer may use either the sequence operations or its own
 * copy to user routines. This function records a simple string
 * into a special buffer (@s) for later retrieval by a sequencer
 * or other mechanism.
 */
int trace_seq_puts(struct trace_seq *s, const char *str)
{
	int len;

	TRACE_SEQ_CHECK(s);

	len = strlen(str);

	while (len > ((s->buffer_size - 1) - s->len))
		expand_buffer(s);

	memcpy(s->buffer + s->len, str, len);
	s->len += len;

	return len;
}

int trace_seq_putc(struct trace_seq *s, unsigned char c)
{
	TRACE_SEQ_CHECK(s);

	while (s->len >= (s->buffer_size - 1))
		expand_buffer(s);

	s->buffer[s->len++] = c;

	return 1;
}

void trace_seq_terminate(struct trace_seq *s)
{
	TRACE_SEQ_CHECK(s);

	/* There's always one character left on the buffer */
	s->buffer[s->len] = 0;
}

int trace_seq_do_printf(struct trace_seq *s)
{
	TRACE_SEQ_CHECK(s);
	return printf("%.*s", s->len, s->buffer);
}
