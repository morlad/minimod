// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "netw.h"

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma GCC diagnostic ignored "-Wunused-macros"

#define LOG(FMT, ...) printf("[netw] " FMT "\n", ##__VA_ARGS__)

#define ASSERT(in_condition)                                                 \
	do                                                                       \
	{                                                                        \
		if (__builtin_expect(!(in_condition), 0))                            \
		{                                                                    \
			LOG(                                                             \
			  "[assertion] %s:%i: '%s'", __FILE__, __LINE__, #in_condition); \
			__asm__ volatile("int $0x03");                                   \
			__builtin_unreachable();                                         \
		}                                                                    \
	} while (__LINE__ == -1)

#pragma GCC diagnostic pop

struct netw
{
	int error_rate;
	int min_delay;
	int max_delay;
};
static struct netw l_netw;


static void
random_delay()
{
	if (l_netw.max_delay > 0)
	{
		int delay = (l_netw.max_delay > l_netw.min_delay)
			? l_netw.min_delay + (rand() % (l_netw.max_delay - l_netw.min_delay))
			: l_netw.min_delay;
		LOG("adding delay: %i ms", delay);
		usleep((unsigned)delay * 1000);
	}
}


static bool
is_random_server_error(void)
{
	if (l_netw.error_rate > (rand() % 100))
	{
		random_delay();
		return true;
	}
	return false;
}


bool
netw_init(void)
{
	LOG("curl: %s", curl_version());

	curl_global_init(CURL_GLOBAL_ALL);

	return true;
}


void
netw_deinit(void)
{
	curl_global_cleanup();
}


struct task
{
	CURL *curl;
	struct curl_slist *header_list;
	union
	{
		netw_request_callback request;
		netw_download_callback download;
	} callback;
	void *udata;
	uint8_t *buffer;
	size_t bytes;
	FILE *file;
};


static size_t
on_curl_write_memory(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct task *task = userdata;
	size_t len = size * nmemb;
	task->buffer = realloc(task->buffer, len + task->bytes);
	memcpy(task->buffer + task->bytes, ptr, len);
	task->bytes += len;
	return len;
}


struct netw_header
{
	char **keys;
	char **values;
	size_t nkeys;
	size_t nreserved;
};


static void *
memdup(void const *in, size_t in_bytes)
{
	void *ptr = malloc(in_bytes);
	memcpy(ptr, in, in_bytes);
	return ptr;
}


static size_t
hdr_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
	struct netw_header *hdr = userdata;

	// resize buffer if necessary
	if (hdr->nkeys == hdr->nreserved)
	{
		hdr->nreserved = hdr->nreserved ? 2 * hdr->nreserved : 16;
		hdr->keys = realloc(hdr->keys, sizeof *hdr->keys * hdr->nreserved);
		hdr->keys[hdr->nkeys] = NULL;
		hdr->values =
		  realloc(hdr->values, sizeof *hdr->values * hdr->nreserved);
	}

	// check if line contains a colon
	char *colon = memchr(buffer, ':', size * nitems);
	if (colon)
	{
		hdr->keys[hdr->nkeys] = memdup(buffer, size * nitems);
		hdr->keys[hdr->nkeys][colon - buffer] = '\0';

		hdr->values[hdr->nkeys] = hdr->keys[hdr->nkeys] + (colon - buffer) + 1;
		// trim leading whitespace
		while (isspace(*hdr->values[hdr->nkeys]))
		{
			hdr->values[hdr->nkeys] += 1;
		}
		// trim trailing whitespace
		char *ptr = hdr->keys[hdr->nkeys] + size * nitems - 1;
		while (isspace(*ptr))
		{
			*ptr = '\0';
			ptr -= 1;
		}

		hdr->nkeys += 1;
	}

	return size * nitems;
}


static void
free_netw_header(struct netw_header *hdr)
{
	for (size_t i = 0; i < hdr->nkeys; ++i)
	{
		free(hdr->keys[i]);
	}
	free(hdr->keys);
	free(hdr->values);
}


static void *
task_handler(void *in_context)
{
	struct task *task = in_context;

	curl_easy_setopt(task->curl, CURLOPT_VERBOSE, 0);

	curl_easy_setopt(task->curl, CURLOPT_FOLLOWLOCATION, 1);

	struct netw_header hdr = { 0 };
	curl_easy_setopt(task->curl, CURLOPT_HEADERDATA, &hdr);
	curl_easy_setopt(task->curl, CURLOPT_HEADERFUNCTION, hdr_callback);

	curl_easy_perform(task->curl);

	curl_slist_free_all(task->header_list);

	long status_code;
	curl_easy_getinfo(task->curl, CURLINFO_RESPONSE_CODE, &status_code);
	LOG("status_code: %li", status_code);


	random_delay();
	if (task->file)
	{
		LOG("probably written to FILE");
		task->callback
			.download(task->udata, task->file, (int)status_code, &hdr);
	}
	else
	{
		LOG("received bytes: %zu", task->bytes);
		task->callback.request(
		  task->udata,
		  task->buffer,
		  task->bytes,
		  (int)status_code,
		  &hdr);
	}

	free_netw_header(&hdr);
	curl_easy_cleanup(task->curl);
	free(task->buffer);
	free(task);

	return NULL;
}


