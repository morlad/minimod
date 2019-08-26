// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0
#include "minimod/minimod.h"

#include "miniz/miniz.h"
#include "netw.h"
#include "qajson4c/src/qajson4c/qajson4c.h"
#include "util.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define UNUSED(X) __pragma(warning(suppress : 4100)) X
#else
#define UNUSED(X) __attribute__((unused)) X
#endif


// CONFIG
// ------
#define DEFAULT_ROOT "_minimod"


struct callback
{
	union
	{
		minimod_get_games_callback get_games;
		minimod_get_mods_callback get_mods;
		minimod_email_request_callback email_request;
		minimod_email_exchange_callback email_exchange;
		minimod_get_users_callback get_users;
		minimod_get_modfiles_callback get_modfiles;
		minimod_install_callback install;
		minimod_rate_callback rate;
		minimod_get_ratings_callback get_ratings;
		minimod_subscription_change_callback subscription_change;
		minimod_get_dependencies_callback get_dependencies;
		minimod_get_events_callback get_events;
	} fptr;
	void *userdata;
};


struct task
{
	struct callback callback;
	uint64_t meta64;
};


struct mmi
{
	char *api_key;
	char *root_path;
	char *cache_tokenpath;
	char *token;
	char *token_bearer;
	enum minimod_environment env;
	bool unzip;
	char _padding[3];
};
static struct mmi l_mmi;


static char const *endpoints[2] = {
	"https://api.mod.io/v1",
	"https://api.test.mod.io/v1",
};


static struct task *
alloc_task(void)
{
	return malloc(sizeof(struct task));
}


static void
free_task(struct task *task)
{
	free(task);
}


static char *
get_tokenpath(void)
{
	assert(l_mmi.root_path);

	if (!l_mmi.cache_tokenpath)
	{
		asprintf(&l_mmi.cache_tokenpath, "%s/token", l_mmi.root_path);
	}

	return l_mmi.cache_tokenpath;
}


static bool
read_token(void)
{
	int64_t fsize = fsu_fsize(get_tokenpath());
	if (fsize > 0)
	{
		// read file into l_mmi.token (does null-terminate it)
		FILE *f = fsu_fopen(get_tokenpath(), "rb");
		assert(f);
		l_mmi.token = malloc((size_t)(fsize + 1));
		fread(l_mmi.token, (size_t)fsize, 1, f);
		l_mmi.token[fsize] = '\0';
		fclose(f);
		asprintf(&l_mmi.token_bearer, "Bearer %s", l_mmi.token);
		return true;
	}
	return false;
}


static void
populate_game(struct minimod_game *game, QAJ4C_Value const *node)
{
	assert(game);
	assert(QAJ4C_is_object(node));

	game->id = QAJ4C_get_uint(QAJ4C_object_get(node, "id"));
	game->name = QAJ4C_get_string(QAJ4C_object_get(node, "name"));
	game->more = node;
}


static void
populate_user(struct minimod_user *user, QAJ4C_Value const *node)
{
	assert(user);
	assert(QAJ4C_is_object(node));

	user->id = QAJ4C_get_uint64(QAJ4C_object_get(node, "id"));
	user->username = QAJ4C_get_string(QAJ4C_object_get(node, "username"));
	user->more = node;
}


static void
populate_stats(struct minimod_stats *stats, QAJ4C_Value const *node)
{
	assert(stats);
	assert(QAJ4C_is_object(node));

	stats->mod_id = QAJ4C_get_uint64(QAJ4C_object_get(node, "mod_id"));
	stats->ndownloads =
	  QAJ4C_get_uint64(QAJ4C_object_get(node, "downloads_total"));
	stats->nsubscribers =
	  QAJ4C_get_uint64(QAJ4C_object_get(node, "subscribers_total"));
	stats->nratings_positive =
	  QAJ4C_get_uint64(QAJ4C_object_get(node, "ratings_positive"));
	stats->nratings_negative =
	  QAJ4C_get_uint64(QAJ4C_object_get(node, "ratings_negative"));

	stats->more = node;
}


static void
populate_modfile(struct minimod_modfile *modfile, QAJ4C_Value const *node)
{
	assert(modfile);
	assert(QAJ4C_is_object(node));

	modfile->id = QAJ4C_get_uint64(QAJ4C_object_get(node, "id"));
	modfile->filesize = QAJ4C_get_uint64(QAJ4C_object_get(node, "filesize"));

	QAJ4C_Value const *filehash = QAJ4C_object_get(node, "filehash");
	modfile->md5 = QAJ4C_get_string(QAJ4C_object_get(filehash, "md5"));

	QAJ4C_Value const *download = QAJ4C_object_get(node, "download");
	modfile->url = QAJ4C_get_string(QAJ4C_object_get(download, "binary_url"));

	modfile->more = node;
}


