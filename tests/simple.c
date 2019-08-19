// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#include "minimod/minimod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>


#ifdef _WIN32
#include <Windows.h>
void
static sys_sleep(uint32_t ms)
{
	Sleep(ms);
}
#else
#include <unistd.h>
void
static sys_sleep(uint32_t ms)
{
	usleep(ms * 1000);
}
#endif

// just init
// ---------
static void
test_1(void)
{
	printf("\n= Simple init()/deinit() test\n");
	minimod_init(
		MINIMOD_ENVIRONMENT_TEST,
		309,
		"f90f25ceed3708627a5b85ee52e4f930",
		NULL);

	minimod_deinit();
}


// get all games
// -------------
static void
get_games_callback(
	void *udata,
	size_t ngames,
	struct minimod_game const *games)
{
	for (size_t i = 0; i < ngames; ++i)
	{
		printf("- %s {%llu}\n", games[i].name, games[i].id);
		printf("\t+ https://%s.mod.io\n", minimod_get_more_string(games[i].more, "name_id"));
		time_t added = (time_t)minimod_get_more_int(games[i].more, "date_added");
		printf("\t+ date added: %s\n", ctime(&added));
	}

	++(*(int *)udata);
}


static void
test_2(void)
{
	minimod_init(
		MINIMOD_ENVIRONMENT_LIVE,
		0,
		"4cb29b99f25a2f0d1ba30c5a71419e5b",
		NULL);

	printf("\n= Requesting list of live games on mod.io\n");

	int nrequests_completed = 0;
	minimod_get_games(NULL, get_games_callback, &nrequests_completed);

	while (nrequests_completed == 0)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// get all mods
static void
get_mods_callback(
	void *udata,
	size_t nmods,
	struct minimod_mod const *mods)
{
	for (size_t i = 0; i < nmods; ++i)
	{
		printf("- %s {%llu}\n", mods[i].name, mods[i].id);
		printf("  - ? {%llu}\n", mods[i].modfile_id);
	}

	++(*(int *)udata);
}


static void
test_3(uint64_t game_id)
{
#if 0
	minimod_init(
		MINIMOD_ENVIRONMENT_TEST,
		309,
		"f90f25ceed3708627a5b85ee52e4f930",
		NULL);

	printf("\n= Requesting list of mods for game X on test-mod.io\n");

	int nrequests_completed = 0;
	minimod_get_mods(NULL, 0, get_mods_callback, &nrequests_completed);

	while (nrequests_completed < 1)
	{
		sys_sleep(10);
	}

	minimod_deinit();
#endif


	minimod_init(
		MINIMOD_ENVIRONMENT_LIVE,
		0,
		"4cb29b99f25a2f0d1ba30c5a71419e5b",
		NULL);

	printf("\n= Requesting list of mods for game {%llu} on live-mod.io\n", game_id);

	int nrequests_completed = 0;
	minimod_get_mods(NULL, game_id, get_mods_callback, &nrequests_completed);

	while (nrequests_completed < 1)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// authentication
// --------------
static void
on_email_request(void *in_udata, bool in_success)
{
	*((int *)in_udata) = in_success ? 1 : -1;
	printf("Email request %s.\n", in_success ? "successful" : "failed");
}


static void
on_email_exchange(void *in_udata, char const *in_token, size_t in_bytes)
{
	*((int *)in_udata) = in_token ? 1 : -1;
	printf("Authentication %s.\n", in_token ? "successful" : "failed");
}


static bool
test_4(void)
{
	minimod_init(
		MINIMOD_ENVIRONMENT_TEST,
		309,
		"f90f25ceed3708627a5b85ee52e4f930",
		NULL);

	printf("\n= Email authentication workflow\n");

	if (minimod_is_authenticated())
	{
		printf("You are already logged in. Log out and proceed? [y/n] ");
		if (fgetc(stdin) != 'y')
		{
			return true;
		}
		minimod_deauthenticate();
	}

	// get e-mail address
	printf("Enter email: ");
	char email[128] = {0};
	fgets(email, sizeof email, stdin);
	size_t len = strlen(email);

	// mail address needs to be at least a@b.cc
	// and truncate accordingly
	if (len > 6)
	{
		for (size_t i = 0; i < len; ++i)
		{
			if (isspace(email[i]))
			{
				email[i] = '\0';
				break;
			}
		}
		printf("Sending email to '%s'...\n", email);
	}
	else
	{
		printf("Invalid email-address.\n");
		return false;
	}

	int success = 0;
	minimod_email_request(email, on_email_request, &success);
	
	while (success == 0)
	{
		sys_sleep(10);
	}

	if (success == 1)
	{
		// get 5 character security code
		printf("Enter security code received by email: ");
		char code[6];
		if (!fgets(code, sizeof code, stdin))
		{
			return false;
		}

		for (size_t i = 0; i < sizeof code; ++i)
		{
			code[i] = isalnum(code[i]) ? (char)toupper(code[i]) : '\0';
		}

		success = 0;
		printf("Verifying security code\n");
		minimod_email_exchange(code, on_email_exchange, &success);

		while (success == 0)
		{
			sys_sleep(10);
		}
	}

	minimod_deinit();

	return true;
}


// me
// --
static void
on_get_users(void *in_udata, size_t nusers, struct minimod_user const *users)
{
	*((int *)in_udata) = 0;
	printf("Users: %zu\n", nusers);
	for (size_t i = 0; i < nusers; ++i)
	{
		printf("- %s {%llu}\n", users[i].name, users[i].id);
	}
}


static void
test_5(void)
{
	minimod_init(
		MINIMOD_ENVIRONMENT_TEST,
		309,
		"f90f25ceed3708627a5b85ee52e4f930",
		NULL);

	int wait = 1;
	minimod_get_user(0, on_get_users, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// modfiles
// --------
static void
on_get_modfiles(void *in_udata, size_t nmodfiles, struct minimod_modfile const *modfiles)
{
	for (size_t i = 0; i < nmodfiles; ++i)
	{
		printf("- {%llu} @ %s (%lli bytes)\n", modfiles[i].id, modfiles[i].url, modfiles[i].filesize);
	}
	*((int *)in_udata) = 0;
}


static void
test_6(void)
{
	minimod_init(
		MINIMOD_ENVIRONMENT_TEST,
		309,
		"f90f25ceed3708627a5b85ee52e4f930",
		NULL);

	int wait = 1;
	minimod_get_modfiles(NULL, 0, 1720, 0, on_get_modfiles, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


static void
on_downloaded(void *in_udata, char const *in_path)
{
	printf("mod downloaded to: %s\n", in_path);
	*((int *)in_udata) = 0;
}


static void
test_7(void)
{
	minimod_init(
		MINIMOD_ENVIRONMENT_TEST,
		309,
		"f90f25ceed3708627a5b85ee52e4f930",
		NULL);

	int wait = 1;
	minimod_download(0, 1720, 1685, on_downloaded, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


static void
on_installed(void *in_udata, char const *in_path)
{
	printf("mod installed to: %s\n", in_path);
	*((int *)in_udata) = 0;
}


static void
test_8(void)
{
	minimod_init(
		MINIMOD_ENVIRONMENT_TEST,
		309,
		"f90f25ceed3708627a5b85ee52e4f930",
		NULL);

	int wait = 1;
	minimod_install(0, 1720, 1685, on_installed, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


int
main(int argc, char const *argv[])
{
	printf("[test] Starting\n");

	test_1();
	test_2();
	test_3(1);
	test_4();
	test_5();
	test_6();
	test_7();
	test_8();

	printf("[test] Done\n");

	return 0;
}
