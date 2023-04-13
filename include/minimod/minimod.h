// vi: filetype=c
#pragma once
#ifndef MINIMOD_MINIMOD_H_INCLUDED
#define MINIMOD_MINIMOD_H_INCLUDED

/* Title: minimod
 *
 * Topic: Introduction
 *
 * minimod is a lightweight C interface around mod.io's API.
 *
 * Most functions execute asynchronously and thusly take a callback function
 * (*in_callback*) and userpointer (*in_userdata*) as arguments.
 */

#include <stddef.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdint.h>
#include <sys/types.h>

#if defined(_WIN32)
#if defined(MINIMOD_BUILD_LIB)
#define MINIMOD_LIB __declspec(dllexport)
#else
#define MINIMOD_LIB __declspec(dllimport)
#endif
#else
#if defined(MINIMOD_BUILD_LIB)
#define MINIMOD_LIB __attribute__((visibility("default")))
#else
#define MINIMOD_LIB
#endif
#endif

#define MINIMOD_CURRENT_ABI 2

#ifdef __cplusplus
extern "C" {
#endif

/* Section: API */

/* Enum: minimod_initflag
 *
 * Flags to configure how minimod operates, used with <minimod_init()>.
 *
 * MINIMOD_INITFLAG_TESTENV - Connect to mod.io's test server instead of
 *	the live system. https://test.mod.io
 * MINIMOD_INITFLAG_UNZIP - Mods are downloaded as ZIP files from mod.io.
 *	If your game cannot handle those directly and needs the files to be
 *	unpacked, this flag is what you are looking for.
 */
enum minimod_initflag
{
	MINIMOD_INITFLAG_TESTENV = 1,
	MINIMOD_INITFLAG_UNZIP = 2,
};

/* Enum: minimod_err
 *
 * Return values of <minimod_init()>.
 *
 * MINIMOD_ERR_OK - No error. Everything's fine, mostly.
 * MINIMOD_ERR_ABI - ABI not compatible.
 * MINIMOD_ERR_PATH - Unable to access or create root-path.
 * MINIMOD_ERR_KEY - No or invalid API key.
 * MINIMOD_ERR_NET - Unable to initialize netw.
 */
enum minimod_err
{
	MINIMOD_ERR_OK = 0,
	MINIMOD_ERR_ABI,
	MINIMOD_ERR_PATH,
	MINIMOD_ERR_KEY,
	MINIMOD_ERR_NET,
};

/* Enum: minimod_eventtype
 *
 * Different event types of the mod.io API.
 *
 * MINIMOD_EVENTTYPE_UNKNOWN - APIs event type string is unknown
 *
 * https://docs.mod.io/#get-user-events
 *
 * MINIMOD_EVENTTYPE_SUBSCRIBE - "USER_SUBSCRIBE"
 * MINIMOD_EVENTTYPE_UNSUBSCRIBE - "USER_UNSUBSCRIBE"
 * MINIMOD_EVENTTYPE_TEAM_JOIN - "USER_TEAM_JOIN"
 * MINIMOD_EVENTTYPE_TEAM_LEAVE - "USER_TEAM_LEAVE"
 *
 * https://docs.mod.io/#events
 *
 * MINIMOD_EVENTTYPE_MOD_AVAILABLE - "MOD_AVAILABLE"
 * MINIMOD_EVENTTYPE_MOD_UNAVAILABLE - "MOD_UNAVAILABLE"
 * MINIMOD_EVENTTYPE_MOD_EDITED - "MOD_EDITED"
 * MINIMOD_EVENTTYPE_MOD_DELETED - "MOD_DELETED"
 * MINIMOD_EVENTTYPE_MODFILE_CHANGED - "MODFILE_CHANGED"
 */
enum minimod_eventtype
{
	MINIMOD_EVENTTYPE_UNKNOWN,
	MINIMOD_EVENTTYPE_SUBSCRIBE,
	MINIMOD_EVENTTYPE_UNSUBSCRIBE,
	MINIMOD_EVENTTYPE_TEAM_JOIN,
	MINIMOD_EVENTTYPE_TEAM_LEAVE,
	MINIMOD_EVENTTYPE_MOD_AVAILABLE,
	MINIMOD_EVENTTYPE_MOD_UNAVAILABLE,
	MINIMOD_EVENTTYPE_MOD_EDITED,
	MINIMOD_EVENTTYPE_MOD_DELETED,
	MINIMOD_EVENTTYPE_MODFILE_CHANGED,
};

/* Enum: minimod_mod_status
 *
 * https://docs.mod.io/#status-amp-visibility
 *
 * MINIMOD_MODSTATUS_NOT_ACCEPTED - Not accepted and not shown when browsing
 * MINIMOD_MODSTATUS_ACCEPTED - Accepted and shown
 * MINIMOD_MODSTATUS_ARCHIVED - Accepted and shown,
 *  but flagged as out-of-date or incompatible
 * MINIMOD_MODSTATUS_DELETED - Only shown through /me endpoints
 *  (i.e. subscriptions, ratings, events, ...)
 *
 * See:
 *  <minimod_mod>
 */
enum minimod_modstatus
{
	MINIMOD_MODSTATUS_NOT_ACCEPTED = 0,
	MINIMOD_MODSTATUS_ACCEPTED = 1,
	MINIMOD_MODSTATUS_ARCHIVED = 2,
	MINIMOD_MODSTATUS_DELETED = 3,
};

/* Struct: minimod_game
 *
 * https://docs.mod.io/#game-object
 *
 * more - Allows access to more data (<[More Is Less]>)
 */
struct minimod_game
{
	uint64_t id;
	char const *name;
	void const *more;
};

/* Struct: minimod_stats
 *
 * https://docs.mod.io/#stats-object
 *
 * more - Allows access to more data (<[More Is Less]>)
 */
struct minimod_stats
{
	uint64_t mod_id;
	uint64_t ndownloads;
	uint64_t nsubscribers;
	uint64_t nratings_positive;
	uint64_t nratings_negative;
	void const *more;
};

/* Struct: minimod_user
 *
 * https://docs.mod.io/#user-object
 *
 * more - Allows access to more data (<[More Is Less]>)
 */
struct minimod_user
{
	uint64_t id;
	char const *username;
	void const *more;
};

/* Struct: minimod_mod
 *
 * https://docs.mod.io/#mod-object
 *
 * more - Allows access to more data (<[More Is Less]>)
 */
struct minimod_mod
{
	uint64_t id;
	uint64_t game_id;
	uint64_t modfile_id;
	uint64_t date_updated;
	char const *name;
	char const *summary;
	void const *more;
	struct minimod_user submitted_by;
	struct minimod_stats stats;
	enum minimod_modstatus status;
	char _padding[4];
};

/* Struct: minimod_modfile
 *
 * https://docs.mod.io/#modfile-object
 *
 * more - Allows access to more data (<[More Is Less]>)
 */
struct minimod_modfile
{
	uint64_t id;
	uint64_t mod_id;
	uint64_t date_added;
	char const *md5;
	char const *url;
	uint64_t filesize;
	void const *more;
};

/* Struct: minimod_rating
 *
 * https://docs.mod.io/#rating-object
 *
 * more - Allows access to more data (<[More Is Less]>)
 */
struct minimod_rating
{
	uint64_t game_id;
	uint64_t mod_id;
	uint64_t date;
	int64_t rating;
	void const *more;
};

/* Struct: minimod_event
 *
 * https://docs.mod.io/#user-event-object and
 * https://docs.mod.io/#mod-event-object
 *
 * more - Allows access to more data (<[More Is Less]>)
 */
struct minimod_event
{
	uint64_t id;
	uint64_t game_id;
	uint64_t mod_id;
	uint64_t user_id;
	uint64_t date_added;
	void const *more;
	enum minimod_eventtype type;
	char _padding[4];
};

/* Struct: minimod_pagination
 *
 * https://docs.mod.io/#pagination
 *
 * If you wonder why *count* is not included: Every callback function with
 *  a pagination parameter also has a *size_t nsomething* parameter that
 *  contains the number of returned entries.
 */
struct minimod_pagination
{
	uint64_t offset;
	uint64_t limit;
	uint64_t total;
};

/* Topic: [More Is Less]
 *
 *   minimod-structs only contain a subset of the underlying JSON
 *   objects.
 *
 *   However, the structs include a *more* field, which can be used together
 *   with the *minimod_get_more*-functions to access more fields.
 *   - <minimod_get_more_string()>
 *   - <minimod_get_more_int()>
 *   - <minimod_get_more_bool()>
 *   - <minimod_get_more_float()>
 *
 *   This way neither memory nor time is spent on extracting/converting
 *   fields of the underlying JSON object, which may not even be required
 *   by the calling code.
 *   But they are still accessible should the need arise.
 *   
 *   This has *another advantage*: if the underlying API adds new fields
 *   there is no need to update minimod, nor wait for minimod to be updated
 *   but the API's new features can be exploited immediately.
 *
 *   Example:
 *   (start code)
 * static void
 * get_games_callback(void *udata, size_t ngames, struct minimod_game const *games, struct minimod_pagination const *pagi)
 * {
 * 	for (size_t i = 0; i < ngames; ++i)
 * 	{
 * 		// use existing fields in struct minimod_game
 * 		printf("- %s {%" PRIu64 "}\n", games[i].name, games[i].id);
 * 		// access name_id in the underlying JSON object to generate the game's URL
 * 		printf("\t+ https://%s.mod.io\n", minimod_get_more_string(games[i].more, "name_id"));
 * 		// access even more data to show when the game was added to mod.io
 * 		time_t added = (time_t)minimod_get_more_int(games[i].more, "date_added");
 * 		printf("\t+ date added: %s\n", ctime(&added));
 * 	}
 * }
 *   (end)
 */

/* Callback: minimod_get_games_callback()
 *
 * See:
 *	<minimod_get_games()>
 */
typedef void (*minimod_get_games_callback)(
  void *userdata,
  size_t ngames,
  struct minimod_game const *games,
  struct minimod_pagination const *pagi);

/* Callback: minimod_get_mods_callback()
 *
 * See:
 *  <minimod_get_mods()>, <minimod_get_installed_mod()>,
 *  <minimod_get_subscriptions()>
 */
typedef void (*minimod_get_mods_callback)(
  void *userdata,
  size_t nmods,
  struct minimod_mod const *mods,
  struct minimod_pagination const *pagi);

/* Callback: minimod_get_modfiles_callback()
 *
 * See:
 *  <minimod_get_modfiles()>
 */
typedef void (*minimod_get_modfiles_callback)(
  void *userdata,
  size_t nmodfiles,
  struct minimod_modfile const *modfiles,
  struct minimod_pagination const *pagi);

/* Callback: minimod_get_users_callback()
 *
 * See:
 *  <minimod_get_me()>
 */
typedef void (*minimod_get_users_callback)(
  void *userdata,
  size_t nusers,
  struct minimod_user const *users,
  struct minimod_pagination const *pagi);

/* Callback: minimod_email_request_callback()
 *
 * Parameters:
 *	success - *true* if the request was transmitted successfully
 *
 * See:
 *  <minimod_email_request()>
 */
typedef void (*minimod_email_request_callback)(void *userdata, bool success);

/* Callback: minimod_access_token_callback()
 *
 * If the authorization attempt failed *token* is NULL and *ntoken_bytes* is 0.
 * Otherwise *token* is a pointer to the access-token of *ntoken_bytes*.
 *
 * See:
 *  <minimod_email_exchange()>, <minimod_steam_auth()>
 */
typedef void (*minimod_access_token_callback)(
  void *userdata,
  char const *token,
  size_t ntoken_bytes);

/* Callback: minimod_install_callback()
 *
 * Parameters:
 *  in_success - true if the installation went according to plan
 *  in_game_id - game-id of the installed mod
 *  in_mod_id - mod-id of the installed mod
 *
 * See:
 *  <minimod_install()>
 */
typedef void (*minimod_install_callback)(
  void *in_userdata,
  bool in_success,
  uint64_t in_game_id,
  uint64_t in_mod_id);

/* Callback: minimod_enum_installed_mods_callback()
 *
 * Called once for each currently installed mod.
 * *in_path* is either the path to the ZIP file or directory where the mod
 * was extracted to, if minimod was told to do so (See <minimod_initflag>).
 *
 * See:
 *  <minimod_enum_installed_mods()>
 */
typedef void (*minimod_enum_installed_mods_callback)(
  void *in_userdata,
  uint64_t in_game_id,
  uint64_t in_mod_id,
  char const *in_path);

/* Callback: minimod_get_events_callback()
 *
 * See:
 *  <minimod_get_user_events()>, <minimod_get_mod_events()>
 */
typedef void (*minimod_get_events_callback)(
  void *userdata,
  size_t nevents,
  struct minimod_event const *events,
  struct minimod_pagination const *pagi);

/* Callback: minimod_get_dependencies_callback()
 *
 * See:
 *  <minimod_get_dependencies()>
 */
typedef void (*minimod_get_dependencies_callback)(
  void *userdata,
  size_t ndependencies,
  uint64_t const *dependencies,
  struct minimod_pagination const *pagi);

/* Callback: minimod_rate_callback()
 *
 * See:
 *  <minimod_rate()>
 */
typedef void (*minimod_rate_callback)(void *userdata, bool success);

/* Callback: minimod_get_ratings_callback()
 *
 * See:
 *	<minimod_get_ratings()>
 */
typedef void (*minimod_get_ratings_callback)(
  void *userdata,
  size_t nratings,
  struct minimod_rating const *ratings,
  struct minimod_pagination const *pagi);

/* Callback: minimod_subscription_change_callback()
 *
 * Parameters:
 *	mod_id - mod-id of the subscribed/unsubscribed mod.
 *	change - 1 = subscribed; -1 = unsubscribed; 0 = error.
 *
 * See:
 *  <minimod_subscribe()>, <minimod_unsubscribe()>
 */
typedef void (*minimod_subscription_change_callback)(
  void *userdata,
  uint64_t mod_id,
  int change);

/* Function: minimod_init()
 *
 * Not surprisingly this needs to be called before any other minimod_*
 * functions can be (successfully) used.
 *
 * Parameters:
 *	in_api_key - Your API key. Get one at https://mod.io/apikey/widget
 *		or https://test.mod.io/apikey/widget
 *	in_root_path - Absolute or relative root path where data/mods &c. will
 *		be stored. Needs to be writeable by the user. If the
 *		directory does not yet exist, it will be created automatically.
 *	in_flags - Combination of <minimod_initflag>s.
 *	in_abi_version - Has to be MINIMOD_CURRENT_ABI.
 *		This bit is used to make sure the actual library and the header
 *		you were compiling against are ABI compatible.
 *
 * Returns:
 *	MINIMOD_ERR_OK on success. See <minimod_err> for possible errors.
 *
 * See:
 *  <minimod_deinit()>
 */
MINIMOD_LIB enum minimod_err
minimod_init(
  char const *in_api_key,
  char const *in_root_path,
  unsigned int in_flags,
  uint32_t in_abi_version);

/* Function: minimod_deinit()
 *
 * When you are done with minimod, call this function to free all resources.
 */
MINIMOD_LIB void
minimod_deinit(void);

/* Function: minimod_is_ratelimited()
 *
 * Returns:
 *  A negative value when the API is not currently rate-limited.
 *  Otherwise it returns the seconds until the limiting expires.
 */
MINIMOD_LIB int64_t
minimod_is_ratelimited(void);

/* Function: minimod_set_debugtesting()
 *
 * Enable random delays in server responses and a chance for failed
 * requests. This is meant to help battle-hardening the client application
 * against variable and long response times as well as server failures.
 *
 * Set *max_delay* to 0 to disable latency-simulation.
 * *min_delay* needs to be *<= max_delay*.
 *
 * Parameters:
 *	error_rate - [0;100]% specify chance of internal server error
 *	min_delay - Minimum delay of responses in milliseconds
 *	max_delay - Maximum delay of responses in milliseconds
 */
MINIMOD_LIB void
minimod_set_debugtesting(int error_rate, int min_delay, int max_delay);

/* Topic: Queries */

/* Topic: [Filtering Sorting Pagination]
 *
 *  Many functions have an *in_filter* parameter.
 *  This string is sent verbatim (after percent-encoding accordingly)
 *  to the mod.io API.
 *
 *  It is called *in_filter* but it is really meant to be a combination
 *  of Filtering (https://docs.mod.io/#filtering),
 *  Sorting (https://docs.mod.io/#sorting) and
 *  Pagination (https://docs.mod.io/#pagination).
 *
 *  Example for *in_filter* that returns the 10 highest rated mods
 *  with names ending in "Asset Pack":
 *  (start code)
 *  char const *filter = "name-lk=*Asset Pack&_sort=-rating&_limit=10";
 *  (end)
 */

/* Function: minimod_get_games()
 *
 *	Retrieve all available games on mod.io.
 *
 *	Parameters:
 *		in_filter - Can be NULL, otherwise see <[Filtering Sorting Pagination]>
 *
 *	See:
 *	  https://docs.mod.io/#get-all-games
 */
MINIMOD_LIB void
minimod_get_games(
  char const *in_filter,
  minimod_get_games_callback in_callback,
  void *in_userdata);

/* Function: minimod_get_mods()
 *
 * Retrieve a list of mods for *in_game_id*. Or if *in_mod_id* is also set
 * only information for this specific mod is retrieved.
 *
 *	Parameters:
 *		in_filter - Can be NULL, otherwise see <[Filtering Sorting Pagination]>
 *		in_game_id - ID of the game for which a list of mods shall be retrieved
 *		in_mod_id - ID for the specific mod to retrieve data about, or 0
 *			to get all mods for the game.
 *
 *	See:
 *	  https://docs.mod.io/#get-all-mods
 */
MINIMOD_LIB void
minimod_get_mods(
  char const *in_filter,
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_get_mods_callback in_callback,
  void *in_userdata);

/* Function: minimod_get_modfiles()
 *
 * Retrieve a list of available modfiles for a certain mod.
 *
 * Parameters:
 *		in_filter - Can be NULL, otherwise see <[Filtering Sorting Pagination]>
 *		in_game_id - ID of the game for which the mod was made
 *		in_mod_id - ID of the mod for which available modfiles shall be retrieved
 *		in_modfile_id - Either 0 to request a list of all available modfiles
 *			for the specified mod, or an actual modfile-ID to request only
 *			information for this specific modfile_id
 *
 * See:
 *  https://docs.mod.io/#get-all-modfiles
 */
MINIMOD_LIB void
minimod_get_modfiles(
  char const *in_filter,
  uint64_t in_game_id,
  uint64_t in_mod_id,
  uint64_t in_modfile_id,
  minimod_get_modfiles_callback in_callback,
  void *in_userdata);

/* Function: minimod_get_mod_events()
 *
 * Get events for the specified mod.
 *
 * Parameters:
 *		in_filter - Can be NULL, otherwise see <[Filtering Sorting Pagination]>
 *		in_game_id - ID of the game for which the request is made
 *		in_mod_id - Is optional and can be set to 0, requesting events for
 *			all the mods of the specified game to be fetched
 *		in_date_cutoff - Is optional and can be set to 0 and is otherwise
 *			just a shorthand to limit the events to newer ones than the
 *			cutoff date, without requiring to use *in_filter* just for that
 *
 * Example:
 *		This way it is easy to check for an updated version of an already
 *		installed mod. If the callback *on_check_for_updates()* is called with
 *		*nevents == 0*, then the installed modfile is up to date.
 *
 * (start code)
 * minimod_get_mod_events(
 *		"event_type-eq=MODFILE_CHANGED&latest=true",
 *		YOUR_GAME_ID,
 *		MOD_IN_QUESTION,
 *		TIME_OF_INSTALL,
 *		on_check_for_updates,
 *		NULL);
 * (end)
 *
 * See:
 *  https://docs.mod.io/#get-all-mod-events
 */
MINIMOD_LIB void
minimod_get_mod_events(
  char const *in_filter,
  uint64_t in_game_id,
  uint64_t in_mod_id,
  uint64_t in_date_cutoff,
  minimod_get_events_callback in_callback,
  void *in_userdata);

/* Function: minimod_get_dependencies()
 *
 * Retrieve all dependencies for the specified mod.
 *
 * Parameters:
 *  in_game_id - id of the game to which the mod belongs
 *  in_mod_id - id of the mod
 *
 * See:
 *  https://docs.mod.io/#get-all-mod-dependencies
 */
MINIMOD_LIB void
minimod_get_dependencies(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_get_dependencies_callback in_callback,
  void *in_userdata);


/* Topic: Authentication */

/* Function: minimod_is_authenticated()
 *
 * Returns:
 *  true if an access token is locally available.
 *  But the token may be expired or invalidated by the server, so this
 *  is not a guarantee that a call will succeed. If a call fails because
 *  the token is invalid, minimod automatically removes the token and
 *  *minimod_is_authenticated()* will return false from there on.
 *
 * See:
 *	<minimod_deauthenticate()>
 */
MINIMOD_LIB bool
minimod_is_authenticated(void);

/* Function: minimod_deauthenticate()
 *
 * Remove the current access token from the system.
 */
MINIMOD_LIB void
minimod_deauthenticate(void);

/* Function: minimod_email_request()
 *
 * Request an authentication code to be sent to *in_email*.
 *
 * See:
 *  <minimod_email_exchange()>, https://docs.mod.io/#authenticate-via-email
 */
MINIMOD_LIB void
minimod_email_request(
  char const *in_email,
  minimod_email_request_callback in_callback,
  void *in_userdata);

/* Function: minimod_email_exchange()
 *
 * Request an access token by sending *in_code*, which was requested
 * previously with <minimod_email_request()>, to the server.
 *
 * See:
 *  <minimod_email_request()>, https://docs.mod.io/#authenticate-via-email
 */
MINIMOD_LIB void
minimod_email_exchange(
  char const *in_code,
  minimod_access_token_callback in_callback,
  void *in_userdata);

/* Function: minimod_steam_auth()
 *
 * Request an access token by sending *in_ticket* (of *in_ticketbytes* length),
 * which was requested previously with
 * https://partner.steamgames.com/doc/api/ISteamUser#GetEncryptedAppTicket,
 * to the server.
 *
 * See:
 *  https://docs.mod.io/#authenticate-via-steam
 */
MINIMOD_LIB void
minimod_steam_auth(
  void const *in_ticket,
  size_t in_ticketbytes,
  minimod_access_token_callback in_callback,
  void *in_userdata);

/* Topic: Me */

/* Function: minimod_get_me()
 *
 * Fetch information about the currently authenticated user.
 *
 * Returns:
 *	false if no user is currently authenticated.
 */
MINIMOD_LIB bool
minimod_get_me(minimod_get_users_callback in_callback, void *in_userdata);

/* Function: minimod_get_user_events()
 *
 * Get events for the currently authenticated user.
 *
 * Parameters:
 *		in_filter - Can be NULL, otherwise see <[Filtering Sorting Pagination]>
 *		in_game_id - Is optional and can be set to 0 and is otherwise just
 *			a shorthand to limit the events to game_id, without requiring
 *			to use in_filter just for that
 *		in_date_cutoff - Is optional and can be set to 0 and is otherwise
 *			just a shorthand to limit the events to newer ones than the
 *			cutoff date, without requiring to use in_filter just for that
 *
 * Returns:
 *	false if no user is currently authenticated.
 *
 * See:
 *  https://docs.mod.io/#get-user-events
 */
MINIMOD_LIB bool
minimod_get_user_events(
  char const *in_filter,
  uint64_t in_game_id,
  uint64_t in_date_cutoff,
  minimod_get_events_callback in_callback,
  void *in_userdata);


/* Topic: Installation */

/* Function: minimod_install()
 *
 * Install a mod to the mod directory. This will either just download the
 * ZIP file or, if MINIMOD_INITFLAG_UNZIP was set, decompress the ZIP file
 * into a directory.
 *
 * Parameters:
 *	in_game_id - Cannot be 0.
 *	in_mod_id - Cannot be 0.
 *	in_modfile_id - Can be 0 to select the most current modfile for the mod.
 */
MINIMOD_LIB void
minimod_install(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  uint64_t in_modfile_id,
  minimod_install_callback in_callback,
  void *in_userdata);

/* Function: minimod_uninstall()
 *
 * Attempt to uninstall (delete) the specified mod.
 */
MINIMOD_LIB bool
minimod_uninstall(uint64_t in_game_id, uint64_t in_mod_id);

/* Function: minimod_is_installed()
 *
 * Returns:
 *	true if the specified mod is installed.
 */
MINIMOD_LIB bool
minimod_is_installed(uint64_t in_game_id, uint64_t in_mod_id);

/* Function: minimod_is_downloading()
 *
 * Returns:
 *	true if the mod is currently downloading (or being extracted)
 */
MINIMOD_LIB bool
minimod_is_downloading(uint64_t in_game_id, uint64_t in_mod_id);

/* Function: minimod_enum_installed_mods()
 *
 * Enumerate all currently installed mods.
 *
 * Parameters:
 *  in_game_id - Can either specify a game-id to limit the enumeration
 *		or 0 to enumerate all installed mods.
 */
MINIMOD_LIB void
minimod_enum_installed_mods(
  uint64_t in_game_id,
  minimod_enum_installed_mods_callback in_callback,
  void *in_userdata);

/* Function: minimod_get_installed_mod()
 *
 * Get the cached information for a installed mod.
 * Since mod-data (descriptions, names, &c.) may change as the mod evolves,
 * minimod stores the mod-information at the time of installing. This
 * function accesses this data.
 *
 * Returns:
 *  false if the specified mod is not installed.
 */
MINIMOD_LIB bool
minimod_get_installed_mod(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_get_mods_callback in_callback,
  void *in_userdata);


/* Topic: Ratings */

/* Function: minimod_rate()
 *
 * Rate a mod as the currently authenticated user.
 *
 * Params:
 *	in_rating - Positive value is a positive rating, while a negative
 *		value indicates a negative rating. Shocking, is it not?
 *		Use 0 to remove any previously set rating by the user.
 *
 * Returns:
 *	false if no user is currently authenticated.
 *
 * See:
 *  https://docs.mod.io/#ratings
 */
MINIMOD_LIB bool
minimod_rate(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  int in_rating,
  minimod_rate_callback in_callback,
  void *in_userdata);

/* Function: minimod_get_ratings()
 *
 * Retrieve all ratings of currently authenticated user.
 *
 * Returns:
 *	false if no user is currently authenticated.
 *
 * See:
 *  https://docs.mod.io/#get-user-ratings
 */
MINIMOD_LIB bool
minimod_get_ratings(
  char const *in_filter,
  minimod_get_ratings_callback in_callback,
  void *in_userdata);


/* Topic: Subscriptions */

/* Function: minimod_get_subscriptions()
 *
 * Retrieve all subscriptions of the currently authenticated user.
 *
 * Returns:
 *	false if no user is currently authenticated.
 *
 * See:
 *  https://docs.mod.io/#get-user-subscriptions
 */
MINIMOD_LIB bool
minimod_get_subscriptions(
  char const *in_filter,
  minimod_get_mods_callback in_callback,
  void *in_userdata);

/* Function: minimod_subscribe()
 *
 * Subscribe to a mod.
 *
 * Returns:
 *	false if no user is currently authenticated.
 *
 * See:
 *  <minimod_unsubscribe()>, https://docs.mod.io/#subscribe
 */
MINIMOD_LIB bool
minimod_subscribe(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_subscription_change_callback in_callback,
  void *in_userdata);

/* Function: minimod_unsubscribe()
 *
 * Unsubscribe from a mod.
 *
 * Returns:
 *	false if no user is currently authenticated.
 *
 * See:
 *  <minimod_subscribe()>, https://docs.mod.io/#subscribe
 */
MINIMOD_LIB bool
minimod_unsubscribe(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_subscription_change_callback in_callback,
  void *in_userdata);


/* Topic: 'more' */

/* Function: minimod_get_more_string()
 *
 * Access data from JSON objects not 1:1 mapped to minimod-structs.
 * See <[More Is Less]>
 */
MINIMOD_LIB char const *
minimod_get_more_string(void const *in_more, char const *in_name);

/* Function: minimod_get_more_int()
 *
 * Access data from JSON objects not 1:1 mapped to minimod-structs.
 * See <[More Is Less]>
 */
MINIMOD_LIB int64_t
minimod_get_more_int(void const *in_more, char const *in_name);

/* Function: minimod_get_more_float()
 *
 * Access data from JSON objects not 1:1 mapped to minimod-structs.
 * See <[More Is Less]>
 */
MINIMOD_LIB double
minimod_get_more_float(void const *in_more, char const *in_name);

/* Function: minimod_get_more_bool()
 *
 * Access data from JSON objects not 1:1 mapped to minimod-structs.
 * See <[More Is Less]>
 */
MINIMOD_LIB bool
minimod_get_more_bool(void const *in_more, char const *in_name);

#ifdef __cplusplus
} // extern "C"
#endif