static void
populate_mod(struct minimod_mod *mod, QAJ4C_Value const *node)
{
	assert(mod);
	assert(QAJ4C_is_object(node));

	mod->id = QAJ4C_get_uint(QAJ4C_object_get(node, "id"));
	mod->name = QAJ4C_get_string(QAJ4C_object_get(node, "name"));
	mod->more = node;

	// modfile
	QAJ4C_Value const *modfile = QAJ4C_object_get(node, "modfile");
	assert(QAJ4C_is_object(modfile));
	QAJ4C_Value const *modfile_id = QAJ4C_object_get(modfile, "id");
	if (modfile_id)
	{
		QAJ4C_is_uint64(modfile_id);
		mod->modfile_id = QAJ4C_get_uint64(modfile_id);
	}

	// submitted_by
	QAJ4C_Value const *submitted_by = QAJ4C_object_get(node, "submitted_by");
	assert(QAJ4C_is_object(submitted_by));
	populate_user(&mod->submitted_by, submitted_by);

	// stats
	QAJ4C_Value const *stats = QAJ4C_object_get(node, "stats");
	assert(QAJ4C_is_object(stats));
	populate_stats(&mod->stats, stats);
}


static void
populate_event(struct minimod_event *event, QAJ4C_Value const *node)
{
	assert(event);
	assert(QAJ4C_is_object(node));

	event->id = QAJ4C_get_uint64(QAJ4C_object_get(node, "id"));

	// game_id is only part of user-events
	QAJ4C_Value const *game_id = QAJ4C_object_get(node, "game_id");
	if (game_id)
	{
		event->game_id = QAJ4C_get_uint64(game_id);
	}
	else
	{
		event->game_id = 0;
	}
	event->mod_id = QAJ4C_get_uint64(QAJ4C_object_get(node, "mod_id"));
	event->user_id = QAJ4C_get_uint64(QAJ4C_object_get(node, "user_id"));
	event->date_added = QAJ4C_get_uint64(QAJ4C_object_get(node, "date_added"));

	// uses string length in comparisons first to save on strcmp()s
	event->type = MINIMOD_EVENTTYPE_UNKNOWN;
	QAJ4C_Value const *event_type = QAJ4C_object_get(node, "event_type");
	size_t et_len = QAJ4C_get_string_length(event_type);
	char const *et = QAJ4C_get_string(event_type);
	if (et_len == 15 && 0 == strcmp(et, "MODFILE_CHANGED"))
	{
		event->type = MINIMOD_EVENTTYPE_MODFILE_CHANGED;
	}
	else if (et_len == 14 && 0 == strcmp(et, "USER_SUBSCRIBE"))
	{
		event->type = MINIMOD_EVENTTYPE_SUBSCRIBE;
	}
	else if (et_len == 16 && 0 == strcmp(et, "USER_UNSUBSCRIBE"))
	{
		event->type = MINIMOD_EVENTTYPE_UNSUBSCRIBE;
	}
	else if (et_len == 13 && 0 == strcmp(et, "MOD_AVAILABLE"))
	{
		event->type = MINIMOD_EVENTTYPE_MOD_AVAILABLE;
	}
	else if (et_len == 15 && 0 == strcmp(et, "MOD_UNAVAILABLE"))
	{
		event->type = MINIMOD_EVENTTYPE_MOD_UNAVAILABLE;
	}
	else if (et_len == 10 && 0 == strcmp(et, "MOD_EDITED"))
	{
		event->type = MINIMOD_EVENTTYPE_MOD_EDITED;
	}
	else if (et_len == 11 && 0 == strcmp(et, "MOD_DELETED"))
	{
		event->type = MINIMOD_EVENTTYPE_MOD_DELETED;
	}
	else if (et_len == 14 && 0 == strcmp(et, "USER_TEAM_JOIN"))
	{
		event->type = MINIMOD_EVENTTYPE_TEAM_JOIN;
	}
	else if (et_len == 15 && 0 == strcmp(et, "USER_TEAM_LEAVE"))
	{
		event->type = MINIMOD_EVENTTYPE_MOD_DELETED;
	}

	event->more = node;
}


static void
populate_rating(struct minimod_rating *rating, QAJ4C_Value const *node)
{
	rating->game_id = QAJ4C_get_uint(QAJ4C_object_get(node, "game_id"));
	rating->mod_id = QAJ4C_get_uint(QAJ4C_object_get(node, "mod_id"));
	rating->date = QAJ4C_get_uint(QAJ4C_object_get(node, "date_added"));
	rating->rating = QAJ4C_get_int(QAJ4C_object_get(node, "rating"));
}


static void
handle_get_games(void *in_udata, void const *in_data, size_t in_len, int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.get_games(task->callback.userdata, 0, NULL);
		return;
	}

	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_Value const *document = NULL;
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	if (data)
	{
		assert(QAJ4C_is_array(data));

		size_t ngames = QAJ4C_array_size(data);
		struct minimod_game *games = calloc(sizeof *games, ngames);

		for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
		{
			populate_game(&games[i], QAJ4C_array_get(data, i));
		}

		task->callback.fptr.get_games(task->callback.userdata, ngames, games);

		free(games);
	}

	free(buffer);
}


