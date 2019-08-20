// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#include "netw.h"
#include "util.h"

#include <stdio.h>
#include <Windows.h>
#include <winhttp.h>

// size of download -> file buffer
#define BUFFERSIZE 4096
#define USER_AGENT L"minimod/0.1"
#define TEMPFILE_PREFIX L"mmi"

#define PRINTERR(X) \
	printf("[netw] " X " failed %lu (%lx)\n", GetLastError(), GetLastError())

struct netw
{
	HINTERNET session;
	struct netw_callbacks callbacks;
};
static struct netw l_netw;


bool
netw_init(struct netw_callbacks *in_callbacks)
{
	l_netw.callbacks = *in_callbacks;

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
		PRINTERR("HttpOpen");
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
	wchar_t *verb;
	void *udata;
	void *payload;
	size_t payload_bytes;
	uint16_t port;
	bool is_download;
};


static HANDLE
create_temp_file(wchar_t out_path[MAX_PATH])
{
	// funny how GetTempPath() requires up to MAX_PATH+1 characters without
	// the terminating \0, while GetTempFileName(), which appends to this
	// very string requires its output-buffer to be MAX_PATH only (including
	// \0). And in fact GetTempFileName() fails with ERROR_BUFFER_OVERFLOW
	// when its first argument is > MAX_PATH-14.
	// Well done Microsoft.
	wchar_t temp_dir[MAX_PATH + 1 + 1] = { 0 };
	GetTempPathW(MAX_PATH + 1, temp_dir);
	GetTempFileName(temp_dir, TEMPFILE_PREFIX, 0, out_path);
	wprintf(L"[netw] Setting up temporary file: %s\n", out_path);
	HANDLE hfile = CreateFile(
	  out_path,
	  GENERIC_WRITE,
	  FILE_SHARE_READ | FILE_SHARE_DELETE,
	  NULL,
	  CREATE_ALWAYS,
	  FILE_ATTRIBUTE_TEMPORARY,
	  NULL);
	return hfile;
}


static DWORD
task_handler(LPVOID context)
{
	struct task *task = context;

	HINTERNET hconnection =
	  WinHttpConnect(l_netw.session, task->host, task->port, 0);
	if (!hconnection)
	{
		PRINTERR("HttpConnect");
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
		PRINTERR("HttpOpenRequest");
		return false;
	}

	printf("[netw] Sending request (payload: %zuB)...\n", task->payload_bytes);
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
		PRINTERR("HttpSendRequest");
		return false;
	}

	printf("[netw] Waiting for response...\n");
	ok = WinHttpReceiveResponse(hrequest, NULL);
	if (!ok)
	{
		PRINTERR("HttpReceiveResponse");
		return false;
	}

	printf("[netw] Query headers...\n");
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
		PRINTERR("HttpQueryHeaders");
		return false;
	}
	printf("[netw] status code of response: %lu\n", status_code);

	printf("[netw] Read content...\n");
	uint8_t *buffer = NULL;
	if (task->is_download)
	{
		wchar_t temp_path[MAX_PATH] = { 0 };
		HANDLE hfile = NULL;
		printf("[netw] Setting up temporary file\n");

		hfile = create_temp_file(temp_path);
		if (!hfile)
		{
			printf("[netw] Failed to create temporary file\n");
			return false;
		}
		buffer = malloc(BUFFERSIZE);
		DWORD avail_bytes = 0;
		do
		{
			ok = WinHttpQueryDataAvailable(hrequest, &avail_bytes);
			if (!ok)
			{
				PRINTERR("HttpQueryDataAvailable");
				return false;
			}
			if (avail_bytes > 0)
			{
				DWORD actual_bytes_read = 0;
				WinHttpReadData(hrequest, buffer, BUFFERSIZE, &actual_bytes_read);
				printf(
				  "[netw] Read %lu from %lu bytes\n",
				  actual_bytes_read,
				  avail_bytes);
				DWORD actual_bytes_written = 0;
				do
				{
					WriteFile(
					  hfile,
					  buffer,
					  actual_bytes_read,
					  &actual_bytes_written,
					  NULL);
					actual_bytes_read -= actual_bytes_written;
				} while (actual_bytes_read > 0);
				printf(
				  "[netw] Written %lu from %lu bytes\n",
				  actual_bytes_written,
				  actual_bytes_read);
			}
		} while (avail_bytes > 0);

		// convert path to utf8
		size_t pathlen = sys_utf8_from_wchar(temp_path, NULL, 0);
		char *u8path = malloc(pathlen);
		sys_utf8_from_wchar(temp_path, u8path, pathlen);

		l_netw.callbacks.downloaded(task->udata, u8path, (int)status_code);

		CloseHandle(hfile);
		DeleteFile(temp_path);

		free(u8path);
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
				PRINTERR("HttpQueryDataAvailable");
				return false;
			}
			if (avail_bytes > 0)
			{
				buffer = realloc(buffer, bytes + avail_bytes);
				DWORD actual_bytes = 0;
				WinHttpReadData(hrequest, buffer + bytes, avail_bytes, &actual_bytes);
				bytes += actual_bytes;
				printf(
				  "[netw] Read %lu from %lu bytes\n",
				  actual_bytes,
				  avail_bytes);
			}
		} while (avail_bytes > 0);

		l_netw.callbacks.completion(task->udata, buffer, bytes, (int)status_code);
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


