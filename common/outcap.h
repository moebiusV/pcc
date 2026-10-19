/*
 * Capture what the compiler prints on standard output into memory.
 *
 * The back ends keep their assembly in memory to look at it as a whole
 * (-xdmr).  They used to assign stdout = open_memstream(...), which is not
 * allowed where stdout is a constant (musl); this redirects file descriptor 1
 * to a temporary file instead and reads it back.
 */
#ifndef OUTCAP_H
#define OUTCAP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static FILE *outcap_tmp;
static int outcap_saved = -1;

/* start capturing; 1 if it is on */
static int
outcap_begin(void)
{
	fflush(stdout);
	if ((outcap_tmp = tmpfile()) == NULL)
		return 0;
	if ((outcap_saved = dup(1)) < 0) {
		fclose(outcap_tmp);
		return 0;
	}
	if (dup2(fileno(outcap_tmp), 1) < 0) {
		close(outcap_saved);
		fclose(outcap_tmp);
		return 0;
	}
	return 1;
}

/* stop; the text written meanwhile, a malloced string (its length in *len) */
static char *
outcap_end(size_t *len)
{
	char *buf;
	long n;

	fflush(stdout);
	dup2(outcap_saved, 1);
	close(outcap_saved);
	outcap_saved = -1;
	fseek(outcap_tmp, 0, SEEK_END);
	n = ftell(outcap_tmp);
	rewind(outcap_tmp);
	buf = malloc((size_t)n + 1);
	if (buf == NULL)
		return NULL;
	n = (long)fread(buf, 1, (size_t)n, outcap_tmp);
	buf[n] = '\0';
	fclose(outcap_tmp);
	outcap_tmp = NULL;
	if (len != NULL)
		*len = (size_t)n;
	return buf;
}

#endif
