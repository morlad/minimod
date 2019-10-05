# minimod
A lightweight C library to wrap the https://mod.io API.

It supports the '*consuming*' part of the API only, it does not support uploading new mods (and related tasks).

- Works on Windows, macOS, Linux and FreeBSD
- No dependencies on non-system libraries during runtime, other than libcurl under Linux and FreeBSD.
- Small filesize of the library (~200 KiB for macOS)
- Allows installed mods to remain in their ZIP form or unzip them
- Includes functionality to simulate high latency connections and server failures
- API documentation in [`/docs`](https://morlad.github.io/minimod) folder
- Many examples in `/tests/examples.c`
- (GNU)make based build on all platforms, using GCC under Linux and clang everywhere else

## Example: Print all games currently on mod.io
```c
#include "minimod/minimod.h"
#include <stdio.h>

void get_games_callback(void *udata, size_t ngames, struct minimod_game const *games, struct minimod_pagination const *pagination)
{
	for (size_t i = 0; i < ngames; ++i)
	{
		printf("- %s {%llu}\n", games[i].name, games[i].id);
	}

	// signal waiting loop in main() that we are done.
	int *nrequests_completed = udata;
	*nrequests_completed += 1;
}

int main(void)
{
	minimod_init(
		MODIO_API_KEY, // your API key
		".modio", // local path for mods + data
		0, // flags like: use test-environment, unzip mods locally, ...
		MINIMOD_CURRENT_ABI); // used to detect incompatible ABIs

	printf("List of games on mod.io\n");

	int nrequests_completed = 0;
	minimod_get_games(NULL, get_games_callback, &nrequests_completed);

	// wait for the callback to finish
	while (nrequests_completed == 0) { /* sleep */ }

	minimod_deinit();
}
```

## Design

### Mega-structs vs. 'more'
Instead of having every minimod-struct map all of the (currently) available
data of the underlying JSON object, they only contain a subset of it.

However, the structs include a *more* field, which can be used together
with the `minimod_get_more_(string|int|bool|float)`-functions to access more fields.

This way neither memory nor time is spent on extracting/converting
fields of the underlying JSON object, which may not even be required
by the calling code.
But they are still accessible when the need arises.

This has *another advantage*: if the underlying API adds new fields
there is no need to update minimod, nor wait for minimod to be updated
but the API's new features can be exploited immediately.

### Caching
minimod does no caching of server responses internally. This would increase
the complexity of the code as well as introduce performance penalties
because of more memory allocations, processing overhead &c.
After all, minimod knows nothing about how often a query will happen,
and which data of the response is actually used by the client app.

So it is up to the client code to handle the caching, if, what and when it
is the right thing to do.

### Filtering: minimod vs. API
Most minimod functions take a *filter*-string, which is passed through to
the API call unaltered. There are a few shortcuts however, so that the client
does not have to construct a string solely for the most used queries.

i.e. `minimod_get_mods(filter, game_id, mod_id, ..)` also has an
argument *mod_id*, which is strictly not necessary as *filter* could be
set to `id=<mod_id>`. But constructing strings can be a nuisance sometimes
so `mod_id` is a regular parameter as well.

A similar approach is taken in other functions, like `minimod_get_mod_events`:
`game_id`, `mod_id`, `date_cutoff`.

### Low on dependencies
On **Windows** minimod only uses system libraries (*kernel32.dll* and *winhttp.dll*)
and links the C runtime statically, thus it is not necessary to bundle/install
a specific version of the msvcrt with your product. Just copy and use the DLL.

minimod on **macOS** only uses system libraries and frameworks, so it is just
about copying the *dylib* along with your project to use it.

The **Linux** and **FreeBSD** builds of minimod require [libcurl](https://curl.haxx.se/libcurl/) to be available,
since there is no system library to make HTTPS requests.

#### Source Dependencies
minimod depends on 2 other MIT licensed libraries when compiling:
- [richgel999/miniz](https://github.com/richgel999/miniz) to handle compression and ZIP files.
	Its amalgamated source is included in this repo.
- [DeHecht/qajson4c](https://github.com/morlad/qajson4c) to parse JSON.
	Its repository is cloned automatically during the build process.
- [morlad/netw](https://github.com/morlad/netw) for cross-platform HTTPS connectivity.
	Its repository is cloned automatically during the build process.

### Unzip or Not
Depending on how an application integrates mods, it is either possible
to use them directly as ZIP file or they need to be unpacked.
minimod supports both by selecting the modus operandi during initialisation
by setting `minimod_init()`'s `MINIMOD_INITFLAG_UNZIP` flag.

### Testing & Debugging
minimod includes the awkwardly named function `minimod_set_debugtesting()`,
which instructs minimod to introduce random delays in its responses to
simulate a bad network connection, i.e. latency.

Further more it can be used to set a rate for simulating internal server
errors (server responding with HTTP status code 500), to test how the
client code copes with those.
