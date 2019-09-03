#include "minimod/minimod.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STR_IMPL_(X) #X
#define STR(X) STR_IMPL_(X)

// CONFIG
// ------
#define API_KEY_LIVE "4cb29b99f25a2f0d1ba30c5a71419e5b"

#define API_KEY_TEST "64a70c497c922857c435c1578562a118"
#define GAME_ID_TEST 347
#define MOD_ID_TEST 1889

#ifdef _WIN32
#include <Windows.h>
static void
sys_sleep(uint32_t ms)
{
	Sleep(ms);
}
#else
#include <unistd.h>
static void
sys_sleep(uint32_t ms)
{
	usleep(ms * 1000);
}
#endif


// ===================================================================
// JUST INIT+DEINIT
// -------------------------------------------------------------------
static void
test_init(void)
{
	printf("\n= Simple init()/deinit() test\n");
	minimod_init(API_KEY_TEST, NULL, 0, MINIMOD_CURRENT_ABI);

	minimod_deinit();
}


// ===================================================================
// GET GAMES
// -------------------------------------------------------------------
static void
get_all_games_callback(
  void *udata,
  size_t ngames,
  struct minimod_game const *games,
  struct minimod_pagination const *pagi)
{
	for (size_t i = 0; i < ngames; ++i)
	{
		printf("- %s {%" PRIu64 "}\n", games[i].name, games[i].id);
		printf(
		  "\t+ https://%s.mod.io\n",
		  minimod_get_more_string(games[i].more, "name_id"));
		time_t added =
		  (time_t)minimod_get_more_int(games[i].more, "date_added");
		printf("\t+ date added: %s\n", ctime(&added));
	}

	*((int *)udata) = 1;
}