static void
handle_get_mods(void *in_udata, void const *in_data, size_t in_len, int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.get_mods(task->callback.userdata, 0, NULL);
		return;
	}

	// parse data
	QAJ4C_Value const *document = NULL;
	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	// single item or array of items?
	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	if (data)
	{
		assert(QAJ4C_is_array(data));

		size_t nmods = QAJ4C_array_size(data);
		struct minimod_mod *mods = calloc(sizeof *mods, nmods);

		for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
		{
			populate_mod(&mods[i], QAJ4C_array_get(data, i));
		}

		task->callback.fptr.get_mods(task->callback.userdata, nmods, mods);

		free(mods);
	}
	else
	{
		struct minimod_mod mod = { 0 };
		populate_mod(&mod, document);
		task->callback.fptr.get_mods(task->callback.userdata, 1, &mod);
	}
	free(buffer);
}


static void
handle_get_users(void *in_udata, void const *in_data, size_t in_len, int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.get_mods(task->callback.userdata, 0, NULL);
		return;
	}

	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_Value const *document = NULL;
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	// check for 'data' to see if it is a 'single' or 'multi' data object
	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	if (data)
	{
		assert(QAJ4C_is_array(data));

		size_t nusers = QAJ4C_array_size(data);
		struct minimod_user *users = calloc(sizeof *users, nusers);

		for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
		{
			populate_user(&users[i], QAJ4C_array_get(data, i));
		}

		task->callback.fptr.get_users(task->callback.userdata, nusers, users);

		free(users);
	}
	// single user
	else
	{
		struct minimod_user user;
		populate_user(&user, document);
		task->callback.fptr.get_users(task->callback.userdata, 1, &user);
	}
	free(buffer);
}


static void
handle_get_modfiles(
  void *in_udata,
  void const *in_data,
  size_t in_len,
  int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.get_modfiles(task->callback.userdata, 0, NULL);
		return;
	}

	// parse data
	QAJ4C_Value const *document = NULL;
	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	// single item or array of items?
	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	if (data)
	{
		assert(QAJ4C_is_array(data));

		size_t nmodfiles = QAJ4C_array_size(data);
		struct minimod_modfile *modfiles = calloc(sizeof *modfiles, nmodfiles);

		for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
		{
			populate_modfile(&modfiles[i], QAJ4C_array_get(data, i));
		}

		task->callback.fptr.get_modfiles(
		  task->callback.userdata,
		  nmodfiles,
		  modfiles);

		free(modfiles);
	}
	else
	{
		struct minimod_modfile modfile;
		populate_modfile(&modfile, document);
		task->callback.fptr.get_modfiles(task->callback.userdata, 1, &modfile);
	}
	free(buffer);
}


static void
handle_get_events(
  void *in_udata,
  void const *in_data,
  size_t in_len,
  int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.get_events(task->callback.userdata, 0, NULL);
		return;
	}

	// parse data
	QAJ4C_Value const *document = NULL;
	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	assert(QAJ4C_is_array(data));

	size_t nevents = QAJ4C_array_size(data);
	struct minimod_event *events = calloc(sizeof *events, nevents);

	for (size_t i = 0; i < nevents; ++i)
	{
		populate_event(&events[i], QAJ4C_array_get(data, i));
	}

	task->callback.fptr.get_events(task->callback.userdata, nevents, events);

	free(events);

	free(buffer);
}


static void
handle_get_dependencies(
  void *in_udata,
  void const *in_data,
  size_t in_len,
  int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.get_dependencies(task->callback.userdata, 0, NULL);
		return;
	}

	// parse data
	QAJ4C_Value const *document = NULL;
	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	// single item or array of items?
	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	assert(QAJ4C_is_array(data));

	size_t ndeps = QAJ4C_array_size(data);
	uint64_t *deps = calloc(sizeof *deps, ndeps);

	for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
	{
		deps[i] = QAJ4C_get_uint64(QAJ4C_array_get(data, i));
	}

	task->callback.fptr.get_dependencies(task->callback.userdata, ndeps, deps);

	free(deps);

	free(buffer);
}


static void
handle_email_request(
  void *in_udata,
  void const *UNUSED(in_data),
  size_t UNUSED(in_len),
  int error)
{
	struct task *task = in_udata;
	task->callback.fptr.email_request(task->callback.userdata, error == 200);
}


static void
handle_email_exchange(
  void *in_udata,
  void const *in_data,
  size_t in_len,
  int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.email_exchange(task->callback.userdata, NULL, 0);
	}

	// parse data
	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_Value const *document = NULL;
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	QAJ4C_Value const *token = QAJ4C_object_get(document, "access_token");
	assert(QAJ4C_is_string(token));

	char const *tok = QAJ4C_get_string(token);
	size_t tok_bytes = QAJ4C_get_string_length(token);

	FILE *f = fsu_fopen(get_tokenpath(), "wb");
	fwrite(tok, tok_bytes, 1, f);
	fclose(f);

	task->callback.fptr.email_exchange(
	  task->callback.userdata,
	  tok,
	  tok_bytes);

	read_token();
}


static void
handle_rate(
  void *in_udata,
  void const *UNUSED(in_data),
  size_t UNUSED(in_len),
  int error)
{
	struct task *task = in_udata;
	if (error == 201)
	{
		printf("[mm] Rating applied successful\n");
		task->callback.fptr.rate(task->callback.userdata, true);
	}
	else
	{
		printf("[mm] Raiting not applied: %i\n", error);
		task->callback.fptr.rate(task->callback.userdata, false);
	}
}


