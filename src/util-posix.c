// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>


enum fsu_pathtype
fsu_ptype(char const *in_path)
{
	struct stat sbuffer;
	int const result = stat(in_path, &sbuffer);
	if (result != 0)
	{
		return FSU_PATHTYPE_NONE;
	}
	else if (S_ISDIR(sbuffer.st_mode))
	{
		return FSU_PATHTYPE_DIR;
	}
	else if (S_ISREG(sbuffer.st_mode))
	{
		return FSU_PATHTYPE_FILE;
	}
	else
	{
		return FSU_PATHTYPE_OTHER;
	}
}


int64_t
fsu_fsize(char const *in_path)
{
	struct stat st;
	bool ok = (0 == stat(in_path, &st) && S_ISREG(st.st_mode));
	return ok ? st.st_size : -1;
}


FILE *
fsu_fopen(char const *in_path, char const *in_mode)
{
	// create directory if in_mode contains 'w'
	if (strchr(in_mode, 'w'))
	{
		fsu_mkdir(in_path);
	}
	return fopen(in_path, in_mode);
}


bool
fsu_mkdir(char const *in_dir)
{
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


bool
fsu_rmdir(char const *in_path)
{
	return 0 == rmdir(in_path);
}


bool
fsu_rmdir_recursive(char const *in_path)
{
	printf("[util] fsu_rmdir_recursive(%s)\n", in_path);
	DIR *dir = opendir(in_path);
	struct dirent *entry;
	while ((entry = readdir(dir)))
	{
		if (entry->d_name[0] == '.')
		{
			/* do nothing - skip it */
		}
		else if (entry->d_type == DT_DIR)
		{
			char *subdir;
			asprintf(&subdir, "%s/%s", in_path, entry->d_name);
			fsu_rmdir_recursive(subdir);
			free(subdir);
		}
		else
		{
			char *file;
			asprintf(&file, "%s/%s", in_path, entry->d_name);
			printf("[util] deleting file %s\n", file);
			unlink(file);
			free(file);
		}
	}
	closedir(dir);

	return 0 == rmdir(in_path);
}


static bool
fsu_cpfile(char const *in_srcpath, char const *in_dstpath, bool in_replace)
{
	// fail if something does exist at the destination but in_replace is false
	struct stat st = { 0 };
	if (!in_replace && stat(in_dstpath, &st) == 0)
	{
		return false;
	}

	// if the file cannot be opened, all bets are off.
	FILE *src = fopen(in_srcpath, "rb");
	if (!src)
	{
		return false;
	}

	// make sure the destination directory exists.
	fsu_mkdir(in_dstpath);
	// either the file does not exist, or in_replace is true so make sure
	// the old file does not exist anymore.
	unlink(in_dstpath);

	FILE *dst = fopen(in_dstpath, "wb");
	if (dst)
	{
		char buffer[4096];
		size_t nbytes = 0;
		while ((nbytes = fread(buffer, 1, sizeof buffer, src)) > 0)
		{
			fwrite(buffer, 1, nbytes, dst);
		}
		fclose(dst);
	}
	fclose(src);

	return (dst);
}


bool
fsu_mvfile(char const *in_srcpath, char const *in_dstpath, bool in_replace)
{
	fsu_mkdir(in_dstpath);

	// fail if something does exist at the destination but in_replace is false
	struct stat st = { 0 };
	if (!in_replace && stat(in_dstpath, &st) == 0)
	{
		return false;
	}

	int rv = rename(in_srcpath, in_dstpath);

	if (rv == 0)
	{
		return true;
	}
	else if (rv == -1 && errno == EXDEV)
	{
		// if rename() failed because src and dst were on different
		// file systems use mlfs_cpfile() and mlfs_rmfile()
		if (fsu_cpfile(in_srcpath, in_dstpath, in_replace))
		{
			fsu_rmfile(in_srcpath);
			return true;
		}
	}

	return false;
}


bool
fsu_rmfile(char const *in_path)
{
	return (unlink(in_path) == 0);
}


bool
fsu_enum_dir(
  char const *in_dir,
  fsu_enum_dir_callback in_callback,
  void *in_userdata)
{
	DIR *dir = opendir(in_dir);
	if (!dir)
	{
		return false;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)))
	{
		if (entry->d_name[0] == '.')
		{
			/* do nothing - skip it */
		}
		else if (entry->d_type == DT_DIR)
		{
			in_callback(in_dir, entry->d_name, true, in_userdata);
		}
		else
		{
			in_callback(in_dir, entry->d_name, false, in_userdata);
		}
	}

	closedir(dir);

	return true;
}


void
sys_sleep(uint32_t ms)
{
	usleep(ms * 1000);
}
