// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "netw.h"
#include "util.h"

#include <Windows.h>
#include <assert.h>
#include <stdio.h>
#include <winhttp.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define LOG(FMT, ...) printf("[netw] " FMT "\n", ##__VA_ARGS__)
#pragma GCC diagnostic pop
#define LOG_ERR(X) LOG(X " failed %lu", GetLastError())

// size of download -> file buffer
#define BUFFERSIZE 4096
#define USER_AGENT L"minimod/0.1"


struct netw
{
	HINTERNET session;
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
		Sleep((unsigned)delay);
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
	l_netw.session = WinHttpOpen(
	  USER_AGENT,

	  // TODO if windows < 8.1
	  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
	  // TODO if windows >= 8.1
	  // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,

	  WINHTTP_NO_PROXY_NAME,
	  WINHTTP_NO_PROXY_BYPASS,

	  0);

	if (!l_netw.session)
	{
		LOG_ERR("HttpOpen");
		return false;
	}

	return true;
}


void
netw_deinit(void)
{
	WinHttpCloseHandle(l_netw.session);
}


static char *
combine_headers(char const *const in_headers[], size_t *out_len)
{
	size_t len_headers = 0;
	char const *const *h = in_headers;
	while (*h)
	{
		len_headers += strlen(*(h++));
		len_headers += 2; /* ": " or "\r\n" */
	}
	char *headers = malloc(len_headers + 1 /*NUL*/);

	h = in_headers;
	char *hdrptr = headers;
	while (*h)
	{
		// key
		size_t l = strlen(*h);
		memcpy(hdrptr, *h, l);
		hdrptr += l;
		*(hdrptr++) = ':';
		*(hdrptr++) = ' ';
		++h;

		// value
		l = strlen(*h);
		memcpy(hdrptr, *h, l);
		hdrptr += l;
		*(hdrptr++) = '\r';
		*(hdrptr++) = '\n';
		++h;
	}

	// NUL terminate
	*hdrptr = '\0';

	if (out_len)
	{
		*out_len = len_headers;
	}

	return headers;
}


static wchar_t *
wcstrndup(wchar_t const *in, size_t in_len)
{
	wchar_t *ptr = malloc(sizeof *ptr * (in_len + 1));
	ptr[in_len] = '\0';
	memcpy(ptr, in, sizeof *ptr * in_len);
	return ptr;
}


static void *
memdup(void const *in, size_t bytes)
{
	void *dup = malloc(bytes);
	memcpy(dup, in, bytes);
	return dup;
}


struct task
{
	wchar_t *host;
	wchar_t *path;
	wchar_t *header;
	wchar_t const *verb;
	union
	{
		netw_request_callback request;
		netw_download_callback download;
	} callback;
	void *udata;
	void *payload;
	size_t payload_bytes;
	uint16_t port;
	FILE *file;
};


static DWORD
task_handler(LPVOID context)
{
	struct task *task = context;

	HINTERNET hconnection =
	  WinHttpConnect(l_netw.session, task->host, task->port, 0);
	if (!hconnection)
	{
		LOG_ERR("HttpConnect");
		return false;
	}

	HINTERNET hrequest = WinHttpOpenRequest(
	  hconnection,
	  task->verb,
	  task->path,
	  NULL,
	  WINHTTP_NO_REFERER,
	  WINHTTP_DEFAULT_ACCEPT_TYPES,
	  WINHTTP_FLAG_SECURE);
	if (!hrequest)
	{
		LOG_ERR("HttpOpenRequest");
		return false;
	}

	LOG("Sending request (payload: %zuB)...", task->payload_bytes);
	BOOL ok = WinHttpSendRequest(
	  hrequest,
	  task->header,
	  (DWORD)-1,
	  task->payload,
	  (DWORD)task->payload_bytes,
	  (DWORD)task->payload_bytes,
	  1);
	if (!ok)
	{
		LOG_ERR("HttpSendRequest");
		return false;
	}

	LOG("Waiting for response...");
	ok = WinHttpReceiveResponse(hrequest, NULL);
	if (!ok)
	{
		LOG_ERR("HttpReceiveResponse");
		return false;
	}

	LOG("Query headers...");
	DWORD status_code;
	DWORD sc_bytes = sizeof status_code;
	ok = WinHttpQueryHeaders(
	  hrequest,
	  WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
	  WINHTTP_HEADER_NAME_BY_INDEX,
	  &status_code,
	  &sc_bytes,
	  WINHTTP_NO_HEADER_INDEX);
	if (!ok)
	{
		LOG_ERR("HttpQueryHeaders");
		return false;
	}
	LOG("status code of response: %lu", status_code);

	LOG("Read content...");
	uint8_t *buffer = NULL;
	if (task->file)
	{
		buffer = malloc(BUFFERSIZE);
		DWORD avail_bytes = 0;
		do
		{
			ok = WinHttpQueryDataAvailable(hrequest, &avail_bytes);
			if (!ok)
			{
				LOG_ERR("HttpQueryDataAvailable");
				return false;
			}
			if (avail_bytes > 0)
			{
				DWORD m = avail_bytes <= BUFFERSIZE ? avail_bytes : BUFFERSIZE;
				DWORD actual_bytes_read = 0;
				WinHttpReadData(hrequest, buffer, m, &actual_bytes_read);
				LOG("Read %lu from %lu bytes", actual_bytes_read, avail_bytes);
				size_t nitems =
				  fwrite(buffer, actual_bytes_read, 1, task->file);
				assert(nitems == 1);
			}
		} while (avail_bytes > 0);

		random_delay();
		task->callback.download(task->udata, task->file, (int)status_code);
	}
	else
	{
		size_t bytes = 0;
		DWORD avail_bytes = 0;
		do
		{
			ok = WinHttpQueryDataAvailable(hrequest, &avail_bytes);
			if (!ok)
			{
				LOG_ERR("HttpQueryDataAvailable");
				return false;
			}
			if (avail_bytes > 0)
			{
				buffer = realloc(buffer, bytes + avail_bytes);
				DWORD actual_bytes = 0;
				WinHttpReadData(
				  hrequest,
				  buffer + bytes,
				  avail_bytes,
				  &actual_bytes);
				bytes += actual_bytes;
				LOG("Read %lu from %lu bytes", actual_bytes, avail_bytes);
			}
		} while (avail_bytes > 0);

		random_delay();
		task->callback.request(task->udata, buffer, bytes, (int)status_code);
	}

	// free local data
	free(buffer);
	WinHttpCloseHandle(hrequest);
	WinHttpCloseHandle(hconnection);

	// free task data
	free(task->host);
	free(task->path);
	free(task->header);
	free(task->payload);

	// free actual task
	free(task);

	return true;
}


