// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "netw.h"
#import <Foundation/Foundation.h>

#define UNUSED(X) __attribute__((unused)) X

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
	int error_rate;
	int min_delay;
	int max_delay;
	char _padding[4];
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


@implementation MyDelegate
- (void)URLSession:(NSURLSession *)UNUSED(session)
                  task:(NSURLSessionTask *)nstask
  didCompleteWithError:(NSError *)error
{
	ASSERT(nstask.state == NSURLSessionTaskStateCompleted);

	struct task *task = task_from_dictionary(l_netw.task_dict, nstask);

	random_delay();

	NSHTTPURLResponse *response = (NSHTTPURLResponse *)nstask.response;
	// downloads have no buffer object
	if (task->buffer)
	{
		ASSERT(!task->file);
		task->callback.request(
		  task->userdata,
		  task->buffer.bytes,
		  task->buffer.length,
		  (int)response.statusCode,
		  (struct netw_header const *)response.allHeaderFields);
		task->buffer = nil;
	}
	else
	{
		ASSERT(task->file);
		task->callback.download(
		  task->userdata,
		  task->file,
		  (int)response.statusCode,
		  (struct netw_header const *)response.allHeaderFields);
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
		ASSERT(!task->buffer);
		fwrite(in_data.bytes, in_data.length, 1, task->file);
	}
	else
	{
		ASSERT(task->buffer);
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
	sranddev();
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
	if (l_netw.error_rate > 0 && is_random_server_error())
	{
		LOG("Failing request: %s", in_uri);
		if (fout)
		{
			in_callback.download(in_userdata, fout, 500, NULL);
		}
		else
		{
			in_callback.request(in_userdata, NULL, 0, 500, NULL);
		}
		return true;
	}
	ASSERT(in_uri);
	LOG("Sending request: %s", in_uri);

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


char const *
netw_get_header(struct netw_header const *in_header, char const *name)
{
	NSDictionary *header = (NSDictionary *)in_header;
	NSString *key = [NSString stringWithUTF8String:name];
	NSString *val = header[key];
	return val ? [val UTF8String] : NULL;
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
