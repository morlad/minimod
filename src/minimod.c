// vi: noexpandtab tabstop=4 softtabstop=4 shiftwidth=0 list
#include "minimod/minimod.h"
#include "netw.h"
#include "util.h"
#include "qajson4c/src/qajson4c/qajson4c.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


enum task_type
{
	MINIMOD_TASKTYPE_GET_GAMES,
	MINIMOD_TASKTYPE_GET_MODS,
	MINIMOD_TASKTYPE_EMAIL_REQUEST,
	MINIMOD_TASKTYPE_EMAIL_EXCHANGE,
	MINIMOD_TASKTYPE_GET_USERS,
	MINIMOD_TASKTYPE_GET_MODFILES,
	MINIMOD_TASKTYPE_DOWNLOAD,
	MINIMOD_TASKTYPE__COUNT
};


struct callback
{
	union
	{
		minimod_get_games_fptr get_games;
		minimod_get_mods_fptr get_mods;
		minimod_email_request_fptr email_request;
		minimod_email_exchange_fptr email_exchange;
		minimod_get_users_fptr get_users;
		minimod_get_modfiles_fptr get_modfiles;
		minimod_download_fptr download;
		minimod_install_fptr install;
	} fptr;
	void *userdata;
};


struct task
{
	struct callback callback;

	uint64_t meta64;

	enum task_type type;
	int32_t meta32;
};


static struct mmi
{
	char *api_key;
	char *root_path;
	char *cache_tokenpath;
	uint64_t game_id;
	char *token;
	enum minimod_environment env;
	bool unzip;
	char _padding[3];
} l_mmi;