static void
test_get_all_games(void)
{
	printf("\n= Requesting list of live games on mod.io\n");
	minimod_init(API_KEY_LIVE, NULL, 0, MINIMOD_CURRENT_ABI);

	int nrequests_completed = 0;
	minimod_get_games(NULL, get_all_games_callback, &nrequests_completed);

	while (nrequests_completed == 0)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// GET MODS
// -------------------------------------------------------------------
static void
on_get_all_mods(
  void *udata,
  size_t nmods,
  struct minimod_mod const *mods,
  struct minimod_pagination const *pagi)
{
	for (size_t i = 0; i < nmods; ++i)
	{
		printf("- %s {%" PRIu64 "}\n", mods[i].name, mods[i].id);
		printf("  - {%" PRIu64 "}\n", mods[i].modfile_id);
	}

	++(*(int *)udata);
}


static void
test_get_all_mods(uint64_t game_id)
{
	printf("\n= Requesting list of mods for game X on test-mod.io\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int nrequests_completed = 0;
	minimod_get_mods(
	  NULL,
	  GAME_ID_TEST,
	  0,
	  on_get_all_mods,
	  &nrequests_completed);

	while (nrequests_completed < 1)
	{
		sys_sleep(10);
	}

	minimod_deinit();

	printf(
	  "\n= Requesting list of mods for game {%" PRIu64 "} on live-mod.io\n",
	  game_id);
	minimod_init(API_KEY_LIVE, NULL, 0, MINIMOD_CURRENT_ABI);

	nrequests_completed = 0;
	minimod_get_mods(
	  NULL,
	  game_id,
	  0,
	  on_get_all_mods,
	  &nrequests_completed);

	while (nrequests_completed < 1)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// AUTHENTICATION
// -------------------------------------------------------------------
static void
on_email_request(void *in_udata, bool in_success)
{
	printf("Email request %s.\n", in_success ? "successful" : "failed");
	*((int *)in_udata) = in_success ? 1 : -1;
}


static void
on_email_exchange(void *in_udata, char const *in_token, size_t in_bytes)
{
	printf("Authentication %s.\n", in_token ? "successful" : "failed");
	*((int *)in_udata) = in_token ? 1 : -1;
}


static bool
test_authentication(void)
{
	printf("\n= Email authentication workflow\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

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
	char email[128] = { 0 };
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


// ===================================================================
// ME
// -------------------------------------------------------------------
static void
on_get_users(
  void *in_udata,
  size_t nusers,
  struct minimod_user const *users,
  struct minimod_pagination const *pagi)
{
	printf("Users: %zu\n", nusers);
	for (size_t i = 0; i < nusers; ++i)
	{
		printf("- %s {%" PRIu64 "}\n", users[i].username, users[i].id);
	}
	*((int *)in_udata) = 0;
}


static void
test_me(void)
{
	printf("\n= Get Me\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int wait = 1;
	minimod_get_me(on_get_users, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// MODFILES
// -------------------------------------------------------------------
static void
on_get_modfiles(
  void *in_udata,
  size_t nmodfiles,
  struct minimod_modfile const *modfiles,
  struct minimod_pagination const *pagi)
{
	for (size_t i = 0; i < nmodfiles; ++i)
	{
		printf(
		  "- {%" PRIu64 "} @ %s (%" PRIi64 " bytes)\n",
		  modfiles[i].id,
		  modfiles[i].url,
		  modfiles[i].filesize);
	}
	*((int *)in_udata) = 0;
}


static void
test_get_modfiles(void)
{
	printf("\n= Get Modfiles\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int wait = 1;
	minimod_get_modfiles(NULL, GAME_ID_TEST, MOD_ID_TEST, 0, on_get_modfiles, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// INSTALLATION &c.
// -------------------------------------------------------------------
static void
on_installed(
  void *in_udata,
  bool in_success,
  uint64_t in_game_id,
  uint64_t in_mod_id)
{
	printf(
	  "mod %" PRIu64 ":%" PRIu64 " installation %s\n",
	  in_game_id,
	  in_mod_id,
	  in_success ? "successful" : "failed");
	*((int *)in_udata) = 0;
}


static void
installed_mod_enumerator(
  void *in_userdata,
  uint64_t in_game_id,
  uint64_t in_mod_id,
  char const *in_path)
{
	printf("- %" PRIu64 ":%" PRIu64 " = %s\n", in_game_id, in_mod_id, in_path);
}


static void
on_installed_mod(
  void *in_userdata,
  size_t in_nmods,
  struct minimod_mod const *mods,
  struct minimod_pagination const *pagi)
{
	if (in_nmods > 0)
	{
		printf("- %" PRIu64 " %s\n", mods[0].id, mods[0].name);
	}
	*((int *)in_userdata) = 0;
}


static void
test_installation(void)
{
	printf("\n= Installing Mod\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV | MINIMOD_INITFLAG_UNZIP,
	  MINIMOD_CURRENT_ABI);

	// install the mod
	int wait = 1;
	minimod_install(GAME_ID_TEST, MOD_ID_TEST, 0, on_installed, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	// make sure the mod is installed
	bool is_installed = minimod_is_installed(GAME_ID_TEST, MOD_ID_TEST);
	printf("== Mod is installed: %s\n", is_installed ? "YES" : "NO");

	// enum all installed mods
	printf("== Installed mods:\n");
	minimod_enum_installed_mods(0, installed_mod_enumerator, NULL);

	// get data for the installed mod
	printf("== Get installed mods:\n");
	wait = 1;
	minimod_get_installed_mod(GAME_ID_TEST, MOD_ID_TEST, on_installed_mod, &wait);
	while (wait)
	{
		sys_sleep(10);
	}

	// undo stuff and deinstall the mod
	printf("== Uninstalling Mod\n");
	minimod_uninstall(GAME_ID_TEST, MOD_ID_TEST);

	minimod_deinit();
}


// ===================================================================
// RATINGS
// -------------------------------------------------------------------
static void
on_get_ratings(
  void *in_udata,
  size_t nratings,
  struct minimod_rating const *ratings,
  struct minimod_pagination const *pagi)
{
	printf("got %zu ratings\n", nratings);
	*((int *)in_udata) = (int)ratings[0].rating;
}


static void
on_rated(void *in_udata, bool in_success)
{
	printf("rating %s\n", in_success ? "succeeded" : "failed");
	*((int *)in_udata) = 0;
}


static void
test_rating(void)
{
	printf("\n= Rating\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int rating = -2;
	minimod_get_ratings(
	  "game_id=" STR(GAME_ID_TEST) "&mod_id=" STR(MOD_ID_TEST),
	  on_get_ratings,
	  &rating);
	while (rating == -2)
	{
		sys_sleep(10);
	}
	printf("mod-rating is %i\n", rating);

	int wait = 1;
	minimod_rate(GAME_ID_TEST, MOD_ID_TEST, rating == 1 ? -1 : 1, on_rated, &wait);
	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// SUBSCRIPTIONS
// -------------------------------------------------------------------
static void
on_subscriptions(
  void *udata,
  size_t nmods,
  struct minimod_mod const *mods,
  struct minimod_pagination const *pagi)
{
	printf("Subscribed mods:\n");
	for (size_t i = 0; i < nmods; ++i)
	{
		printf(
		  "- \"%s\" {%" PRIu64 "} for game {%" PRIi64 "}\n",
		  mods[i].name,
		  mods[i].id,
		  minimod_get_more_int(mods[i].more, "game_id"));
	}
	*((int *)udata) = 0;
}


static void
test_subscription(void)
{
	printf("\n= Get Subscriptions\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int wait = 1;
	minimod_get_subscriptions(NULL, on_subscriptions, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// MOD EVENTS
// -------------------------------------------------------------------
static void
on_mod_events(
  void *in_userdata,
  size_t nevents,
  struct minimod_event const *events,
  struct minimod_pagination const *pagi)
{
	for (size_t i = 0; i < nevents; ++i)
	{
		time_t t = (time_t)events[i].date_added;
		printf(
		  "- %" PRIu64 ":%" PRIu64 " et=%i %s",
		  events[i].game_id,
		  events[i].mod_id,
		  events[i].type,
		  ctime(&t));
	}
	*((int *)in_userdata) = 0;
}


static void
test_mod_events(void)
{
	printf("\n= Get all mod events for game:\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int wait = 1;
	minimod_get_mod_events(NULL, GAME_ID_TEST, 0, 0, on_mod_events, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// USER EVENTS
// -------------------------------------------------------------------
static void
on_user_events(
  void *in_userdata,
  size_t nevents,
  struct minimod_event const *events,
  struct minimod_pagination const *pagi)
{
	for (size_t i = 0; i < nevents; ++i)
	{
		time_t t = (time_t)events[i].date_added;
		printf(
		  "- %" PRIu64 ":%" PRIu64 " et=%i %s",
		  events[i].game_id,
		  events[i].mod_id,
		  events[i].type,
		  ctime(&t));
	}
	*((int *)in_userdata) = 0;
}


static void
test_user_events(void)
{
	printf("\n= Get all user events:\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int wait = 1;
	minimod_get_user_events(NULL, GAME_ID_TEST, 0, on_user_events, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


// ===================================================================
// DEPENDENCIES
// -------------------------------------------------------------------
static void
on_dependencies(
  void *in_userdata,
  size_t ndeps,
  uint64_t const *deps,
  struct minimod_pagination const *pagi)
{
	printf("Num dependencies: %zu\n", ndeps);
	for (size_t i = 0; i < ndeps; ++i)
	{
		printf("- %" PRIu64 "\n", deps[i]);
	}
	*((int *)in_userdata) = 0;
}


static void
test_dependencies(void)
{
	printf("\n= Get dependencies:\n");
	minimod_init(
	  API_KEY_TEST,
	  NULL,
	  MINIMOD_INITFLAG_TESTENV,
	  MINIMOD_CURRENT_ABI);

	int wait = 1;
	minimod_get_dependencies(GAME_ID_TEST, MOD_ID_TEST, on_dependencies, &wait);

	while (wait)
	{
		sys_sleep(10);
	}

	minimod_deinit();
}


int
main(void)
{
	printf("[test] Starting\n");

	test_init();
	test_get_all_games();
	test_get_all_mods(1);
	test_authentication();
	test_me();
	test_get_modfiles();
	test_installation();
	test_rating();
	test_subscription();
	test_mod_events();
	test_user_events();
	test_dependencies();

	printf("[test] Done\n");

	return 0;
}
