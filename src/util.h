// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> // FILE


// get byte-size of file
// returns -1 on error/if file does not exist
int64_t fsu_fsize(char const *path);

// fopen() - wrapper that does 2 things:
// 1) convert path (utf8) to wchar_t for windows to be happy.
// 2) create missing directories (if any) of path, if mode contains 'w'
FILE *fsu_fopen(char const *path, char const *mode);

// create directory - recursively up to the last '/'
bool fsu_mkdir(char const *path);

// move file
bool fsu_mvfile(char const *from, char const *to, bool in_replace);

// remove file
bool fsu_rmfile(char const *path);

// sleep in millseconds
void sys_sleep(uint32_t ms);

#ifdef _WIN32

// wchar_t <-> utf8 conversion
size_t sys_wchar_from_utf8(char const *in, wchar_t *out, size_t chars);
size_t sys_utf8_from_wchar(wchar_t const *in, char *out, size_t bytes);

// add missing functions to win32
int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);
#define strdup _strdup

#endif
