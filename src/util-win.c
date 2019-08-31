// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "util.h"

#include <Windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#ifdef MINIMOD_LOG_ENABLE
#define LOG(FMT, ...) printf("[util] " FMT "\n", ##__VA_ARGS__)
#define WLOG(FMT, ...) wprintf("[util] " FMT "\n", ##__VA_ARGS__)
#else
#define LOG(...)
#define WLOG(...)
#endif
#define LOGE(FMT, ...) fprintf(stderr, "[util] " FMT "\n", ##__VA_ARGS__)
#define WLOGE(FMT, ...) wfprintf(stderr, "[util] " FMT "\n", ##__VA_ARGS__)

#define ASSERT(in_condition)                                                 \
	do                                                                       \
	{                                                                        \
		if (__builtin_expect(!(in_condition), 0))                            \
		{                                                                    \
			LOGE(                                                            \
			  "[assertion] %s:%i: '%s'", __FILE__, __LINE__, #in_condition); \
			__asm__ volatile("int $0x03");                                   \
			__builtin_unreachable();                                         \
		}                                                                    \
	} while (__LINE__ == -1)

#pragma GCC diagnostic pop

size_t
sys_utf8_from_wchar(wchar_t const *in, char *out, size_t bytes)
{
	ASSERT(in);
	ASSERT(bytes == 0 || out);
	//  CAST bytes: size_t -> int = assert range
	ASSERT(bytes <= INT_MAX);
	//  CAST retval: int -> size_t = WCTMB always returns >= 0 (error == 0)
	return (size_t)WideCharToMultiByte(
	  CP_UTF8,
	  0,
	  in,
	  -1, // length of 'in'. -1: NUL-terminated
	  out,
	  (int)bytes,
	  NULL, // must be set to 0 for CP_UTF8
	  NULL /* must be set to 0 for CP_UTF8 */);
}


size_t
sys_wchar_from_utf8(char const *in, wchar_t *out, size_t chars)
{
	// CAST chars: size_t -> int = assert range
	ASSERT(chars <= INT_MAX);
	// CAST retval: int -> size_t = MBTWC always returns >= 0 (error == 0)
	return (size_t)MultiByteToWideChar(
	  CP_UTF8,
	  0, // MB_PRECOMPOSED is default.
	  in,
	  -1, // length of 'in'. -1: NUL-terminated
	  out,
	  (int)chars /* size in characters, not bytes! */);
}


bool
fsu_rmfile(char const *in_path)
{
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);
	bool result = (DeleteFileW(utf16) == TRUE);
	free(utf16);
	return result;
}


enum fsu_pathtype
fsu_ptype(char const *in_path)
{
	// convert utf8 to utf16/wide char
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);

	DWORD const result = GetFileAttributes(utf16);
	free(utf16);
	if (result == INVALID_FILE_ATTRIBUTES)
	{
		return FSU_PATHTYPE_NONE;
	}
	if (result & FILE_ATTRIBUTE_DIRECTORY)
	{
		return FSU_PATHTYPE_DIR;
	}
	else
	{
		return FSU_PATHTYPE_FILE;
	}
}


static bool
fsu_recursive_mkdir(wchar_t *in_dir)
{
	// store the first directory to be created, for early outs
	wchar_t *first_hit = NULL;

	// go back, creating directories until one already exists or can be created
	wchar_t *end = in_dir + wcslen(in_dir);
	wchar_t *ptr = end;
	while (ptr >= in_dir)
	{
		if (*ptr == '\\' || *ptr == '/')
		{
			if (!first_hit)
			{
				first_hit = ptr;
			}
			wchar_t old = *ptr;
			*ptr = '\0';
			BOOL result = CreateDirectory(in_dir, NULL);
			DWORD err = GetLastError();
			LOG("CreateDirectory(%ls) %lu", in_dir, err);
			*ptr = old;

			if (result || err == ERROR_ALREADY_EXISTS)
			{
				LOG("- done");
				break;
			}
		}
		--ptr;
	}

	// requested directory was already found or created
	if (ptr == first_hit)
	{
		return true;
	}

	// unable to create any of the required directories
	if (ptr < in_dir)
	{
		return false;
	}

	// now go back forward until the full directory is created
	while (ptr <= first_hit)
	{
		if (*ptr == '\\' || *ptr == '/')
		{
			wchar_t old = *ptr;
			*ptr = '\0';
			BOOL result = CreateDirectory(in_dir, NULL);
			DWORD err = GetLastError();
			LOG("CreateDirectory(%ls) %lu", in_dir, err);
			*ptr = old;

			if (result || err == ERROR_ALREADY_EXISTS)
			{
				LOG("- done");
				if (ptr == end)
				{
					LOG("- final");
					return true;
				}
				++ptr;
				continue;
			}
		}
		++ptr;
	}

	return false;
}


