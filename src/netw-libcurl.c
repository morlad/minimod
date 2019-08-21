// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "netw.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>


bool
netw_init(void)
{
	printf("[netw] curl: %s\n", curl_version());

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


static void *
task_handler(void *in_context)
{
	struct task *task = in_context;

	curl_easy_setopt(task->curl, CURLOPT_VERBOSE, 0);

	curl_easy_setopt(task->curl, CURLOPT_FOLLOWLOCATION, 1);

	curl_easy_perform(task->curl);

	curl_slist_free_all(task->header_list);

	long status_code;
	curl_easy_getinfo(task->curl, CURLINFO_RESPONSE_CODE, &status_code);
	printf("[netw] status_code: %li\n", status_code);


	if (task->file)
	{
		printf("[netw] probably written to FILE\n");
		task->callback.download(task->udata, "tmppath", (int)status_code);
	}
	else
	{
		printf("[netw] received bytes: %zu\n", task->bytes);
		task->callback
		  .request(task->udata, task->buffer, task->bytes, (int)status_code);
	}

	// TODO free all header lines
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
  void *in_udata)
{
	struct task *task = calloc(1, sizeof *task);

	task->callback.request = in_callback;
	task->udata = in_udata;

	task->curl = curl_easy_init();

	switch (in_verb)
	{
	case NETW_VERB_GET:
		curl_easy_setopt(task->curl, CURLOPT_HTTPGET, 1);
		break;
	case NETW_VERB_POST:
		curl_easy_setopt(task->curl, CURLOPT_POST, 1);
		curl_off_t nbody_bytes = (curl_off_t)in_nbytes;
		curl_easy_setopt(task->curl, CURLOPT_POSTFIELDSIZE_LARGE, nbody_bytes);
		curl_easy_setopt(task->curl, CURLOPT_COPYPOSTFIELDS, in_body);
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
  void *in_udata)
{
	assert(fout);

	struct task *task = calloc(1, sizeof *task);

	task->file = fout;

	task->callback.download = in_callback;
	task->udata = in_udata;

	task->curl = curl_easy_init();

	switch (in_verb)
	{
	case NETW_VERB_GET:
		curl_easy_setopt(task->curl, CURLOPT_HTTPGET, 1);
		break;
	case NETW_VERB_POST:
		curl_easy_setopt(task->curl, CURLOPT_POST, 1);
		curl_off_t nbody_bytes = (curl_off_t)in_nbytes;
		curl_easy_setopt(task->curl, CURLOPT_POSTFIELDSIZE_LARGE, nbody_bytes);
		curl_easy_setopt(task->curl, CURLOPT_COPYPOSTFIELDS, in_body);
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
		task->header_list = build_header_list(headers);
		curl_easy_setopt(task->curl, CURLOPT_HTTPHEADER, task->header_list);
	}

	curl_easy_setopt(task->curl, CURLOPT_WRITEDATA, task->file);

	pthread_t tid;
	int err = pthread_create(&tid, NULL, task_handler, task);
	return (err == 0);
}
