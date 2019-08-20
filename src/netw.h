// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct netw_callbacks
{
	void (*completion)(
	  void const *in_udata,
	  void const *in_data,
	  size_t in_bytes,
	  int error);
	void (*downloaded)(void const *in_udata, char const *path, int error);
};

bool
netw_init(struct netw_callbacks *callbacks);

void
netw_deinit(void);


// headers are specified as NULL terminated array of NULL terminated strings
// where key and value are one element in the array each.
// i.e. { "Accept", "application/json", NULL }

// send GET request. Calls *completion* callback when done.
bool
netw_get_request(char const *uri, char const *const headers[], void *udata);

// send POST request. Calls *completion* callback when done.
bool
netw_post_request(
  char const *uri,
  char const *const headers[],
  void const *body,
  size_t nbody_bytes,
  void *udata);

// GET request writing response to temporary file.
// Calls *downloaded* callback first, then *completion*.
bool
netw_download(char const *in_uri, void *udata);

// percent-encode *input*. Do not forget to free() the returned string.
char *
netw_percent_encode(char const *input, size_t len, size_t *out_len);
