// vi: ft=c noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#pragma once
#ifndef MINIMOD_UTIL_H_INCLUDED
#define MINIMOD_UTIL_H_INCLUDED

/* Title: util
 *
 * Topic: Introduction
 *
 * Some utility functions used by minimod and netw.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> // FILE

#ifdef __cplusplus
extern "C" {
#endif

/* Section: API */


/* Enum: fsu_pathtype
 */
enum fsu_pathtype
{
	FSU_PATHTYPE_NONE,
	FSU_PATHTYPE_FILE,
	FSU_PATHTYPE_DIR,
	FSU_PATHTYPE_OTHER,
};


/** Callback: fsu_enum_dir_callback()
 */
typedef void (*fsu_enum_dir_callback)(
  char const *root,
  char const *name,
  bool is_dir,
  void *in_userdata);


/* Function: fsu_ptype()
 *
 * Check if a path points at a directory, file, nothing or something else.
 */
enum fsu_pathtype
fsu_ptype(char const *path);


/* Function: fsu_fsize()
 *
 *	Get byte-size of file.
 *
 *	Returns:
 *		-1 on error/if file does not exist.
 */
int64_t
fsu_fsize(char const *path);


/* Function: fsu_fopen()
 *
 *	fopen()-wrapper that does 2 things
 *	o convert path (utf8) to wchar_t for windows to be happy.
 *	o create missing directories (if any) of path, if mode contains 'w' or 'a'
 */
FILE *
fsu_fopen(char const *path, char const *mode);


/* Function: fsu_mkdir()
 *
 *	Create directory. Recursively up to the last '/'.
 */
bool
fsu_mkdir(char const *path);


/* Function: fsu_rmdir()
 *
 *	TL;DR: rm -f
 */
bool
fsu_rmdir(char const *path);


/* Function: fsu_rmdir_recursive()
 *
 *	TL;DR: rm -rf
 */
bool
fsu_rmdir_recursive(char const *path);


/* Function: fsu_mvfile()
 *
 *	Move file. Creates required directories automatically.
 */
bool
fsu_mvfile(char const *from, char const *to, bool in_replace);


/* Function: fsu_rmfile()
 *
 *	Remove file.
 */
bool
fsu_rmfile(char const *path);


/* Function: fsu_enum_dir()
 *
 * Parameters:
 *	in_dir - Needs to end with '/' and be a valid directory.
 */
bool
fsu_enum_dir(
  char const *in_dir,
  fsu_enum_dir_callback in_callback,
  void *in_userdata);


/* Function: sys_sleep()
 *
 * Sleep thread for certain amount of milliseconds.
 */
void
sys_sleep(uint32_t ms);


#ifdef _WIN32

/* Function: sys_wchar_from_utf8()
 *
 * Convert utf8 to wchar_t
 *
 * Parameters:
 *	out - If *out* is NULL, no conversion takes place but the
 *		function returns the number of wchar_t required to hold the
 *		converted string (excluding terminating NUL)
 */
size_t
sys_wchar_from_utf8(char const *in, wchar_t *out, size_t chars);


/* Function: sys_utf8_from_wchar()
 *
 * Convert wchar_t to utf8
 *
 * Parameters:
 *	out - If *out* is NULL, no conversion takes place but the
 *		function returns the number of bytes required to hold the
 *		converted string (excluding terminating NUL)
 */
size_t
sys_utf8_from_wchar(wchar_t const *in, char *out, size_t bytes);


// add missing functions to win32
int
asprintf(char **strp, const char *fmt, ...);
int
vasprintf(char **strp, const char *fmt, va_list ap);
#define strdup _strdup

#endif // _WIN32

#ifdef __cplusplus
} // extern "C"
#endif

#endif
