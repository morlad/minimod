// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#include "netw.h"
#import <Foundation/Foundation.h>


@interface MyDelegate : NSObject<NSURLSessionDelegate,
	NSURLSessionTaskDelegate,
	NSURLSessionDataDelegate>
@end


struct netw
{
	NSURLSession *session;
	MyDelegate *delegate;
	struct netw_callbacks callbacks;
	NSMutableDictionary *dict;
};
static struct netw l_netw;


@implementation MyDelegate
- (void)URLSession:(NSURLSession *)session
	task:(NSURLSessionTask *)task
	didCompleteWithError:(NSError *)error
{
	assert(task.state == NSURLSessionTaskStateCompleted);
	NSData *data = l_netw.dict[task];
	l_netw.callbacks.completion(task.taskIdentifier, data.bytes, data.length, (int)((NSHTTPURLResponse*)task.response).statusCode);
	[l_netw.dict removeObjectForKey:task];
}

- (void)URLSession:(NSURLSession *)session
	dataTask:(NSURLSessionDataTask *)task
	didReceiveData:(NSData *)in_data
{
	if (!l_netw.dict[task])
	{
	  l_netw.dict[task] = [NSMutableData new];
	}
	NSMutableData *data = l_netw.dict[task];
	[data appendData:in_data];
}

- (void)URLSession:(NSURLSession *)session
	downloadTask:(NSURLSessionDownloadTask *)task
	didFinishDownloadingToURL:(NSURL *)location
{
	l_netw.callbacks.downloaded(task.taskIdentifier, location.path.UTF8String, (int)((NSHTTPURLResponse*)task.response).statusCode);
}
@end


bool
netw_init(struct netw_callbacks *in_callbacks)
{
	l_netw.callbacks = *in_callbacks;

	l_netw.delegate = [MyDelegate new];

	l_netw.dict = [NSMutableDictionary dictionaryWithCapacity:8];

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
	l_netw.dict = nil;
}


uint64_t
netw_get_request(
	char const *in_uri,
	char const *const headers[])
{
	assert(in_uri);
	assert(headers);

	NSString *uri = [NSString stringWithUTF8String:in_uri];
	NSURL *url = [NSURL URLWithString:uri];

	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
	for (size_t i = 0; headers[i]; i += 2)
	{
		NSString *field = [NSString stringWithUTF8String:headers[i]];
		NSString *value = [NSString stringWithUTF8String:headers[i+1]];
		[request setValue:value forHTTPHeaderField:field];
	}

	NSURLSessionDataTask *task = [l_netw.session dataTaskWithRequest:request];

	[task resume];

	return task.taskIdentifier;
}


uint64_t
netw_post_request(
	char const *in_uri,
	char const *const headers[],
	void const *in_body,
	size_t in_nbytes)
{
	assert(in_uri);
	assert(headers);

	NSString *uri = [NSString stringWithUTF8String:in_uri];
	NSURL *url = [NSURL URLWithString:uri];

	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];

	request.HTTPMethod = @"POST";

	for (size_t i = 0; headers[i]; i += 2)
	{
		NSString *field = [NSString stringWithUTF8String:headers[i]];
		NSString *value = [NSString stringWithUTF8String:headers[i+1]];
		[request setValue:value forHTTPHeaderField:field];
	}

	NSData *body = [NSData dataWithBytes:in_body length:in_nbytes];

	NSURLSessionDataTask *task = [l_netw.session uploadTaskWithRequest:request fromData:body];

	[task resume];

	return task.taskIdentifier;
}


uint64_t
netw_download(char const *in_uri)
{
	assert(in_uri);

	printf("[netw] download(%s)\n", in_uri);

	NSString *uri = [NSString stringWithUTF8String:in_uri];
	NSURL *url = [NSURL URLWithString:uri];

	NSURLSessionDownloadTask *task = [l_netw.session downloadTaskWithURL:url];

	[task resume];

	return task.taskIdentifier;
}
