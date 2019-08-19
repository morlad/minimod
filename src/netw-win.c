// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#include "netw.h"
#include "util.h"

#include <stdio.h>
#include <windows.h>
#include <winhttp.h>


struct netw
{
	HINTERNET session;
	struct netw_callbacks callbacks;
};
static struct netw l_netw;


static void
on_status_callback(
	HINTERNET in_request,
	DWORD_PTR in_context,
	DWORD in_status,
	LPVOID in_statusinfo,
	DWORD in_statusinfo_bytes
)
{
	printf("[netw] callback: 0x%lx %llu\n", in_status, in_context);
	if (in_status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE)
	{
		printf("[netw] request complete -> requesting response\n");
		BOOL res = WinHttpReceiveResponse(in_request, NULL);
		if (!res)
		{
			printf("[netw] receive response failed: %lu\n", GetLastError());
		}
		DWORD bytes = 0;
		BOOL r = WinHttpQueryDataAvailable(in_request, &bytes);
		if (r)
		{
#if 0
			printf("[netw] bytes available: %lu\n", bytes);
			if (bytes > 0)
			{
				void *buffer = malloc(bytes);
				DWORD nread = 0;
				WinHttpReadData(in_request, buffer, bytes, &nread);
				printf("[netw] bytes read: %lu\n", nread);
				l_netw.callbacks.completion(in_context, buffer, nread, 200);
				free(buffer);
			}
#endif
		}
		else
		{
			printf("[netw] query data failed: %lu\n", GetLastError());
		}
	}
	if (in_status == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)
	{
		DWORD *avail = in_statusinfo;
		DWORD bytes = *avail;
		printf("[netw] bytes available: %lu\n", bytes);
		if (bytes > 0)
		{
			void *buffer = malloc(bytes);
			DWORD nread = 0;
			WinHttpReadData(in_request, buffer, bytes, &nread);
			printf("[netw] bytes read: %lu\n", nread);
			l_netw.callbacks.completion(in_context, buffer, nread, 200);
			free(buffer);
		}
	}
	if (in_status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
	{
		printf("[netw] READ-COMPLETE: %lu bytes\n", in_statusinfo_bytes);
	}
}


bool
netw_init(struct netw_callbacks *in_callbacks)
{
	l_netw.callbacks = *in_callbacks;

	l_netw.session = WinHttpOpen(
		L"minimod-client",

// TODO if windows < 8.1
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
// TODO if windows >= 8.1
		//WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,

		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,

		0);

	if (!l_netw.session)
	{
		printf("[netw] HttpOpen failed %lu (%lx)\n", GetLastError(), GetLastError());
		return false;
	}

	return true;
}


void
netw_deinit(void)
{
	WinHttpCloseHandle(l_netw.session);
}


static char *combine_headers(char const *const in_headers[], size_t *out_len)
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

	if (out_len)
	{
		*out_len = len_headers;
	}

	return headers;
}


static wchar_t*
wcstrndup(wchar_t const *in, size_t in_len)
{
	wchar_t *ptr = malloc(sizeof *ptr * (in_len + 1));
	ptr[in_len] = '\0';
	memcpy(ptr, in, sizeof *ptr * in_len);
	return ptr;
}


uint64_t
netw_get_request(char const *in_uri, char const *const in_headers[])
{
	size_t urilen = sys_wchar_from_utf8(in_uri, NULL, 0);
	wchar_t *uri = malloc(sizeof *uri * urilen);
	sys_wchar_from_utf8(in_uri, uri, urilen);

	URL_COMPONENTS url_components = {
		.dwStructSize = sizeof url_components,
		.dwHostNameLength = (DWORD)-1,
		.dwUrlPathLength = (DWORD)-1,
		.dwExtraInfoLength = (DWORD)-1,
	};
	WinHttpCrackUrl(uri, (DWORD)urilen, 0, &url_components);

	printf("[netw] get_request: %s\n", in_uri);
	wprintf(L"[netw] host: %s, %lu\n", url_components.lpszHostName, url_components.dwHostNameLength);
	wprintf(L"[netw] port: %i\n", url_components.nPort);
	wprintf(L"[netw] urlpath: %s\n", url_components.lpszUrlPath);
	wprintf(L"[netw] extra: %s\n", url_components.lpszExtraInfo);

	wchar_t *hostname = wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	wprintf(L"[netw] clean-host: %s\n", hostname);

	HINTERNET hconnect = WinHttpConnect(
		l_netw.session,
		hostname,
		url_components.nPort,
		0);

	free(hostname);

	if (!hconnect)
	{
		printf("[netw] err: httpconnect: 0x%lx (%lu)\n", GetLastError(), GetLastError());
	}

	HINTERNET hrequest = WinHttpOpenRequest(
		hconnect,
		L"GET",
		url_components.lpszUrlPath,
		NULL,
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		WINHTTP_FLAG_SECURE);

	if (!hrequest)
	{
		printf("[netw] err: httpopenrequest: 0x%lx (%lu)\n", GetLastError(), GetLastError());
	}

	char *header = combine_headers(in_headers, NULL);

	size_t headerlen = sys_wchar_from_utf8(header, NULL, 0);
	wchar_t *header_wc = malloc(sizeof *header_wc * headerlen);
	sys_wchar_from_utf8(header, header_wc, headerlen);
	free(header);

	wprintf(L"[netw] headers: --\n%s-- (%zu)\n", header_wc, headerlen);

	BOOL success = WinHttpSendRequest(
		hrequest,
		header_wc,
		(DWORD)headerlen - 1,
		WINHTTP_NO_REQUEST_DATA,
		0,
		0,
		1);

	if (!success)
	{
		printf("[netw] err: httpsendrequest: 0x%lx (%lu)\n", GetLastError(), GetLastError());
	}
	
	free(header_wc);

	return 1;
}


uint64_t
netw_post_request(
	char const *uri,
	char const *const in_headers[],
	void const *body,
	size_t nbody_bytes)
{
#if 0
	DWORD_PTR context = 2;
	HINTERNET session = InternetConnect(
		l_netw.session,
		server_name,
		server_port,
		NULL,
		NULL,
		INTERNET_SERVICE_HTTP,
		0,
		context);

	HINTERNET request = HttpOpenRequestA(
		session,
		"POST",
		uri,
		NULL, // version (NULL auto selects)
		NULL, // referrer
		NULL, // accepted types
		0,
		context);

	size_t len_headers = 0;
	char const *const *h = in_headers;
	while (*h)
	{
		len_headers += strlen(*(h++));
		len_headers += 2; /* ": " or "\r\n" */
	}
	char *headers = malloc(len_headers);

	HttpSendRequestA(
		request,
		headers,
		(DWORD)len_headers,
		body,
		(DWORD)nbody_bytes);

	free(headers);

#endif
	return 0;
}


uint64_t
netw_download(char const *in_uri)
{
#if 0
	DWORD_PTR context = 3;
	HINTERNET task = InternetOpenUrlA(
		l_netw.session,
		in_uri,
		NULL,
		0,
		INTERNET_FLAG_NEED_FILE,
		context);

#endif
	return 0;
}
