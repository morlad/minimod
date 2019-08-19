// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct netw_callbacks
{
	void (*completion)(
		uint64_t in_request_id,
		void const *in_data,
		size_t in_bytes,
		int error);
	void (*downloaded)(
		uint64_t in_request_id,
		char const *path,
		int error);
};

bool netw_init(struct netw_callbacks *callbacks);

void netw_deinit(void);

uint64_t
netw_get_request(char const *uri, char const *const headers[]);

uint64_t
netw_post_request(
	char const *uri,
	char const *const headers[],
	void const *body,
	size_t nbody_bytes);

char *
netw_percent_encode(
  char const *input,
  size_t len,
  size_t *out_len);

uint64_t
netw_download(char const *in_uri);
