// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#if defined(_WIN32)
#	if defined(MINIMOD_BUILD_LIB)
#		define MINIMOD_LIB __declspec(dllexport)
#	else
#		define MINIMOD_LIB __declspec(dllimport)
#	endif
#else
#	if defined(MINIMOD_BUILD_LIB)
#		define MINIMOD_LIB __attribute__((visibility("default")))
#	else
#		define MINIMOD_LIB
#	endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


enum minimod_environment
{
	MINIMOD_ENVIRONMENT_LIVE = 0,
	MINIMOD_ENVIRONMENT_TEST = 1
};


MINIMOD_LIB bool
minimod_init(
  enum minimod_environment env,
  uint32_t game_id,
  char const *api_key,
  char const *root_path);


MINIMOD_LIB void
minimod_deinit(void);


struct minimod_game
{
	uint64_t id;
	char const *name;
	void const *more;
};


typedef void (*minimod_get_games_fptr)(
  void *userdata,
  size_t ngames,
  struct minimod_game const *games);


//	Function: minimod_get_games
//
//	Retrieve all available games on the mod.io environment selected
//	during minimod_init().
//
//	Parameters:
//		filter - can be NULL, otherwise the exact filter string
//			as laid out in the api docs.
MINIMOD_LIB void
minimod_get_games(
  char const *filter,
  minimod_get_games_fptr callback,
  void *udata);


struct minimod_mod
{
	uint64_t id;
	char const *name;
	uint64_t modfile_id;
	void const *more;
};


typedef void (*minimod_get_mods_fptr)(
  void *userdata,
  size_t nmods,
  struct minimod_mod const *mods);


// Function: minimod_get_mods
//
// Retrieve a list of mods (filtered by 'filter') for 'gameid'.
// When the data is received from the server callback is called.
//
//	Parameters:
//		filter - can be NULL, otherwise the exact filter string
//			as laid out in the api docs.
//		gameid - either specify a game-id or 0 to use the game-id
//			set during minimod_init().
//
MINIMOD_LIB void
minimod_get_mods(
  char const *filter,
  uint64_t gameid,
  minimod_get_mods_fptr callback,
  void *udata);


typedef void (*minimod_email_request_fptr)(void *userdata, bool success);


MINIMOD_LIB void
minimod_email_request(
  char const *in_email,
  minimod_email_request_fptr in_callback,
  void *in_udata);


typedef void (*minimod_email_exchange_fptr)(
  void *userdata,
  char const *token,
  size_t ntoken_bytes);


MINIMOD_LIB void
minimod_email_exchange(
  char const *in_code,
  minimod_email_exchange_fptr in_callback,
  void *in_udata);


struct minimod_user
{
	uint64_t id;
	char const *name;
};


typedef void (*minimod_get_users_fptr)(
  void *userdata,
  size_t nusers,
  struct minimod_user const *users);


MINIMOD_LIB bool
minimod_get_me(minimod_get_users_fptr in_callback, void *in_udata);


MINIMOD_LIB bool
minimod_is_authenticated(void);


MINIMOD_LIB void
minimod_deauthenticate(void);


struct minimod_modfile
{
	uint64_t id;
	char const *md5;
	char const *url;
	uint64_t filesize;
};


typedef void (*minimod_get_modfiles_fptr)(
  void *userdata,
  size_t nmodfiles,
  struct minimod_modfile const *modfiles);


MINIMOD_LIB void
minimod_get_modfiles(
  char const *in_filter,
  uint64_t in_gameid,
  uint64_t in_modid,
  uint64_t in_modfileid,
  minimod_get_modfiles_fptr in_callback,
  void *in_udata);


typedef void (*minimod_download_fptr)(void *userdata, char const *path);


MINIMOD_LIB void
minimod_download(
  uint64_t in_gameid,
  uint64_t in_modid,
  uint64_t in_modfileid,
  minimod_download_fptr in_callback,
  void *in_udata);


typedef void (*minimod_install_fptr)(void *userdata, char const *path);


MINIMOD_LIB void
minimod_install(
  uint64_t in_gameid,
  uint64_t in_modid,
  uint64_t in_modfileid,
  minimod_install_fptr in_callback,
  void *in_udata);


typedef void (*minimod_rate_fptr)(void *userdata, bool success);


MINIMOD_LIB void
minimod_rate(
  uint64_t in_gameid,
  uint64_t in_modid,
  int in_rating,
  minimod_rate_fptr in_callback,
  void *in_udata);


struct minimod_rating
{
	uint64_t gameid;
	uint64_t modid;
	uint64_t date;
	int64_t rating;
};


typedef void (*minimod_get_ratings_fptr)(
  void *userdata,
  size_t nratings,
  struct minimod_rating const *ratings);


MINIMOD_LIB void
minimod_get_ratings(
  char const *in_filter,
  minimod_get_ratings_fptr in_callback,
  void *in_udata);


MINIMOD_LIB void
minimod_get_subscriptions(
  char const *in_filter,
  minimod_get_mods_fptr in_callback,
  void *in_udata);


typedef void (*minimod_subscription_change_fptr)(
  void *userdata,
  uint64_t mod_id,
  int change);


MINIMOD_LIB bool
minimod_subscribe(
  uint64_t in_gameid,
  uint64_t in_modid,
  minimod_subscription_change_fptr in_callback,
  void *in_udata);


MINIMOD_LIB bool
minimod_unsubscribe(
  uint64_t in_gameid,
  uint64_t in_modid,
  minimod_subscription_change_fptr in_callback,
  void *in_udata);


MINIMOD_LIB char const *
minimod_get_more_string(void const *more, char const *name);


MINIMOD_LIB int64_t
minimod_get_more_int(void const *more, char const *name);


MINIMOD_LIB double
minimod_get_more_float(void const *more, char const *name);


MINIMOD_LIB bool
minimod_get_more_bool(void const *more, char const *name);


#ifdef __cplusplus
} // extern "C"
#endif