static wchar_t const *l_verbs[] = { L"GET", L"POST", L"PUT", L"DELETE" };


bool
netw_request(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const in_headers[],
  void const *body,
  size_t nbody_bytes,
  netw_request_callback in_callback,
  void *in_userdata)
{
	if (l_netw.error_rate > 0 && is_random_server_error())
	{
		LOG("Failing request: %s", in_uri);
		in_callback(in_userdata, NULL, 0, 500);
		return true;
	}

	LOG("request: %s", in_uri);

	struct task *task = calloc(sizeof *task, 1);
	task->callback.request = in_callback;
	task->udata = in_userdata;
	task->verb = l_verbs[in_verb];
	if (body && nbody_bytes > 0)
	{
		task->payload = memdup(body, nbody_bytes);
		task->payload_bytes = nbody_bytes;
	}

	// convert/extract URI information
	size_t urilen = sys_wchar_from_utf8(in_uri, NULL, 0);
	wchar_t *uri = malloc(sizeof *uri * urilen);
	sys_wchar_from_utf8(in_uri, uri, urilen);

	URL_COMPONENTS url_components = {
		.dwStructSize = sizeof url_components,
		.dwHostNameLength = (DWORD)-1,
		.dwUrlPathLength = (DWORD)-1,
	};
	WinHttpCrackUrl(uri, (DWORD)urilen, 0, &url_components);

	task->port = url_components.nPort;
	task->host =
	  wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	task->path =
	  wcstrndup(url_components.lpszUrlPath, url_components.dwUrlPathLength);

	free(uri);

	// combine/convert headers
	if (in_headers)
	{
		char *header = combine_headers(in_headers, NULL);

		size_t headerlen = sys_wchar_from_utf8(header, NULL, 0);
		task->header = malloc(sizeof *(task->header) * headerlen);
		sys_wchar_from_utf8(header, task->header, headerlen);

		free(header);
	}

	HANDLE h = CreateThread(NULL, 0, task_handler, task, 0, NULL);
	if (h)
	{
		CloseHandle(h);
	}
	else
	{
		LOG("failed to create thread");
	}

	return true;
}


bool
netw_download_to(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const in_headers[],
  void const *in_body,
  size_t in_nbytes,
  FILE *fout,
  netw_download_callback in_callback,
  void *in_userdata)
{
	assert(fout);
	if (l_netw.error_rate > 0 && is_random_server_error())
	{
		LOG("Failing request: %s", in_uri);
		in_callback(in_userdata, fout, 500);
		return true;
	}
	LOG("download_request: %s", in_uri);

	struct task *task = calloc(sizeof *task, 1);
	task->callback.download = in_callback;
	task->udata = in_userdata;
	task->verb = l_verbs[in_verb];
	task->file = fout;
	if (in_body && in_nbytes > 0)
	{
		task->payload = memdup(in_body, in_nbytes);
		task->payload_bytes = in_nbytes;
	}

	// convert/extract URI information
	size_t urilen = sys_wchar_from_utf8(in_uri, NULL, 0);
	wchar_t *uri = malloc(sizeof *uri * urilen);
	sys_wchar_from_utf8(in_uri, uri, urilen);

	URL_COMPONENTS url_components = {
		.dwStructSize = sizeof url_components,
		.dwHostNameLength = (DWORD)-1,
		.dwUrlPathLength = (DWORD)-1,
	};
	WinHttpCrackUrl(uri, (DWORD)urilen, 0, &url_components);

	task->port = url_components.nPort;
	task->host =
	  wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	task->path =
	  wcstrndup(url_components.lpszUrlPath, url_components.dwUrlPathLength);

	free(uri);

	// combine/convert headers
	if (in_headers)
	{
		char *header = combine_headers(in_headers, NULL);

		size_t headerlen = sys_wchar_from_utf8(header, NULL, 0);
		task->header = malloc(sizeof *(task->header) * headerlen);
		sys_wchar_from_utf8(header, task->header, headerlen);

		free(header);
	}

	HANDLE h = CreateThread(NULL, 0, task_handler, task, 0, NULL);
	if (h)
	{
		CloseHandle(h);
	}
	else
	{
		LOG("failed to create thread");
	}

	return true;
}


void
netw_set_error_rate(int in_percentage)
{
	assert(in_percentage >= 0 && in_percentage <= 100);
	l_netw.error_rate = in_percentage;
}


void
netw_set_delay(int in_min, int in_max)
{
	assert(in_min >= 0);
	assert(in_max >= in_min);
	l_netw.min_delay = in_min;
	l_netw.max_delay = in_max;
}