static void
handle_get_ratings(
  void *in_udata,
  void const *in_data,
  size_t in_len,
  int error)
{
	struct task *task = in_udata;
	if (error != 200)
	{
		task->callback.fptr.get_ratings(task->callback.userdata, 0, NULL);
		return;
	}

	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);
	QAJ4C_Value const *document = NULL;
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	assert(QAJ4C_is_array(data));

	size_t nratings = QAJ4C_array_size(data);
	struct minimod_rating *ratings = calloc(sizeof *ratings, nratings);

	for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
	{
		populate_rating(&ratings[i], QAJ4C_array_get(data, i));
	}

	task->callback.fptr.get_ratings(
	  task->callback.userdata,
	  nratings,
	  ratings);

	free(ratings);

	free(buffer);
}


static void
handle_subscription_change(
  void *in_udata,
  void const *in_data,
  size_t in_bytes,
  int error)
{
	struct task *task = in_udata;

	if (task->meta64 > 0)
	{
		if (error == 201)
		{
			task->callback.fptr.subscription_change(
			  task->callback.userdata,
			  task->meta64,
			  1);
		}
		else
		{
			printf(
			  "[mm] failed to subscribe %i [modid: %" PRIu64 "]\n",
			  error,
			  task->meta64);
			task->callback.fptr.subscription_change(
			  task->callback.userdata,
			  task->meta64,
			  0);
		}
	}
	else
	{
		if (error == 204)
		{
			task->callback.fptr.subscription_change(
			  task->callback.userdata,
			  -task->meta64,
			  -1);
		}
		else
		{
			printf(
			  "[mm] failed to unsubscribe %i [modid: %" PRIu64 "]\n",
			  error,
			  -task->meta64);
			task->callback.fptr.subscription_change(
			  task->callback.userdata,
			  -task->meta64,
			  0);
		}
	}

	free_task(task);
}


enum minimod_err
minimod_init(
  enum minimod_environment in_env,
  char const *in_api_key,
  char const *in_root_path,
  bool in_unzip,
  uint32_t in_abi_version)
{
	// check version compatibility
	if (in_abi_version != MINIMOD_CURRENT_ABI)
	{
		return MINIMOD_ERR_ABI;
	}

	// check if API key is given
	if (!in_api_key)
	{
		return MINIMOD_ERR_KEY;
	}
	else
	{
		if (strlen(in_api_key) != 32)
		{
			return MINIMOD_ERR_KEY;
		}
		for (size_t i = 0; i < 32; ++i)
		{
			if (!isalnum(in_api_key[i]))
			{
				return MINIMOD_ERR_KEY;
			}
		}
	}

	// validate in_env
	if (in_env != MINIMOD_ENVIRONMENT_LIVE && in_env != MINIMOD_ENVIRONMENT_TEST)
	{
		return MINIMOD_ERR_ENV;
	}
	l_mmi.env = in_env;

	// TODO validate path
	l_mmi.root_path = strdup(in_root_path ? in_root_path : DEFAULT_ROOT);
	// make sure the path does not end with '/'
	size_t len = strlen(l_mmi.root_path);
	assert(len > 0);
	if (l_mmi.root_path[len - 1] == '/')
	{
		l_mmi.root_path[len - 1] = '\0';
	}

	// attempt to initialize netw
	if (!netw_init())
	{
		return MINIMOD_ERR_NET;
	}

	l_mmi.api_key = in_api_key ? strdup(in_api_key) : NULL;

	l_mmi.unzip = in_unzip;

	read_token();

	return MINIMOD_ERR_OK;
}


void
minimod_deinit()
{
	netw_deinit();

	free(l_mmi.root_path);
	free(l_mmi.cache_tokenpath);
	free(l_mmi.api_key);
	free(l_mmi.token);
	free(l_mmi.token_bearer);

	l_mmi = (struct mmi){ 0 };
}


void
minimod_get_games(
  char const *in_filter,
  minimod_get_games_callback in_callback,
  void *in_udata)
{
	char *path;
	asprintf(
	  &path,
	  "%s/games?api_key=%s&%s",
	  endpoints[l_mmi.env],
	  l_mmi.api_key,
	  in_filter ? in_filter : "");
	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.fptr.get_games = in_callback;
	task->callback.userdata = in_udata;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_games,
	      task))
	{
		free_task(task);
	}

	free(path);
}


void
minimod_get_mods(
  char const *in_filter,
  uint64_t in_game_id,
  minimod_get_mods_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);
	char *path;
	asprintf(
	  &path,
	  "%s/games/%" PRIu64 "/mods?api_key=%s&%s",
	  endpoints[l_mmi.env],
	  in_game_id,
	  l_mmi.api_key,
	  in_filter ? in_filter : "");

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.fptr.get_mods = in_callback;
	task->callback.userdata = in_userdata;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_mods,
	      task))
	{
		free_task(task);
	}

	free(path);
}


