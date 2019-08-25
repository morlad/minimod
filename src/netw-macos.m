// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "netw.h"
#import <Foundation/Foundation.h>

#define UNUSED(X) __attribute__((unused)) X


@interface MyDelegate : NSObject <
                          NSURLSessionDelegate,
                          NSURLSessionTaskDelegate,
                          NSURLSessionDataDelegate>
@end


struct netw
{
	NSURLSession *session;
	MyDelegate *delegate;
	CFMutableDictionaryRef task_dict;
};
static struct netw l_netw;


union netw_callback
{
	netw_request_callback request;
	netw_download_callback download;
};


struct task
{
	union netw_callback callback;
	void *userdata;
	NSMutableData *buffer;
	FILE *file;
};


static struct task *
task_from_dictionary(CFDictionaryRef in_dict, NSURLSessionTask *in_task)
{
	union
	{
		void *nc;
		void const *c;
	} cnc;
	cnc.c = CFDictionaryGetValue(in_dict, in_task);
	return cnc.nc;
}


@implementation MyDelegate
- (void)URLSession:(NSURLSession *)UNUSED(session)
                  task:(NSURLSessionTask *)nstask
  didCompleteWithError:(NSError *)error
{
	assert(nstask.state == NSURLSessionTaskStateCompleted);

	struct task *task = task_from_dictionary(l_netw.task_dict, nstask);

	// downloads have no buffer object
	if (task->buffer)
	{
		assert(!task->file);
		task->callback.request(
		  task->userdata,
		  task->buffer.bytes,
		  task->buffer.length,
		  (int)((NSHTTPURLResponse *)nstask.response).statusCode);
		task->buffer = nil;
	}
	else
	{
		assert(task->file);
		task->callback.download(
		  task->userdata,
		  task->file,
		  (int)((NSHTTPURLResponse *)nstask.response).statusCode);
	}

	// clean up
	CFDictionaryRemoveValue(l_netw.task_dict, nstask);
	free(task);
}


- (void)URLSession:(NSURLSession *)UNUSED(session)
          dataTask:(NSURLSessionDataTask *)nstask
    didReceiveData:(NSData *)in_data
{
	struct task *task = task_from_dictionary(l_netw.task_dict, nstask);
	if (task->file)
	{
		assert(!task->buffer);
		fwrite(in_data.bytes, in_data.length, 1, task->file);
	}
	else
	{
		assert(task->buffer);
		[task->buffer appendData:in_data];
	}
}
@end


bool
netw_init(void)
{
	l_netw.delegate = [MyDelegate new];

	l_netw.task_dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

	NSURLSessionConfiguration *config =
	  [NSURLSessionConfiguration defaultSessionConfiguration];
	l_netw.session = [NSURLSession sessionWithConfiguration:config
	                                               delegate:l_netw.delegate
	                                          delegateQueue:nil];
	return true;
}


void
netw_deinit(void)
{
	[l_netw.session invalidateAndCancel];
	l_netw.session = nil;
	l_netw.delegate = nil;
	l_netw.task_dict = nil;
}


static NSString *const l_verbs[] = { @"GET", @"POST", @"PUT", @"DELETE" };


static bool
netw_request_generic(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const headers[],
  void const *in_body,
  size_t in_nbytes,
  FILE *fout,
  union netw_callback in_callback,
  void *in_userdata)
{
	assert(in_uri);

	NSString *uri = [NSString stringWithUTF8String:in_uri];
	NSURL *url = [NSURL URLWithString:uri];

	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];

	request.HTTPMethod = l_verbs[in_verb];

	if (headers)
	{
		for (size_t i = 0; headers[i]; i += 2)
		{
			NSString *field = [NSString stringWithUTF8String:headers[i]];
			NSString *value = [NSString stringWithUTF8String:headers[i + 1]];
			[request setValue:value forHTTPHeaderField:field];
		}
	}

	NSURLSessionDataTask *nstask;
	if (in_body)
	{
		NSData *body = [NSData dataWithBytes:in_body length:in_nbytes];
		nstask = [l_netw.session uploadTaskWithRequest:request fromData:body];
	}
	else
	{
		nstask = [l_netw.session dataTaskWithRequest:request];
	}

	struct task *task = calloc(1, sizeof *task);
	task->userdata = in_userdata;
	task->callback = in_callback;
	task->file = fout;
	if (!task->file)
	{
		task->buffer = [NSMutableData new];
	}

	CFDictionarySetValue(l_netw.task_dict, nstask, task);

	[nstask resume];

	return true;
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
	return netw_request_generic(
		in_verb,
		in_uri,
		headers,
		in_body,
		in_nbytes,
		NULL,
		(union netw_callback){ .request = in_callback },
		in_userdata);
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
	return netw_request_generic(
		in_verb,
		in_uri,
		headers,
		in_body,
		in_nbytes,
		fout,
		(union netw_callback){ .download = in_callback },
		in_userdata);
}
