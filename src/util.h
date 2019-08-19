// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#pragma once

#include <stdbool.h>
#include <stdint.h>
// for FILE, off_t
#include <stdio.h>


bool util_mkdir(char const *path);
FILE *util_fopen(char const *path, char const *mode);
bool util_mvfile(char const *from, char const *to, bool in_replace);
int64_t fsu_fsize(char const *path);

bool fsu_rmfile(char const *path);
void sys_sleep(uint32_t ms);

#ifdef _WIN32
int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);
#define strdup _strdup
#define unlink _unlink
// sleep in millseconds
size_t sys_wchar_from_utf8(char const *in, wchar_t *out, size_t chars);
size_t sys_utf8_from_wchar(wchar_t const *in, char *out, size_t bytes);
#endif