void
minimod_email_request(
  char const *in_email,
  minimod_email_request_callback in_callback,
  void *in_udata)
{
	char *path;
	asprintf(&path, "%s/oauth/emailrequest", endpoints[l_mmi.env]);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Content-Type", "application/x-www-form-urlencoded",
		NULL
		// clang-format on
	};

	char *payload;
	char *email = netw_percent_encode(in_email, strlen(in_email), NULL);
	int nbytes =
	  asprintf(&payload, "api_key=%s&email=%s", l_mmi.api_key, email);
	free(email);
	printf("[mm] payload: %s (%i)\n", payload, nbytes);

	assert(nbytes > 0);

	struct task *task = alloc_task();
	task->callback.fptr.email_request = in_callback;
	task->callback.userdata = in_udata;
	if (!netw_request(
	      NETW_VERB_POST,
	      path,
	      headers,
	      payload,
	      (size_t)nbytes,
	      handle_email_request,
	      task))
	{
		free_task(task);
	}

	free(payload);
	free(path);
}


void
minimod_email_exchange(
  char const *in_code,
  minimod_email_exchange_callback in_callback,
  void *in_udata)
{
	char *path;
	asprintf(&path, "%s/oauth/emailexchange", endpoints[l_mmi.env]);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Content-Type", "application/x-www-form-urlencoded",
		NULL
		// clang-format on
	};

	char *payload;
	int nbytes = asprintf(
	  &payload,
	  "api_key=%s&security_code=%s",
	  l_mmi.api_key,
	  in_code);
	printf("[mm] payload: %s (%i)\n", payload, nbytes);

	assert(nbytes > 0);

	struct task *task = alloc_task();
	task->callback.fptr.email_exchange = in_callback;
	task->callback.userdata = in_udata;
	if (!netw_request(
	      NETW_VERB_POST,
	      path,
	      headers,
	      payload,
	      (size_t)nbytes,
	      handle_email_exchange,
	      task))
	{
		free_task(task);
	}

	free(payload);
	free(path);
}


bool
minimod_get_me(minimod_get_users_callback in_callback, void *in_udata)
{
	if (!minimod_is_authenticated())
	{
		return false;
	}

	char *path;
	asprintf(&path, "%s/me", endpoints[l_mmi.env]);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Authorization", l_mmi.token_bearer,
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.fptr.get_users = in_callback;
	task->callback.userdata = in_udata;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_users,
	      task))
	{
		free_task(task);
	}

	free(path);

	return true;
}


bool
minimod_get_user_events(
  char const *in_filter,
  uint64_t in_game_id,
  uint64_t in_date_cutoff,
  minimod_get_events_callback in_callback,
  void *in_userdata)
{
	if (!minimod_is_authenticated())
	{
		return false;
	}

	char *game_filter = NULL;
	if (in_game_id)
	{
		asprintf(&game_filter, "&game_id=%" PRIu64, in_game_id);
	}
	char *cutoff_filter = NULL;
	if (in_date_cutoff)
	{
		asprintf(&cutoff_filter, "&date_added-gt=%" PRIu64, in_date_cutoff);
	}
	char *path;
	asprintf(
	  &path,
	  "%s/me/events?%s%s%s",
	  endpoints[l_mmi.env],
	  in_filter ? in_filter : "",
	  game_filter ? game_filter : "",
	  cutoff_filter ? cutoff_filter : "");

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Authorization", l_mmi.token_bearer,
		NULL
		// clang-format on
	};
	printf("[mm] request: %s\n", path);

	struct task *task = alloc_task();
	task->callback.fptr.get_events = in_callback;
	task->callback.userdata = in_userdata;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_events,
	      task))
	{
		free_task(task);
	}

	free(path);

	return true;
}


void
minimod_get_dependencies(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_get_dependencies_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);
	assert(in_mod_id > 0);

	char *path;
	asprintf(
	  &path,
	  "%s/games/%" PRIu64 "/mods/%" PRIu64 "/dependencies",
	  endpoints[l_mmi.env],
	  in_game_id,
	  in_mod_id);

	struct task *task = alloc_task();
	task->callback.fptr.get_dependencies = in_callback;
	task->callback.userdata = in_userdata;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      NULL,
	      NULL,
	      0,
	      handle_get_dependencies,
	      task))
	{
		free_task(task);
	}

	free(path);
}


bool
minimod_is_authenticated(void)
{
	return l_mmi.token && l_mmi.token_bearer;
}


void
minimod_deauthenticate(void)
{
	fsu_rmfile(get_tokenpath());

	free(l_mmi.token);
	free(l_mmi.token_bearer);

	l_mmi.token = NULL;
	l_mmi.token_bearer = NULL;
}


void
minimod_get_modfiles(
  char const *in_filter,
  uint64_t in_game_id,
  uint64_t in_mod_id,
  uint64_t in_modfile_id,
  minimod_get_modfiles_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);
	assert(in_mod_id > 0);
	char *path;
	if (in_modfile_id)
	{
		asprintf(
		  &path,
		  "%s/games/%" PRIu64 "/mods/%" PRIu64 "/files/%" PRIu64
		  "?api_key=%s&%s",
		  endpoints[l_mmi.env],
		  in_game_id,
		  in_mod_id,
		  in_modfile_id,
		  l_mmi.api_key,
		  in_filter ? in_filter : "");
	}
	else
	{
		asprintf(
		  &path,
		  "%s/games/%" PRIu64 "/mods/%" PRIu64 "/files?api_key=%s&%s",
		  endpoints[l_mmi.env],
		  in_game_id,
		  in_mod_id,
		  l_mmi.api_key,
		  in_filter ? in_filter : "");
	}
	printf("[mm] request: %s\n", path);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.fptr.get_modfiles = in_callback;
	task->callback.userdata = in_userdata;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_modfiles,
	      task))
	{
		free_task(task);
	}

	free(path);
}