static struct curl_slist *
build_header_list(char const *const headers[])
{
	struct curl_slist *list = NULL;
	for (size_t i = 0; headers[i]; i += 2)
	{
		size_t len_field = strlen(headers[i]);
		size_t len_value = strlen(headers[i + 1]);
		char *line = malloc(len_field + 2 + len_value + 1);
		memcpy(line, headers[i], len_field);
		memcpy(line + len_field, ": ", 2);
		// (len_value + 1) to copy terminating NUL
		memcpy(line + len_field + 2, headers[i + 1], len_value + 1);

		list = curl_slist_append(list, line);
		free(line);
	}
	return list;
}


bool
netw_request(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const headers[],
  void const *in_body,
  size_t in_nbytes,
  netw_request_callback in_callback,
  void *in_userdata)
{
	if (l_netw.error_rate > 0 && is_random_server_error())
	{
		LOG("Failing request: %s", in_uri);
		in_callback(in_userdata, NULL, 0, 500, NULL);
		return true;
	}

	struct task *task = calloc(1, sizeof *task);

	task->callback.request = in_callback;
	task->udata = in_userdata;

	task->curl = curl_easy_init();

    curl_off_t nbody_bytes = (curl_off_t)in_nbytes;
	switch (in_verb)
	{
	case NETW_VERB_GET:
		curl_easy_setopt(task->curl, CURLOPT_HTTPGET, 1);
		break;
	case NETW_VERB_POST:
		curl_easy_setopt(task->curl, CURLOPT_POSTFIELDSIZE_LARGE, nbody_bytes);
		curl_easy_setopt(task->curl, CURLOPT_COPYPOSTFIELDS, in_body);
		curl_easy_setopt(task->curl, CURLOPT_POST, 1);
		break;
	case NETW_VERB_PUT:
		curl_easy_setopt(task->curl, CURLOPT_POSTFIELDSIZE_LARGE, nbody_bytes);
		curl_easy_setopt(task->curl, CURLOPT_COPYPOSTFIELDS, in_body);
		curl_easy_setopt(task->curl, CURLOPT_UPLOAD, 1);
		break;
	case NETW_VERB_DELETE:
		curl_easy_setopt(task->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
		break;
#if 0
	case NETW_VERB_HEAD:
		curl_easy_setopt(task->curl, CURLOPT_NOBODY, 1);
		break;
#endif
	}

	curl_easy_setopt(task->curl, CURLOPT_URL, in_uri);

	if (headers)
	{
		task->header_list = build_header_list(headers);
		curl_easy_setopt(task->curl, CURLOPT_HTTPHEADER, task->header_list);
	}

	curl_easy_setopt(task->curl, CURLOPT_WRITEFUNCTION, on_curl_write_memory);
	curl_easy_setopt(task->curl, CURLOPT_WRITEDATA, task);

	pthread_t tid;
	int err = pthread_create(&tid, NULL, task_handler, task);
	return (err == 0);
}


bool
netw_download_to(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const headers[],
  void const *in_body,
  size_t in_nbytes,
  FILE *fout,
  netw_download_callback in_callback,
  void *in_userdata)
{
	ASSERT(fout);
	if (l_netw.error_rate > 0 && is_random_server_error())
	{
		LOG("Failing request: %s", in_uri);
		in_callback(in_userdata, fout, 500, NULL);
		return true;
	}

	struct task *task = calloc(1, sizeof *task);

	task->file = fout;

	task->callback.download = in_callback;
	task->udata = in_userdata;

	task->curl = curl_easy_init();

    curl_off_t nbody_bytes = (curl_off_t)in_nbytes;
	switch (in_verb)
	{
	case NETW_VERB_GET:
		curl_easy_setopt(task->curl, CURLOPT_HTTPGET, 1);
		break;
	case NETW_VERB_POST:
		curl_easy_setopt(task->curl, CURLOPT_POSTFIELDSIZE_LARGE, nbody_bytes);
		curl_easy_setopt(task->curl, CURLOPT_COPYPOSTFIELDS, in_body);
		curl_easy_setopt(task->curl, CURLOPT_POST, 1);
		break;
	case NETW_VERB_PUT:
		curl_easy_setopt(task->curl, CURLOPT_POSTFIELDSIZE_LARGE, nbody_bytes);
		curl_easy_setopt(task->curl, CURLOPT_COPYPOSTFIELDS, in_body);
		curl_easy_setopt(task->curl, CURLOPT_UPLOAD, 1);
		break;
	case NETW_VERB_DELETE:
		curl_easy_setopt(task->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
		break;
#if 0
	case NETW_VERB_HEAD:
		curl_easy_setopt(task->curl, CURLOPT_NOBODY, 1);
		break;
#endif
	}

	curl_easy_setopt(task->curl, CURLOPT_URL, in_uri);

	if (headers)
	{
		task->header_list = build_header_list(headers);
		curl_easy_setopt(task->curl, CURLOPT_HTTPHEADER, task->header_list);
	}

	curl_easy_setopt(task->curl, CURLOPT_WRITEDATA, task->file);

	pthread_t tid;
	int err = pthread_create(&tid, NULL, task_handler, task);
	return (err == 0);
}


void
netw_set_error_rate(int in_percentage)
{
	ASSERT(in_percentage >= 0 && in_percentage <= 100);
	l_netw.error_rate = in_percentage;
}


void
netw_set_delay(int in_min, int in_max)
{
	ASSERT(in_min >= 0);
	ASSERT(in_max >= in_min);
	l_netw.min_delay = in_min;
	l_netw.max_delay = in_max;
}


char const *
netw_get_header(struct netw_header const *in_hdr, char const *in_key)
{
	for (size_t i = 0; i < in_hdr->nkeys; ++i)
	{
		if (strcasecmp(in_key, in_hdr->keys[i]) == 0)
		{
			return in_hdr->values[i];
		}
	}
	return NULL;
}
