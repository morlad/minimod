// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#pragma once

#include <stdbool.h>
// for FILE, off_t
#include <stdio.h>


bool util_mkdir(char const *path);
FILE *util_fopen(char const *path, char const *mode);
bool util_mvfile(char const *from, char const *to);
off_t fsu_filesize(char const *path);