bool
netw_get_request(
  char const *in_uri,
  char const *const in_headers[],
  void *udata)
{
	printf("[netw] get_request: %s\n", in_uri);

	struct task *task = calloc(sizeof *task, 1);
	task->udata = udata;
	task->verb = L"GET";

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
	wprintf(L"[netw] port: %i\n", task->port);
	task->host =
	  wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	wprintf(L"[netw] host: %s\n", task->host);
	task->path =
	  wcstrndup(url_components.lpszUrlPath, url_components.dwUrlPathLength);
	wprintf(L"[netw] path: %s\n", task->path);

	free(uri);

	// combine/convert headers
	char *header = combine_headers(in_headers, NULL);

	size_t headerlen = sys_wchar_from_utf8(header, NULL, 0);
	task->header = malloc(sizeof *(task->header) * headerlen);
	sys_wchar_from_utf8(header, task->header, headerlen);

	wprintf(L"[netw] headers:\n--\n%s--\n", task->header);

	free(header);

	HANDLE h = CreateThread(NULL, 0, task_handler, task, 0, NULL);
	if (h)
	{
		CloseHandle(h);
	}
	else
	{
		printf("[netw] failed to create thread\n");
	}

	return true;
}


bool
netw_post_request(
  char const *in_uri,
  char const *const in_headers[],
  void const *body,
  size_t nbody_bytes,
  void *udata)
{
	printf("[netw] post_request: %s\n", in_uri);

	struct task *task = calloc(sizeof *task, 1);
	task->udata = udata;
	task->payload = memdup(body, nbody_bytes);
	task->payload_bytes = nbody_bytes;
	task->verb = L"POST";

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
	wprintf(L"[netw] port: %i\n", task->port);
	task->host =
	  wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	wprintf(L"[netw] host: %s\n", task->host);
	task->path =
	  wcstrndup(url_components.lpszUrlPath, url_components.dwUrlPathLength);
	wprintf(L"[netw] path: %s\n", task->path);

	free(uri);

	// combine/convert headers
	char *header = combine_headers(in_headers, NULL);

	size_t headerlen = sys_wchar_from_utf8(header, NULL, 0);
	task->header = malloc(sizeof *(task->header) * headerlen);
	sys_wchar_from_utf8(header, task->header, headerlen);

	wprintf(L"[netw] headers:\n--\n%s--\n", task->header);

	free(header);

	HANDLE h = CreateThread(NULL, 0, task_handler, task, 0, NULL);
	if (h)
	{
		CloseHandle(h);
	}
	else
	{
		printf("[netw] failed to create thread\n");
	}

	return true;
}


bool
netw_download(char const *in_uri, void *udata)
{
	printf("[netw] download_request: %s\n", in_uri);

	struct task *task = calloc(sizeof *task, 1);
	task->udata = udata;
	task->verb = L"GET";
	task->is_download = true;

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
	wprintf(L"[netw] port: %i\n", task->port);
	task->host =
	  wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	wprintf(L"[netw] host: %s\n", task->host);
	task->path =
	  wcstrndup(url_components.lpszUrlPath, url_components.dwUrlPathLength);
	wprintf(L"[netw] path: %s\n", task->path);

	free(uri);

	HANDLE h = CreateThread(NULL, 0, task_handler, task, 0, NULL);
	if (h)
	{
		CloseHandle(h);
	}
	else
	{
		printf("[netw] failed to create thread\n");
	}

	return true;
}
