# minimod
A lightweight C library to wrap the https://mod.io API.

- No dependencies on other libraries during runtime other than the system libraries and libcurl under Linux and FreeBSD.
- Small filesize of the library
- Works on Windows, MacOS, Linux and FreeBSD

## Examples

### Print all games currently on mod.io
```c
#include "minimod/minimod.h"
#include <stdio.h>

void get_games_callback(void *udata, size_t ngames, struct minimod_game const *games)
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
		MINIMOD_ENVIRONMENT_LIVE, // choose mod.io's live or test environment
		0, // your game's id on mod.io
		"82394a823b08dc09283fe203a", // your API key
		".modio"); // local path for mods + data

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

minimod-structs only contain a subset of the underlying JSON
objects.

However, the structs include a *more* field, which can be used together
with the *minimod_get_more*-functions to access more fields.
- `minimod_get_more_string()`
- `minimod_get_more_int()`
- `minimod_get_more_bool()`
- `minimod_get_more_float()`

This way neither memory nor time is spent on extracting/converting
fields of the underlying JSON object, which may not even be required
by the calling code.
But they are still accessible when the need arises.

This has *another advantage*: if the underlying API adds new fields
there is no need to update minimod, nor wait for minimod to be updated
but the API's new features can be exploited immediately.

### Caching
minimod does no caching of server responses internally. This would increase
the complexity of the code as well as run-time performance because of more
memory allocations. After all, minimod knows nothing about how often a
query will happen, and which data of the response is actually used by the
client app.

So it is up to the client code to handle the caching, if, what and when it
is the right thing to do. No unnecessary memory overhead or complexity on
the side of minimod.

### Filtering: minimod vs. API
Most minimod functions take a *filter*-string, which is passed through to
the API call unaltered. There are a few short-cuts however, so that the client
does not have to construct a string for the most used queries.

i.e. `minimod_get_mods(filter, game_id, mod_id, ..)` also has an
argument *mod_id*, which is strictly not necessary as *filter* could be
set to `id=<game_id>`. But constructing strings can be a nuisance sometimes
so `mod_id` is a regular parameter as well.

A similar approach is taken in other functions, like `minimod_get_mod_events`:
`game_id`, `mod_id`, `date_cutoff`.

### Low on dependencies
On Windows minimod only uses system libraries (*kernel32.dll* and *winhttp.dll*)
and links the C runtime statically, thus it is not necessary to bundle/install
a specific version of the msvcrt with your product. Just copy and use the DLL.

minimod on macOS only uses system libraries and frameworks, so it is just
about copying the *dylib* with your project and done.

The Linux and FreeBSD builds of minimod require (libcurl)[https://curl.haxx.se/libcurl/] to be available,
since there is no inherent system library to make HTTPS requests.


### Unzip or Not
Depending on how an application integrates mods, it is either possible
to use them directly as ZIP file or they need to be unpacked to a directory.
minimod supports both by selecting the modus operandi during initialisation
via `minimod_init()` by settings its parameter *unzip* to either true or false.