bool
fsu_mkdir(char const *in_path)
{
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);
	bool result = fsu_recursive_mkdir(utf16);
	free(utf16);
	return result;
}


bool
fsu_rmdir(char const *in_path)
{
	// convert to utf16
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);

	RemoveDirectory(utf16);

	free(utf16);

	return true;
}


static bool
fsu_rmdir_recursive_utf16(wchar_t const *in_path)
{
	// pa = path + asterisk
	size_t clen = wcslen(in_path);
	wchar_t *pa = malloc(2 * (clen + 3));
	memcpy(pa, in_path, 2 * clen);
	if (pa[clen - 1] != '/')
	{
		pa[clen + 0] = '/';
		pa[clen + 1] = '*';
		pa[clen + 2] = '\0';
	}
	else
	{
		pa[clen + 0] = '*';
		pa[clen + 1] = '\0';
	}

	WIN32_FIND_DATA fdata;
	HANDLE h;
	if ((h = FindFirstFile(pa, &fdata)))
	{
		do
		{
			if (fdata.cFileName[0] == '.')
			{
				// do nothing, just skip
			}
			else
			{
				size_t path_len = wcslen(in_path);
				size_t file_len = wcslen(fdata.cFileName);
				size_t sub_len = path_len + 1 /*NUL*/ + file_len;
				wchar_t *sub = malloc(sizeof *sub * (sub_len + 1));
				memcpy(sub, in_path, 2 * path_len);
				sub[path_len] = '/';
				memcpy(
				  sub + path_len + 1,
				  fdata.cFileName,
				  2 * (file_len + 1 /*NUL*/));
				if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					fsu_rmdir_recursive_utf16(sub);
				}
				else
				{
					WLOG(L"deleting file: %s", sub);
					DeleteFile(sub);
				}
				free(sub);
			}
		} while (FindNextFile(h, &fdata));
		FindClose(h);
	}

	WLOG(L"RemoveDirectory(%s)", in_path);
	if (!RemoveDirectory(in_path))
	{
		WLOGE(L"Failed to remove %s (%u)", in_path, GetLastError());
	}
	return true;
}


// Why not use SHFileOperation()?
// o It is the only thing requiring shell32, which is no biggy, but still
// o It explicitely warns about using relative pathnames, what we do
//   (but most likely the same problems happen with the hand-written version
//    as well, since it is about other threads changing the current working
//    directory).
// So maybe there is no reason not to use it. Input appreciated.
bool
fsu_rmdir_recursive(char const *in_path)
{
	// convert to utf16
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);

	bool ok = fsu_rmdir_recursive_utf16(utf16);

	free(utf16);

	return ok;
}


#if 0
bool
fsu_rmdir_recursive(char const *in_path)
{
	// convert to utf16
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars > 0);
	// string to SHFileOperation needs to be double-NUL terminated.
	wchar_t *utf16 = calloc(1, sizeof *utf16 * (nchars + 1));
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);

	SHFILEOPSTRUCT op = { 0 };
	op.wFunc = FO_DELETE;
	op.pFrom = utf16;
	op.fFlags = FOF_NO_UI;
	int err = SHFileOperation(&op);
	bool ok = fsu_rmdir_recursive_utf16(utf16);

	free(utf16);

	return !err;
	return ok;
}
#endif


bool
fsu_enum_dir(
  char const *in_dir,
  fsu_enum_dir_callback in_callback,
  void *in_userdata)
{
	// convert to utf16
	size_t nchars = sys_wchar_from_utf8(in_dir, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_dir, utf16, nchars);

	// pa = path + asterisk
	size_t clen = wcslen(utf16);
	wchar_t *pa = malloc(2 * (clen + 3));
	memcpy(pa, utf16, 2 * clen);
	if (pa[clen - 1] != '/')
	{
		pa[clen + 0] = '/';
		pa[clen + 1] = '*';
		pa[clen + 2] = '\0';
	}
	else
	{
		pa[clen + 0] = '*';
		pa[clen + 1] = '\0';
	}
	WLOG(L"enum-pa = '%s'", pa);

	WIN32_FIND_DATA fdata;
	HANDLE h = FindFirstFile(pa, &fdata);
	if (!h)
	{
		return false;
	}
	else
	{
		do
		{
			// convert fdata.cFileName
			size_t nbytes = sys_utf8_from_wchar(fdata.cFileName, NULL, 0);
			ASSERT(nbytes > 0);
			char *utf8 = malloc(nbytes);
			sys_utf8_from_wchar(fdata.cFileName, utf8, nbytes);

			if (fdata.cFileName[0] == '.')
			{
				// do nothing, just skip
			}
			else if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				in_callback(in_dir, utf8, true, in_userdata);
			}
			else
			{
				in_callback(in_dir, utf8, false, in_userdata);
			}
			free(utf8);
		} while (FindNextFile(h, &fdata));
		FindClose(h);
	}

	return true;
}


