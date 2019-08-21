// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#pragma once
#ifndef MINIMOD_NETW_H_INCLUDED
#define MINIMOD_NETW_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


bool
netw_init(void);


void
netw_deinit(void);


enum netw_verb
{
	NETW_VERB_GET,
	NETW_VERB_POST,
	NETW_VERB_PUT,
	NETW_VERB_DELETE,
};


typedef void (*netw_request_callback)(
  void *in_udata,
  void const *in_data,
  size_t in_bytes,
  int error);


// headers are specified as NULL terminated array of NULL terminated strings
// where key and value are one element in the array each.
// i.e. { "Accept", "application/json", NULL }
bool
netw_request(
  enum netw_verb verb,
  char const *uri,
  char const *const headers[],
  void const *body,
  size_t nbody_bytes,
  netw_request_callback callback,
  void *udata);


typedef void (*netw_download_callback)(
  void *in_udata,
  char const *path,
  int error);


// same as netw_request() but received data is written to temporary file
// instead of in-memory buffer.
bool
netw_download_to(
  enum netw_verb verb,
  char const *uri,
  char const *const headers[],
  void const *body,
  size_t nbody_bytes,
  FILE *file,
  netw_download_callback callback,
  void *udata);


// percent-encode *input*. Do not forget to free() the returned string.
char *
netw_percent_encode(char const *input, size_t len, size_t *out_len);

#endif
