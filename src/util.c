// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#else
#	include <sys/stat.h>
#	include <errno.h>
#endif

#ifdef _WIN32
static bool util_recursive_mkdir(wchar_t *in_dir)
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
      LOGD("<CreateDirectory(%ls) %lu", in_dir, err);
      *ptr = old;

      if (result || err == ERROR_ALREADY_EXISTS)
      {
        LOGD("- done");
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
      LOGD(">CreateDirectory(%ls) %lu", in_dir, err);
      *ptr = old;

      if (result || err == ERROR_ALREADY_EXISTS)
      {
        LOGD("- done");
        if (ptr == end)
        {
          LOGD("- final");
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


bool util_mkdir(char const *in_path)
{
	size_t nchars = ml_wchar_from_utf8(in_path, NULL, 0);
	assert(nchars > 0);
	wchar_t *utf16 = malloc(nchars * sizeof *utf16);
	ml_wchar_from_utf8(in_path, utf16, nchars);
	bool result = mlfs__recursive_mkdir(utf16);
	free(utf16);
	return result;
}
#else
bool util_mkdir(char const *in_dir)
{
  assert(in_dir);

  // check if directory already exists
  struct stat sbuffer;
  if (stat(in_dir, &sbuffer) == 0)
  {
    return true;
  }

  char *dir = strdup(in_dir);
  char *ptr = dir;
  while (*(++ptr))
  {
    if (*ptr == '/')
    {
      *ptr = '\0';
      if (mkdir(dir, 0777 /* octal mode */) == -1 && errno != EEXIST)
      {
        free(dir);
        return false;
      }
      *ptr = '/';
    }
  }
  free(dir);
  return true;
}
#endif



#ifdef _WIN32
#else
FILE *util_fopen(char const *path, char const *mode)
{
	// create directory if mode contains 'w'
	if (strchr(mode, 'w'))
	{
		util_mkdir(path);
	}
	return fopen(path, mode);
}
#endif


#ifdef _WIN32
#else
bool util_mvfile(char const *from, char const *to)
{
	util_mkdir(to);
	rename(from, to);
	return true;
}
#endif

#ifdef _WIN32
#else
off_t fsu_filesize(char const *in_path)
{
  struct stat st;
  bool ok = (0 == stat(in_path, &st) && S_ISREG(st.st_mode));
  return ok ? st.st_size : -1;
}
#endif