FILE *
fsu_fopen(char const *in_path, char const *in_mode)
{
	ASSERT(in_mode);

	// convert to utf16
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);

	bool has_write = false;
	// convert mode
	wchar_t wmode[8] = { 0 };
	for (size_t i = 0; i < 8 && in_mode[i]; ++i)
	{
		wmode[i] = (unsigned char)in_mode[i];
		if (in_mode[i] == 'w')
		{
			has_write = true;
		}
	}

	// create directory if mode contains 'w'
	if (has_write)
	{
		fsu_mkdir(in_path);
	}

	FILE *f = _wfopen(utf16, wmode);
	free(utf16);
	return f;
}


bool
fsu_mvfile(char const *in_srcpath, char const *in_dstpath, bool in_replace)
{
	// copy allowed: don't care when destination is on another volume
	// write through: make sure the move is finished before proceeding
	//                'security' over speed
	DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH;
	if (in_replace)
	{
		flags |= MOVEFILE_REPLACE_EXISTING;
	}

	// convert in_srcpath to utf16
	size_t nchars = sys_wchar_from_utf8(in_srcpath, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *srcpath = malloc(nchars * sizeof *srcpath);
	sys_wchar_from_utf8(in_srcpath, srcpath, nchars);

	// convert in_dstpath to utf16
	nchars = sys_wchar_from_utf8(in_dstpath, NULL, 0);
	ASSERT(nchars > 0);
	wchar_t *dstpath = malloc(nchars * sizeof *dstpath);
	sys_wchar_from_utf8(in_dstpath, dstpath, nchars);

	BOOL result = MoveFileExW(srcpath, dstpath, flags);
	if (!result)
	{
		LOGE("MoveFileEx#1 failed %lu", GetLastError());
	}
	if (result == FALSE && GetLastError() == ERROR_PATH_NOT_FOUND)
	{
		fsu_recursive_mkdir(dstpath);
		result = MoveFileExW(srcpath, dstpath, flags);
		if (!result)
		{
			LOGE("MoveFileEx#2 failed %lu", GetLastError());
		}
	}

	free(srcpath);
	free(dstpath);

	return (result == TRUE);
}

int64_t
fsu_fsize(char const *in_path)
{
	// convert to utf16
	size_t nchars = sys_wchar_from_utf8(in_path, NULL, 0);
	ASSERT(nchars);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	sys_wchar_from_utf8(in_path, utf16, nchars);

	HANDLE file = CreateFile(
	  utf16,
	  GENERIC_READ,
	  FILE_SHARE_READ | FILE_SHARE_WRITE,
	  NULL,
	  OPEN_EXISTING,
	  FILE_ATTRIBUTE_NORMAL,
	  NULL);
	free(utf16);

	// early out on failure
	if (file == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	LARGE_INTEGER size = { .QuadPart = 0 };
	GetFileSizeEx(file, &size);
	CloseHandle(file);
	return size.QuadPart;
}


void
sys_sleep(uint32_t ms)
{
	Sleep(ms);
}


int
asprintf(char **strp, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	int size = vasprintf(strp, fmt, args);

	va_end(args);

	return size;
}


int
vasprintf(char **strp, const char *fmt, va_list args)
{
	va_list tmpa;
	va_copy(tmpa, args);

	int size = vsnprintf(NULL, 0, fmt, tmpa);

	va_end(tmpa);

	if (size < 0)
	{
		return -1;
	}

	*strp = malloc((size_t)size + 1 /*NUL*/);
	if (NULL == *strp)
	{
		return -1;
	}

	return vsprintf(*strp, fmt, args);
}


time_t
sys_seconds(void)
{
	return time(NULL);
}


#ifndef UTIL_HAS_THREADS_H
int
mtx_init(mtx_t *mutex, int type)
{
	InitializeCriticalSection(mutex);
	return 0;
}


void
mtx_destroy(mtx_t *mutex)
{
	DeleteCriticalSection(mutex);
}


int
mtx_lock(mtx_t *mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}


int
mtx_unlock(mtx_t *mutex)
{
	LeaveCriticalSection(mutex);
	return 0;
}
#endif