void
minimod_get_mod_events(
  char const *in_filter,
  uint64_t in_game_id,
  uint64_t in_mod_id,
  uint64_t in_date_cutoff,
  minimod_get_events_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);

	char *cutoff = NULL;
	if (in_date_cutoff)
	{
		asprintf(&cutoff, "&date_added-gt=%" PRIu64, in_date_cutoff);
	}
	char *path;
	if (in_mod_id)
	{
		asprintf(
		  &path,
		  "%s/games/%" PRIu64 "/mods/%" PRIu64 "/events/"
		  "?api_key=%s&%s%s",
		  endpoints[l_mmi.env],
		  in_game_id,
		  in_mod_id,
		  l_mmi.api_key,
		  in_filter ? in_filter : "",
		  cutoff ? cutoff : "");
	}
	else
	{
		asprintf(
		  &path,
		  "%s/games/%" PRIu64 "/mods/events?api_key=%s&%s%s",
		  endpoints[l_mmi.env],
		  in_game_id,
		  l_mmi.api_key,
		  in_filter ? in_filter : "",
		  cutoff ? cutoff : "");
	}
	free(cutoff);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		NULL
		// clang-format on
	};
	printf("[mm] request: %s\n", path);

	struct task *task = alloc_task();
	task->callback.fptr.get_events = in_callback;
	task->callback.userdata = in_userdata;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_events,
	      task))
	{
		free_task(task);
	}

	free(path);
}


struct install_request
{
	minimod_install_callback callback;
	void *userdata;
	uint64_t game_id;
	uint64_t mod_id;
	char *zip_path;
	FILE *file;
};


static void
on_install_download(void *in_udata, FILE *in_file, int error)
{
	struct install_request *req = in_udata;

	if (error != 200)
	{
		printf("[mm] mod NOT downloaded\n");
		req->callback(req->userdata, NULL);
		return;
	}

	printf("[mm] mod downloaded\n");

	// extract zip?
	if (l_mmi.unzip)
	{
		long s = ftell(in_file);
		assert(s >= 0);
		int seek_err = fseek(in_file, 0, SEEK_SET);
		if (seek_err != 0)
		{
			printf("Seek failed %i\n", errno);
		}
		// unzip it
		mz_zip_archive zip = { 0 };
		if (!mz_zip_reader_init_cfile(&zip, in_file, (mz_uint64)s, 0))
		{
			printf("zip error: %i\n", zip.m_last_error);
		}
		mz_uint nfiles = mz_zip_reader_get_num_files(&zip);
		printf("#files in zip: %u\n", nfiles);
		for (mz_uint i = 0; i < nfiles; ++i)
		{
			mz_zip_archive_file_stat stat;
			mz_zip_reader_file_stat(&zip, i, &stat);
			if (!stat.m_is_directory)
			{
				char *path;
				asprintf(
				  &path,
				  "%s/mods/%" PRIu64 "/%" PRIu64 "/%s",
				  l_mmi.root_path,
				  req->game_id,
				  req->mod_id,
				  stat.m_filename);
				printf("  + extracting %s\n", path);
				FILE *f = fsu_fopen(path, "wb");
				mz_zip_reader_extract_to_cfile(&zip, i, f, 0);
				free(path);

				fclose(f);
			}
		}
		mz_zip_reader_end(&zip);
		fsu_rmfile(req->zip_path);
	}

	// callback
	req->callback(req->userdata, "-deprecated-");

	fclose(in_file);
	free(req->zip_path);
	free(req);
}


static bool
json_print_callback(void *ptr, const char *buffer, size_t size)
{
	fwrite(buffer, size, 1, ptr);
	return true;
}


static void
on_download_modfile(
  void *udata,
  size_t nmodfiles,
  struct minimod_modfile const *modfiles)
{
	assert(nmodfiles == 1);

	struct install_request *req = udata;

	// write json file
	char *jpath;
	asprintf(
	  &jpath,
	  "%s/mods/%" PRIu64 "/%" PRIu64 ".json",
	  l_mmi.root_path,
	  req->game_id,
	  req->mod_id);

	FILE *jout = fsu_fopen(jpath, "wb");
	QAJ4C_print_buffer_callback(modfiles[0].more, json_print_callback, jout);
	fclose(jout);

	free(jpath);

	// write actual file
	asprintf(
	  &req->zip_path,
	  "%s/mods/%" PRIu64 "/%" PRIu64 ".zip",
	  l_mmi.root_path,
	  req->game_id,
	  req->mod_id);
	FILE *fout = fsu_fopen(req->zip_path, "w+b");
	assert(fout);

	req->file = fout;

	netw_download_to(
	  NETW_VERB_GET,
	  modfiles[0].url,
	  NULL,
	  NULL,
	  0,
	  fout,
	  on_install_download,
	  req);
}


