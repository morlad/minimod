# minimod
A lightweight C library to wrap the https://mod.io API.

- No dependencies on other libraries during runtime other than the system libraries and libcurl under Linux.
- Small filesize of the library

## Examples
### Print all games currently on mod.io
```c
#include "minimod/minimod.h"

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
