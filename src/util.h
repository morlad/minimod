// vi: filetype=c
#pragma once
#ifndef MINIMOD_UTIL_H_INCLUDED
#define MINIMOD_UTIL_H_INCLUDED

/* Title: util
 *
 * Topic: Introduction
 *
 * Some utility functions used by minimod and netw internally.
 */

#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdint.h>
#include <stdio.h> // FILE
#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#define UTIL_HAS_THREADS_H
#include <threads.h>
#else
#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Section: API */

/* Function: enc_base64()
 *
 * Encode *in_srcbytes* bytes from *in_src* into *out_dst* buffer, of size
 * *in_dstbytes*.
 *
 * Returns always the number of bytes required in out_dst.
 * If out_dst is NULL or in_dstbytes is smaller than the required size
 * nothing is written to out_dst.
 */
size_t
enc_base64(
  void const *in_src,
  size_t in_srcbytes,
  void *out_dst,
  size_t in_dstbytes);

/* Enum: fsu_pathtype
 *
 * Types of directory entries.
 *
 * FSU_PATHTYPE_NONE - no directory entry found
 * FSU_PATHTYPE_FILE - regular file
 * FSU_PATHTYPE_DIR - directory
 * FSU_PATHTYPE_OTHER - there is a directory entry, but it is neither a
 *	directory nor a file. i.e. pipe
 */
enum fsu_pathtype
{
	FSU_PATHTYPE_NONE,
	FSU_PATHTYPE_FILE,
	FSU_PATHTYPE_DIR,
	FSU_PATHTYPE_OTHER,
};

/* Callback: fsu_enum_dir_callback()
 *
 * Called by <fsu_enum_dir()> for every entry in a directory.
 *
 * Paramters:
 *	root - Path of the enumerated directory, ending with '/'.
 *		i.e. "/home/johnd/games/"
 *	name - Name of the directory entry
 *	is_dir - true if this entry is a directory, false otherwise
 *	in_userdata - in_userdata passed to <fsu_enum_dir()>
 */
typedef void (*fsu_enum_dir_callback)(
  char const *root,
  char const *name,
  bool is_dir,
  void *in_userdata);

/* Function: fsu_ptype()
 *
 * Check if a path points at a directory, file, nothing or something else.
 *
 * Returns:
 *	Any of <fsu_pathtype>.
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
 * Enumerate a directory by calling in_callback function for every
 * entry in in_dir.
 *
 * Note:
 *	No dot-files are enumerated. (That are files, with a name starting with '.')
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

/* Function: sys_seconds()
 *
 * Gets the number of seconds elapsed from some arbitrary point in time.
 * 
 * Attention:
 *  Do not rely on it being the unix-epoch!
 */
time_t
sys_seconds(void);

#ifndef UTIL_HAS_THREADS_H
// if there is no system/compiler provided implementation of C11's threads.h
// use this barebones mtx-functions to provide the required functionality.
#ifdef _WIN32
typedef CRITICAL_SECTION mtx_t;
#else
typedef pthread_mutex_t mtx_t;
#endif

enum mtx_types
{
	mtx_plain = 0,
};

int
mtx_init(mtx_t *mutex, int type);

int
mtx_lock(mtx_t *mutex);

int
mtx_unlock(mtx_t *mutex);

void
mtx_destroy(mtx_t *mutex);

#endif

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