void
minimod_install(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  uint64_t in_modfile_id,
  minimod_install_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);
	assert(in_mod_id > 0);

	// fetch meta-data and proceed from there
	struct install_request *req = malloc(sizeof *req);
	req->callback = in_callback;
	req->userdata = in_userdata;
	req->mod_id = in_mod_id;
	req->game_id = in_game_id;
	minimod_get_modfiles(
	  NULL,
	  in_game_id,
	  in_mod_id,
	  in_modfile_id,
	  on_download_modfile,
	  req);
}


bool
minimod_uninstall(uint64_t in_game_id, uint64_t in_mod_id)
{
	// check if a json file exists. if it does not, then there is no mod either
	char *path;
	asprintf(
	  &path,
	  "%s/mods/%" PRIu64 "/%" PRIu64 ".json",
	  l_mmi.root_path,
	  in_game_id,
	  in_mod_id);
	if (fsu_ptype(path) != FSU_PATHTYPE_FILE)
	{
		free(path);
		return false;
	}
	fsu_rmfile(path);
	free(path);

	// check if the mod was stored as zip
	asprintf(
	  &path,
	  "%s/mods/%" PRIu64 "/%" PRIu64 ".zip",
	  l_mmi.root_path,
	  in_game_id,
	  in_mod_id);
	if (fsu_ptype(path) == FSU_PATHTYPE_FILE)
	{
		fsu_rmfile(path);
	}
	free(path);

	// finally and probably redundantly check for dir
	asprintf(
	  &path,
	  "%s/mods/%" PRIu64 "/%" PRIu64,
	  l_mmi.root_path,
	  in_game_id,
	  in_mod_id);
	if (fsu_ptype(path) == FSU_PATHTYPE_DIR)
	{
		fsu_rmdir_recursive(path);
	}
	free(path);

	return true;
}


struct enum_data
{
	minimod_enum_installed_mods_callback callback;
	void *userdata;
	uint64_t game_id;
};


static bool
is_str_numeric(char const *str, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		if (!isdigit(str[i]))
		{
			return false;
		}
	}
	return true;
}


static void
game_enumerator(
  char const *root,
  char const *name,
  bool is_dir,
  void *in_userdata)
{
	struct enum_data *edata = in_userdata;

	if (is_dir)
	{
		return;
	}

	size_t l = strlen(name);
	if (l > 5 && 0 == strcmp(name + l - 5, ".json") && is_str_numeric(name, l - 5))
	{
		printf("found mod: %s %s\n", root, name);
		uint64_t mod_id = strtoul(name, NULL, 10);
		printf("mod_id: %" PRIu64 "\n", mod_id);
		char *path = NULL;
		asprintf(&path, "%s%" PRIu64 ".zip", root, mod_id);
		if (fsu_ptype(path) == FSU_PATHTYPE_FILE)
		{
			edata->callback(edata->userdata, edata->game_id, mod_id, path);
		}
		else
		{
			free(path);
			asprintf(&path, "%s%" PRIu64 "/", root, mod_id);
			edata->callback(edata->userdata, edata->game_id, mod_id, path);
		}
		free(path);
	}
}


static void
root_enumerator(
  char const *root,
  char const *name,
  bool is_dir,
  void *in_userdata)
{
	struct enum_data *edata = in_userdata;

	if (is_dir)
	{
		printf("[mm] found dir: %s - %s\n", root, name);
		size_t l = strlen(name);
		bool is_game_id = is_str_numeric(name, l);
		if (is_game_id)
		{
			edata->game_id = strtoull(name, NULL, 10);
			char *path;
			asprintf(&path, "%s%s/", root, name);
			fsu_enum_dir(path, game_enumerator, edata);
			free(path);
		}
	}
}


void
minimod_enum_installed_mods(
  uint64_t in_game_id,
  minimod_enum_installed_mods_callback in_callback,
  void *in_userdata)
{
	struct enum_data edata;
	edata.callback = in_callback;
	edata.userdata = in_userdata;
	edata.game_id = in_game_id;

	char *path;
	if (in_game_id)
	{
		asprintf(&path, "%s/mods/%" PRIu64 "/", l_mmi.root_path, in_game_id);
		printf("[mm] path-wid: %s\n", path);
		fsu_enum_dir(path, game_enumerator, &edata);
	}
	else
	{
		asprintf(&path, "%s/mods/", l_mmi.root_path);
		printf("[mm] path-noid: %s\n", path);
		fsu_enum_dir(path, root_enumerator, &edata);
	}
	free(path);
}


bool
minimod_get_installed_mod(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_get_mods_callback in_callback,
  void *userdata)
{
	// TODO
	return false;
}


bool
minimod_is_installed(uint64_t in_game_id, uint64_t in_mod_id)
{
	// check if a json file exists. if it does not, then there is no mod either
	char *path;
	asprintf(
	  &path,
	  "%s/mods/%" PRIu64 "/%" PRIu64 ".json",
	  l_mmi.root_path,
	  in_game_id,
	  in_mod_id);
	return (fsu_ptype(path) == FSU_PATHTYPE_FILE);
}


bool
minimod_is_downloading(uint64_t in_game_id, uint64_t in_mod_id)
{
	// TODO
	return false;
}


