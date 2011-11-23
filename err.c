/* err.c: Error handling and reporting.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<stdio.h>
#include	<stdarg.h>
#include	"err.h"

enum {
    NOTIFY_LOG,
    NOTIFY_ERR,
    NOTIFY_DIE,
};

/* "Hidden" arguments to _warn, _errmsg, and _die.
 */
char const      *_err_cfile = NULL;
unsigned long	_err_lineno = 0;

static void usermessage(int action, char const *prefix,
			char const *cfile, unsigned long lineno,
			char const *fmt, va_list args)
{
    if (prefix) {
	fprintf(stderr, "%s: ", prefix);
    }
    if (fmt) {
	vfprintf(stderr, fmt, args);
    }
    if (cfile) {
	fprintf(stderr, " [%s:%lu]", cfile, lineno);
    }
    fputc('\n', stderr);
}

/* Log a warning message.
 */
void _warn(char const *fmt, ...)
{
    va_list	args;

    va_start(args, fmt);
    usermessage(NOTIFY_LOG, NULL, _err_cfile, _err_lineno, fmt, args);
    va_end(args);
    _err_cfile = NULL;
    _err_lineno = 0;
}

/* Display an error message to the user.
 */
void _errmsg(char const *prefix, char const *fmt, ...)
{
    va_list	args;

    va_start(args, fmt);
    usermessage(NOTIFY_ERR, prefix, _err_cfile, _err_lineno, fmt, args);
    va_end(args);
    _err_cfile = NULL;
    _err_lineno = 0;
}

/* Display an error message to the user and exit.
 */
void _die(char const *fmt, ...)
{
    va_list	args;

    va_start(args, fmt);
    usermessage(NOTIFY_DIE, NULL, _err_cfile, _err_lineno, fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}