static char const *endpoints[2] =
{
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

//	how to handle callbacks?
//
//	way 1: call once for each list-item + terminator
//		should also include some kind of initiator, telling number of
//		items to pre-alloc in client app.
//		also: what happens when get_games() is called twice?
//		there is no way to differentiate from where the call came from,
//		but it is the same as with #2. only a different granularity
//	way 2: call once with pregenerated list
//		Is nice for the calling code, but requires the library to
//		allocate and transform all of the data. But this is what is
//		needed mostly, because otherwise a filter could be used, reducing
//		not only the size, but also the transfer.
//
//		#2: - No function call overhead
//			- No initiator + terminator
//			- Calling code can quickly iterate over data
//		#1: - Calling code does not have to convert all data.
//			- But an early out is quite rare, otherwise a filter could
//			  have been used to further limit the response.
//		So I'll go with #2, but try to keep as much data in a single
//		chunk as possible, with a sane layout, so client could just
//		reuse the layout, copy and pasting everything instead of having
//		to duplicate everything.
//		In other words: strings or references use indices/offsets to point
//		into another set of memory, so when copying all those chunks
//		no fix-up for pointers is necessary.
//		But there is the necessity to dynamically resize those buffers.
//		Resulting in memory allocations.


static void
handle_get_games(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error)
{
	if (error != 200)
	{
		in_callback.fptr.get_games(in_callback.userdata, 0, NULL);
		return;
	}

	size_t nbuffer = QAJ4C_calculate_max_buffer_size_n(in_data, in_len);
	void *buffer = malloc(nbuffer);

	QAJ4C_Value const *document = NULL;
	QAJ4C_parse_opt(in_data, in_len, 0, buffer, nbuffer, &document);
	assert(QAJ4C_is_object(document));

	QAJ4C_Value const *data = QAJ4C_object_get(document, "data");
	assert(QAJ4C_is_array(data));

	size_t ngames = QAJ4C_array_size(data);
	struct minimod_game *games = malloc(sizeof *games * ngames);

	for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
	{
		QAJ4C_Value const *item = QAJ4C_array_get(data, i);
		assert(QAJ4C_is_object(item));

		games[i].id = QAJ4C_get_uint(QAJ4C_object_get(item, "id"));
		games[i].name = QAJ4C_get_string(QAJ4C_object_get(item, "name"));

	}

	in_callback.fptr.get_games(in_callback.userdata, ngames, games);

	free(games);
	free(buffer);
}


static void
populate_mod(struct minimod_mod *mod, QAJ4C_Value const *node)
{
	assert(mod);

	assert(node);
	assert(QAJ4C_is_object(node));

	mod->id = QAJ4C_get_uint(QAJ4C_object_get(node, "id"));
	mod->name = QAJ4C_get_string(QAJ4C_object_get(node, "name"));

	QAJ4C_Value const *modfile = QAJ4C_object_get(node, "modfile");
	assert(QAJ4C_is_object(modfile));

	QAJ4C_Value const *modfile_id = QAJ4C_object_get(modfile, "id");
	if (modfile_id)
	{
		QAJ4C_is_uint64(modfile_id);
		mod->modfile_id = QAJ4C_get_uint64(modfile_id);
	}
}


static void
handle_get_mods(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error)
{
	if (error != 200)
	{
		in_callback.fptr.get_mods(in_callback.userdata, 0, NULL);
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

		in_callback.fptr.get_mods(in_callback.userdata, nmods, mods);

		free(mods);
	}
	else
	{
		struct minimod_mod mod = { 0 };
		populate_mod(&mod, document);
		in_callback.fptr.get_mods(in_callback.userdata, 1, &mod);
	}
	free(buffer);
}


static void
handle_get_users(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error)
{
	if (error != 200)
	{
		in_callback.fptr.get_mods(in_callback.userdata, 0, NULL);
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
		struct minimod_user *users = malloc(sizeof *users * nusers);

		for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
		{
			QAJ4C_Value const *item = QAJ4C_array_get(data, i);
			assert(QAJ4C_is_object(item));

		}

		in_callback.fptr.get_users(in_callback.userdata, nusers, users);

		free(users);
	}
	// single user
	else
	{
		struct minimod_user user;
		user.id = QAJ4C_get_uint(QAJ4C_object_get(document, "id"));
		user.name = QAJ4C_get_string(QAJ4C_object_get(document, "username"));

		in_callback.fptr.get_users(in_callback.userdata, 1, &user);
	}
	free(buffer);
}


static void
populate_modfile(struct minimod_modfile *modfile, QAJ4C_Value const *node)
{
	assert(modfile);

	assert(node);
	assert(QAJ4C_is_object(node));

	modfile->id = QAJ4C_get_uint64(QAJ4C_object_get(node, "id"));
	modfile->filesize = QAJ4C_get_uint64(QAJ4C_object_get(node, "filesize"));

	QAJ4C_Value const *filehash = QAJ4C_object_get(node, "filehash");
	modfile->md5 = QAJ4C_get_string(QAJ4C_object_get(filehash, "md5"));

	QAJ4C_Value const *download = QAJ4C_object_get(node, "download");
	modfile->url = QAJ4C_get_string(QAJ4C_object_get(download, "binary_url"));
}


static void
handle_get_modfiles(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error)
{
	if (error != 200)
	{
		in_callback.fptr.get_modfiles(in_callback.userdata, 0, NULL);
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
		struct minimod_modfile *modfiles = malloc(sizeof *modfiles * nmodfiles);

		for (size_t i = 0; i < QAJ4C_array_size(data); ++i)
		{
			populate_modfile(&modfiles[i], QAJ4C_array_get(data, i));
		}

		in_callback.fptr.get_modfiles(in_callback.userdata, nmodfiles, modfiles);

		free(modfiles);
	}
	else
	{
		struct minimod_modfile modfile;
		populate_modfile(&modfile, document);
		in_callback.fptr.get_modfiles(in_callback.userdata, 1, &modfile);
	}
	free(buffer);
}


static void
handle_email_request(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error)
{
	in_callback.fptr.email_request(in_callback.userdata, error == 200);
}


static void
handle_email_exchange(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error)
{
	if (error != 200)
	{
		in_callback.fptr.email_exchange(in_callback.userdata, NULL, 0);
	}

	// extract token
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

	in_callback.fptr.email_exchange(in_callback.userdata, tok, tok_bytes);
}


static void
handle_download(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error)
{
	printf("[mm] Downloaded a file %s\n", (char const *)in_data);
	in_callback.fptr.download(in_callback.userdata, in_data);
}


typedef void (*handler)(
	struct callback const in_callback,
	void const *in_data,
	size_t in_len,
	int error);


static handler l_handlers[MINIMOD_TASKTYPE__COUNT] =
{
	[MINIMOD_TASKTYPE_GET_GAMES] = handle_get_games,
	[MINIMOD_TASKTYPE_GET_MODS] = handle_get_mods,
	[MINIMOD_TASKTYPE_EMAIL_REQUEST] = handle_email_request,
	[MINIMOD_TASKTYPE_EMAIL_EXCHANGE] = handle_email_exchange,
	[MINIMOD_TASKTYPE_GET_USERS] = handle_get_users,
	[MINIMOD_TASKTYPE_GET_MODFILES] = handle_get_modfiles,
	[MINIMOD_TASKTYPE_DOWNLOAD] = handle_download,
};


static void
on_completion(void const *in_udata, void const *data, size_t bytes, int error)
{
	if (error != 200)
	{
		printf("[mm] on_completion(%i):\n%s\n--\n", error, (char const *)data);
	}
	FILE *f = fsu_fopen("tmp.json", "wb");
	fwrite(data, bytes, 1, f);
	fclose(f);

	assert(in_udata);
	struct task const *task = in_udata;
	if (task->type != MINIMOD_TASKTYPE_DOWNLOAD)
	{
		l_handlers[task->type](task->callback, data, bytes, error);
	}
}


static void
on_downloaded(void const *in_udata, char const *path, int error)
{
	if (error != 200)
	{
		printf("[mm] on_downloaded(%i)\n", error);
	}

	printf("[mm] on_downloaded(%i): %s\n", error, path);

	assert(in_udata);
	struct task const *task = in_udata;
	l_handlers[task->type](task->callback, path, 0, error);
}


bool
minimod_init(
	enum minimod_environment env,
	uint32_t game_id,
	char const *api_key,
	char const *root_path)
{
	struct netw_callbacks callbacks;
	callbacks.completion = on_completion;
	callbacks.downloaded = on_downloaded;
	if (!netw_init(&callbacks))
	{
		return false;
	}

	l_mmi.env = env;
	l_mmi.game_id = game_id;
	l_mmi.root_path = strdup(root_path ? root_path : "_minimod");
	// TODO make sure the path does not end with '/'
	l_mmi.api_key = api_key ? strdup(api_key) : NULL;

	// attempt to load token
	int64_t fsize = fsu_fsize(get_tokenpath());
	if (fsize > 0)
	{
		FILE *f = fsu_fopen(get_tokenpath(), "rb");
		l_mmi.token = malloc((size_t)(fsize + 1));
		l_mmi.token[fsize] = '\0';
		fread(l_mmi.token, (size_t)fsize, 1, f);
		fclose(f);
	}

	return true;
}


void
minimod_deinit()
{
	netw_deinit();

	free(l_mmi.root_path);
	free(l_mmi.cache_tokenpath);
	free(l_mmi.api_key);
	free(l_mmi.token);

	l_mmi = (struct mmi){0};
}


void
minimod_get_games(
	char const *in_filter,
	minimod_get_games_fptr in_callback,
	void *in_udata)
{
	char *path;
	asprintf(&path, "%s/games?api_key=%s&%s",
		endpoints[l_mmi.env],
		l_mmi.api_key,
		in_filter ? in_filter : "");
	char const *const headers[] = {
		"Accept", "application/json",
		NULL
	};

	struct task *task = alloc_task();
	if (netw_get_request(path, headers, task))
	{
		task->type = MINIMOD_TASKTYPE_GET_GAMES;
		task->callback.fptr.get_games = in_callback;
		task->callback.userdata = in_udata;
	}
	else
	{
		free_task(task);
	}

	free(path);
}


void
minimod_get_mods(
	char const *in_filter,
	uint64_t in_gameid,
	minimod_get_mods_fptr in_callback,
	void *in_udata)
{
	char *path;
	asprintf(&path, "%s/games/%llu/mods?api_key=%s&%s",
		endpoints[l_mmi.env],
		in_gameid == 0 ? l_mmi.game_id : in_gameid,
		l_mmi.api_key,
		in_filter ? in_filter : "");

	char const *const headers[] = {
		"Accept", "application/json",
		NULL
	};

	struct task *task = alloc_task();
	if (netw_get_request(path, headers, task))
	{
		task->type = MINIMOD_TASKTYPE_GET_MODS;
		task->callback.fptr.get_mods = in_callback;
		task->callback.userdata = in_udata;
	}
	else
	{
		free_task(task);
	}

	free(path);
}


void
minimod_email_request(
	char const *in_email,
	minimod_email_request_fptr in_callback,
	void *in_udata)
{
	char *path;
	asprintf(&path, "%s/oauth/emailrequest", endpoints[l_mmi.env]);

	char const *const headers[] = {
		"Accept", "application/json",
		"Content-Type", "application/x-www-form-urlencoded",
		NULL
	};

	char *payload;
	char *email = netw_percent_encode(in_email, strlen(in_email), NULL);
	int nbytes = asprintf(&payload, "api_key=%s&email=%s", l_mmi.api_key, email);
	free(email);
	printf("[mm] payload: %s (%i)\n", payload, nbytes);

	assert(nbytes > 0);

	struct task *task = alloc_task();
	if (netw_post_request(path, headers, payload, (size_t)nbytes, task))
	{
		task->type = MINIMOD_TASKTYPE_EMAIL_REQUEST;
		task->callback.fptr.email_request = in_callback;
		task->callback.userdata = in_udata;
	}
	else
	{
		free_task(task);
	}

	free(payload);
	free(path);
}


void
minimod_email_exchange(
	char const *in_code,
	minimod_email_exchange_fptr in_callback,
	void *in_udata)
{
	char *path;
	asprintf(&path, "%s/oauth/emailexchange", endpoints[l_mmi.env]);

	char const *const headers[] = {
		"Accept", "application/json",
		"Content-Type", "application/x-www-form-urlencoded",
		NULL
	};

	char *payload;
	int nbytes = asprintf(&payload, "api_key=%s&security_code=%s", l_mmi.api_key, in_code);
	printf("[mm] payload: %s (%i)\n", payload, nbytes);

	assert(nbytes > 0);

	struct task *task = alloc_task();
	if (netw_post_request(path, headers, payload, (size_t)nbytes, task))
	{
		task->type = MINIMOD_TASKTYPE_EMAIL_EXCHANGE;
		task->callback.fptr.email_exchange = in_callback;
		task->callback.userdata = in_udata;
	}
	else
	{
		free_task(task);
	}

	free(payload);
	free(path);
}


void
minimod_get_user(
	uint64_t in_uid,
	minimod_get_users_fptr in_callback,
	void *in_udata)
{
	char *path;
	char *auth_field = NULL;
	char *auth_value = NULL;
	if (in_uid == 0)
	{
		assert(l_mmi.token);
		asprintf(&path, "%s/me", endpoints[l_mmi.env]);
		auth_field = "Authorization";
		asprintf(&auth_value, "Bearer %s", l_mmi.token);
	}
	else
	{
		asprintf(&path, "%s/users/%llu", endpoints[l_mmi.env], in_uid);
	}

	char const * const headers[] = {
		"Accept", "application/json",
		auth_field, auth_value,
		NULL
	};

	struct task *task = alloc_task();
	if (netw_get_request(path, headers, task))
	{
		task->type = MINIMOD_TASKTYPE_GET_USERS;
		task->callback.fptr.get_users = in_callback;
		task->callback.userdata = in_udata;
	}
	else
	{
		free_task(task);
	}

	free(path);
}


bool
minimod_is_authenticated(void)
{
	return l_mmi.token;
}


void
minimod_deauthenticate(void)
{
	fsu_rmfile(get_tokenpath());
	free(l_mmi.token);
	l_mmi.token = NULL;
}


MINIMOD_LIB void
minimod_get_modfiles(
	char const *in_filter,
	uint64_t in_gameid,
	uint64_t in_modid,
	uint64_t in_modfileid,
	minimod_get_modfiles_fptr in_callback,
	void *in_udata)
{
	char *path;
	if (in_modfileid)
	{
		asprintf(&path, "%s/games/%llu/mods/%llu/files/%llu?api_key=%s&%s",
			endpoints[l_mmi.env],
			in_gameid == 0 ? l_mmi.game_id : in_gameid,
			in_modid,
			in_modfileid,
			l_mmi.api_key,
			in_filter ? in_filter : "");
	}
	else
	{
		asprintf(&path, "%s/games/%llu/mods/%llu/files?api_key=%s&%s",
			endpoints[l_mmi.env],
			in_gameid == 0 ? l_mmi.game_id : in_gameid,
			in_modid,
			l_mmi.api_key,
			in_filter ? in_filter : "");
	}
	printf("[mm] request: %s\n", path);

	char const *const headers[] = {
		"Accept", "application/json",
		NULL
	};

	struct task *task = alloc_task();
	if (netw_get_request(path, headers, task))
	{
		task->type = MINIMOD_TASKTYPE_GET_MODFILES;
		task->callback.fptr.get_modfiles = in_callback;
		task->callback.userdata = in_udata;
	}
	else
	{
		free_task(task);
	}

	free(path);
}


#define DOWNLOAD_URI_SIZE 2048


static void
on_download_modfile(
	void *udata,
	size_t nmodfiles,
	struct minimod_modfile const *modfiles)
{
	assert(nmodfiles == 1);

	char *uri = (char *)udata;
	strncpy(uri, modfiles[0].url, DOWNLOAD_URI_SIZE);
	// make sure string is NUL terminated (this also unblocks minimod_download)
	uri[DOWNLOAD_URI_SIZE - 1] = 0;
}


void
minimod_download(
	uint64_t in_gameid,
	uint64_t in_modid,
	uint64_t in_modfileid,
	minimod_download_fptr in_callback,
	void *in_udata)
{
	// fetch meta-data
	char uri[DOWNLOAD_URI_SIZE] = { 0 };
	uri[sizeof uri - 1] = 1;
	minimod_get_modfiles(
		NULL,
		in_gameid,
		in_modid,
		in_modfileid,
		on_download_modfile,
		&uri);

	while (uri[sizeof uri - 1])
	{
		sys_sleep(10);
	}

	// meta data received
	printf("[mm] download-url: %s\n", uri);
	struct task *task = alloc_task();
	if (netw_download(uri, task))
	{
		task->type = MINIMOD_TASKTYPE_DOWNLOAD;
		task->callback.fptr.download = in_callback;
		task->callback.userdata = in_udata;
	}
	else
	{
		free_task(task);
	}
}


static void
on_install_download(void *in_udata, char const *in_path)
{
	struct task *task = in_udata;

	char *path = NULL;
	// extract zip?
	if (l_mmi.unzip)
	{
		// unzip it
	}
	else
	{
		// todo move file
		asprintf(&path, "%s/mods/%llu.zip", l_mmi.root_path, task->meta64);
		printf("[mm] installing mod to %s\n", path);
		// always overwrites
		if (fsu_mvfile(in_path, path, true))
		{
			printf("[mm] file moved\n");
		}	
		else
		{
			printf("[mm] file NOT moved\n");
		}
	}

	// callback
	task->callback.fptr.install(task->callback.userdata, path);

	free(path);
	free_task(task);
}


void
minimod_install(
	uint64_t in_gameid,
	uint64_t in_modid,
	uint64_t in_modfileid,
	minimod_install_fptr in_callback,
	void *in_udata)
{
	struct task *task = alloc_task();
	task->callback.fptr.install = in_callback;
	task->callback.userdata = in_udata;
	task->meta64 = in_modid;

	minimod_download(
		in_gameid,
		in_modid,
		in_modfileid,
		on_install_download,
		task);
}