void
minimod_rate(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  int in_rating,
  minimod_rate_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);
	assert(in_rating != 0);
	assert(minimod_is_authenticated());

	char *path = NULL;
	asprintf(
	  &path,
	  "%s/games/%" PRIu64 "/mods/%" PRIu64 "/ratings",
	  endpoints[l_mmi.env],
	  in_game_id,
	  in_mod_id);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Content-Type", "application/x-www-form-urlencoded",
		"Authorization", l_mmi.token_bearer,
		NULL
		// clang-format on
	};

	char const *data = in_rating == 1 ? "rating=1" : "rating=-1";

	struct task *task = alloc_task();
	task->callback.userdata = in_userdata;
	task->callback.fptr.rate = in_callback;
	if (!netw_request(
	      NETW_VERB_POST,
	      path,
	      headers,
	      data,
	      strlen(data),
	      handle_rate,
	      task))
	{
		free_task(task);
	}

	free(path);
}


void
minimod_get_ratings(
  char const *in_filter,
  minimod_get_ratings_callback in_callback,
  void *in_udata)
{
	assert(minimod_is_authenticated());

	char *path = NULL;
	asprintf(&path, "%s/me/ratings?%s", endpoints[l_mmi.env], in_filter);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Authorization", l_mmi.token_bearer,
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.userdata = in_udata;
	task->callback.fptr.get_ratings = in_callback;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_ratings,
	      task))
	{
		free_task(task);
	}

	free(path);
}


void
minimod_get_subscriptions(
  char const *in_filter,
  minimod_get_mods_callback in_callback,
  void *in_udata)
{
	assert(minimod_is_authenticated());

	char *path = NULL;
	asprintf(&path, "%s/me/subscribed?%s", endpoints[l_mmi.env], in_filter);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Authorization", l_mmi.token_bearer,
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.userdata = in_udata;
	task->callback.fptr.get_mods = in_callback;
	if (!netw_request(
	      NETW_VERB_GET,
	      path,
	      headers,
	      NULL,
	      0,
	      handle_get_mods,
	      task))
	{
		free_task(task);
	}

	free(path);
}


bool
minimod_subscribe(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_subscription_change_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);
	assert(in_mod_id > 0);
	if (!minimod_is_authenticated())
	{
		return false;
	}

	char *path = NULL;
	asprintf(
	  &path,
	  "%s/games/%" PRIu64 "/mods/%" PRIu64 "/subscribe",
	  endpoints[l_mmi.env],
	  in_game_id,
	  in_mod_id);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Authorization", l_mmi.token_bearer,
		"Content-Type", "application/x-www-form-urlencoded",
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.userdata = in_userdata;
	task->callback.fptr.subscription_change = in_callback;
	task->meta64 = in_mod_id;

	netw_request(
	  NETW_VERB_POST,
	  path,
	  headers,
	  NULL,
	  0,
	  handle_subscription_change,
	  task);

	free(path);

	return true;
}


bool
minimod_unsubscribe(
  uint64_t in_game_id,
  uint64_t in_mod_id,
  minimod_subscription_change_callback in_callback,
  void *in_userdata)
{
	assert(in_game_id > 0);
	assert(in_mod_id > 0);
	if (!minimod_is_authenticated())
	{
		return false;
	}

	char *path = NULL;
	asprintf(
	  &path,
	  "%s/games/%" PRIu64 "/mods/%" PRIu64 "/subscribe",
	  endpoints[l_mmi.env],
	  in_game_id,
	  in_mod_id);

	char const *const headers[] = {
		// clang-format off
		"Accept", "application/json",
		"Authorization", l_mmi.token_bearer,
		"Content-Type", "application/x-www-form-urlencoded",
		NULL
		// clang-format on
	};

	struct task *task = alloc_task();
	task->callback.userdata = in_userdata;
	task->callback.fptr.subscription_change = in_callback;
	task->meta64 = -in_mod_id;

	netw_request(
	  NETW_VERB_DELETE,
	  path,
	  headers,
	  NULL,
	  0,
	  handle_subscription_change,
	  task);

	free(path);

	return true;
}


char const *
minimod_get_more_string(void const *more, char const *name)
{
	assert(QAJ4C_is_object(more));
	QAJ4C_Value const *obj = QAJ4C_object_get(more, name);
	return QAJ4C_is_string(obj) ? QAJ4C_get_string(obj) : NULL;
}


int64_t
minimod_get_more_int(void const *more, char const *name)
{
	assert(QAJ4C_is_object(more));
	QAJ4C_Value const *obj = QAJ4C_object_get(more, name);
	return QAJ4C_is_int64(obj) ? QAJ4C_get_int64(obj) : 0;
}


double
minimod_get_more_float(void const *more, char const *name)
{
	assert(QAJ4C_is_object(more));
	QAJ4C_Value const *obj = QAJ4C_object_get(more, name);
	return QAJ4C_is_double(obj) ? QAJ4C_get_double(obj) : 0;
}


bool
minimod_get_more_bool(void const *more, char const *name)
{
	assert(QAJ4C_is_object(more));
	QAJ4C_Value const *obj = QAJ4C_object_get(more, name);
	return QAJ4C_is_bool(obj) ? QAJ4C_get_bool(obj) : 0;
}
