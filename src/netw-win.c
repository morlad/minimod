// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#include "netw.h"

#include <wininet.h>


struct netw
{
	HINTERNET session;
	struct netw_callbacks callbacks;
};
static struct netw l_netw;


void on_status_callback(
  HINTERNET in_internet,
  DWORD_PTR in_context,
  DWORD in_status,
  LPVOID in_statusinfo,
  DWORD in_statusinfo_bytes
)
{
	if (in_status == INTERNET_STATUS_REQUEST_COMPLETE)
	{
		INTERNET_ASYNC_RESULT *result = lpvStatusInformation;
	}
}


bool
netw_init(struct netw_callbacks *in_callbacks)
{
	l_netw.callbacks = *in_callbacks;

	l_netw.session = InternetOpen(
		L"minimod-client",
		INTERNET_OPEN_TYPE_PRECONFIG,
		NULL,
		NULL,
		INTERNET_FLAG_ASYNC);

	if (!l_netw.session)
	{
		return false;
	}

	InternetSetStatusCallback(l_netw.session, on_status_callback);

	return true;
}


void
netw_deinit(void)
{
	InternetCloseHandle(l_netw.session);
}


uint64_t
netw_get_request(char const *uri, char const *const headers[])
{
	HINTERNET task = InternetOpenURL(
		l_netw.session,
		in_uri,
		NULL,
		0,
		INTERNET_FLAG_NEED_FILE,
		context);
	return 0;
}


uint64_t
netw_post_request(
	char const *uri,
	char const *const headers[],
	void const *body,
	size_t nbody_bytes)
{
	HINTERNET session = InternetConnect(
		l_netw.session,
		server_name,
		server_port,
		NULL,
		NULL,
		INTERNET_SERVICE_HTTP,
		0,
		context);

	HINTERNET request = HttpOpenRequest(
		session,
		"POST",
		uri,
		NULL,
		NULL,
		0,
		context);

	HttpSendRequest(
		request,
		headers,
		headers_length,
		body,
		nbody_bytes);

	return 0;
}


uint64_t
netw_download(char const *in_uri)
{
	HINTERNET task = InternetOpenURL(
		l_netw.session,
		in_uri,
		NULL,
		0,
		INTERNET_FLAG_NEED_FILE,
		context);

	return 0;
}
