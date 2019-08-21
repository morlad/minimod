// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "netw.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

struct netw
{
	struct netw_callbacks callbacks;
};
static struct netw l_netw;


bool
netw_init(struct netw_callbacks *in_callbacks)
{
	l_netw.callbacks = *in_callbacks;

	printf("[netw] curl: %s\n", curl_version());

	curl_global_init(CURL_GLOBAL_ALL);

	return true;
}


void
netw_deinit(void)
{
	curl_global_cleanup();
}


bool
netw_get_request(
  char const *in_uri,
  char const *const headers[],
  void *in_udata)
{
	return netw_request(
	  NETW_VERB_GET,
	  in_uri,
	  headers,
	  NULL,
	  0,
	  l_netw.callbacks.completion,
	  in_udata);
}


bool
netw_post_request(
  char const *in_uri,
  char const *const headers[],
  void const *in_body,
  size_t in_nbytes,
  void *in_udata)
{
	return netw_request(
	  NETW_VERB_POST,
	  in_uri,
	  headers,
	  in_body,
	  in_nbytes,
	  l_netw.callbacks.completion,
	  in_udata);
}


bool
netw_download(char const *in_uri, void *in_udata)
{
	return netw_request_download(
	  NETW_VERB_GET,
	  in_uri,
	  NULL,
	  NULL,
	  0,
	  l_netw.callbacks.downloaded,
	  in_udata);
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


static void *
task_handler(void *in_context)
{
	struct task *task = in_context;

	CURLcode err = curl_easy_perform(task->curl);
	curl_slist_free_all(task->header_list);

	long status_code;
	curl_easy_getinfo(task->curl, CURLINFO_RESPONSE_CODE, &status_code);
	printf("[netw] status_code: %li\n", status_code);

	printf("[netw] received bytes: %zu\n", task->bytes);

	task->callback.request(task->udata, task->buffer, task->bytes, (int)status_code);

	// TODO free all header lines
	curl_easy_cleanup(task->curl);
	free(task->buffer);
	free(task);

	return NULL;
}


static int
on_curl_debug(
  CURL *handle,
  curl_infotype type,
  char *data,
  size_t size,
  void *userptr)
{
	if (type == CURLINFO_HEADER_OUT)
	{
		char *text = calloc(1, size + 1);
		memcpy(text, data, size);
		printf("[netw-curl] header out\n--\n%s\n--\n", text);
		free(text);
	}

	return 0;
}


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


bool
netw_request(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const headers[],
  void const *in_body,
  size_t in_nbytes,
  netw_request_callback in_callback,
  void *in_udata)
{
	struct task *task = calloc(1, sizeof *task);

	task->callback.request = in_callback;
	task->udata = in_udata;
	task->curl = curl_easy_init();
	curl_easy_setopt(task->curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(task->curl, CURLOPT_DEBUGFUNCTION, on_curl_debug);

	curl_easy_setopt(task->curl, CURLOPT_WRITEFUNCTION, on_curl_write_memory);
	curl_easy_setopt(task->curl, CURLOPT_WRITEDATA, task);

	switch (in_verb)
	{
	case NETW_VERB_GET:
		curl_easy_setopt(task->curl, CURLOPT_HTTPGET, 1);
		break;
	case NETW_VERB_POST:
		curl_easy_setopt(task->curl, CURLOPT_POST, 1);
		curl_easy_setopt(task->curl, CURLOPT_COPYPOSTFIELDS, in_body);
		curl_off_t nbody_bytes = (curl_off_t)in_nbytes;
		curl_easy_setopt(task->curl, CURLOPT_POSTFIELDSIZE_LARGE, nbody_bytes);
		break;
	case NETW_VERB_PUT:
		curl_easy_setopt(task->curl, CURLOPT_UPLOAD, 1);
		// TODO data?
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
		for (size_t i = 0; headers[i]; i += 2)
		{
			size_t len_field = strlen(headers[i]);
			size_t len_value = strlen(headers[i + 1]);
			char *line = malloc(len_field + 2 + len_value + 1);
			memcpy(line, headers[i], len_field);
			memcpy(line + len_field, ": ", 2);
			// (len_value + 1) to copy terminating NUL
			memcpy(line + len_field + 2, headers[i + 1], len_value + 1);

			task->header_list = curl_slist_append(task->header_list, line);
		}
		curl_easy_setopt(task->curl, CURLOPT_HTTPHEADER, task->header_list);
	}

	pthread_t tid;
	int err = pthread_create(&tid, NULL, task_handler, task);
	return (err == 0);
}


bool
netw_request_download(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const headers[],
  void const *in_body,
  size_t in_nbytes,
  netw_download_callback in_callback,
  void *in_udata)
{
	return true;
}