/* Section: Examples */

/* Topic: List all available games on mod.io

Initialize minimod and execute request to fetch list of games from
mod.io server. Wait for the asynchronous response from the API and
list the games when the response from the server was received.

(start code)
#include "minimod/minimod.h"
#include <stdio.h>

void
get_games_callback(void *udata, size_t ngames, struct minimod_game const *games, struct minimod_pagination const *pagi)
{
	for (size_t i = 0; i < ngames; ++i)
	{
		printf("- %s {%llu}\n", games[i].name, games[i].id);
	}

	// signal waiting loop in main() that we are done.
	int *nrequests_completed = udata;
	*nrequests_completed += 1;
}

int
main(void)
{
	minimod_init(YOUR_API_KEY, NULL, 0, MINIMOD_CURRENT_ABI);

	printf("List of games on mod.io\n");

	int nrequests_completed = 0;
	minimod_get_games(NULL, get_games_callback, &nrequests_completed);

	// wait for the callback to finish
	while (nrequests_completed == 0)
	{
		// sleep
	}

	minimod_deinit();
}
(end)

Output:

(start example)
List of games on mod.io
- Mod Support {1}
- Sinespace {2}
- 0 A.D. {5}
- ECO {6}
- Mondrian - Plastic Reality {11}
- Aground {34}
- OpenXcom {51}
- Foundation {63}
- Meeple Station {77}
- Hard Times {102}
- Spoxel {112}
- Avis Rapida {135}
- Trains & Things {139}
- Totally Accurate Battle Simulator {152}
- Playcraft {164}
(end)

*/

#endif
