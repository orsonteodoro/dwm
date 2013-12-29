/* See LICENSE file for copyright and license details. */
#if USE_WINAPI
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT			0x0500
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#if USE_WINAPI
#include <windows.h>
#include <winuser.h>
#endif

#include "util.h"

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
#if USE_WINAPI
	MessageBox(NULL, TEXT(errstr), TEXT("dwm"),MB_OK|MB_APPLMODAL);
#else
	vfprintf(stderr, errstr, ap);
#endif
	va_end(ap);
	exit(EXIT_FAILURE);
}

