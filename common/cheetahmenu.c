
#include "cache.h"
#include "exec.h"
#include "menuengine.h"
#include "cheetahmenu.h"
#include "debug.h"
#include "systeminfo.h"

char *wd_from_path(const char *path, BOOL *is_path_dir)
{
	BOOL is_directory = TRUE;
	char *cheetah_wd = strdup(path);
	if (!is_path_directory(cheetah_wd)) {
		char *c = strrchr(cheetah_wd, PATH_SEPERATOR);
		if (c) /* sanity check in case it's a weird directory */
			*c = 0;

		is_directory = FALSE;
	}

	if (is_path_dir)
		*is_path_dir = is_directory;

	return cheetah_wd;
}

/*
 * Cheetah-specific menu
 */

static void menu_gui(struct git_data *this_, UINT id)
{
	char *wd = wd_from_path(this_->name, NULL);
	const char **argv;
	const char *generic_argv[] = { "git", "gui", NULL };

	argv = menu_get_platform_argv(MENU_GUI, NULL);
	if (!argv)
		argv = generic_argv;

	exec_program_v(wd, NULL, NULL, HIDDENMODE, argv);
	free(wd);
}

static void menu_init(struct git_data *this_, UINT id)
{
	char *wd = wd_from_path(this_->name, NULL);
	const char **argv;
	const char *generic_argv[] = { "git", "init", NULL };

	argv = menu_get_platform_argv(MENU_INIT, NULL);
	if (!argv)
		argv = generic_argv;

	exec_program_v(wd, NULL, NULL, HIDDENMODE, argv);
	free(wd);
}

static void menu_history(struct git_data *this_, unsigned int id)
{
	BOOL is_directory;
	char *wd = wd_from_path(this_->name, &is_directory);
	char *name = "";
	const char **argv;
	const char *generic_argv[] = { "gitk", "HEAD", "--",
		NULL, NULL };

	if (!is_directory)
		name = this_->name + strlen(wd) + 1;
	generic_argv[3] = name;

	argv = menu_get_platform_argv(MENU_HISTORY, name);
	if (!argv)
		argv = generic_argv;

	exec_program_v(wd, NULL, NULL, HIDDENMODE, argv);

	free(wd);
}

static void menu_bash(struct git_data *this_, UINT id)
{
	char *wd = wd_from_path(this_->name, NULL);
	const char **argv = NULL;

	argv = menu_get_platform_argv(MENU_BASH, wd);
	/* There is no generic implementation for this item */
	if (!argv)
		return;

	exec_program_v(wd, NULL, NULL, NORMALMODE, argv);

	free(wd);
}

static void menu_blame(struct git_data *this_, UINT id)
{
	BOOL is_directory;
	char *wd = wd_from_path(this_->name, &is_directory);
	char *name = "";
	const char **argv;
	const char *generic_argv[] = { "git", "gui", "blame",
		NULL, NULL };

	if (!is_directory) {
		name = this_->name + strlen(wd) + 1;
		generic_argv[3] = name;

		argv = menu_get_platform_argv(MENU_BLAME, name);
		if (!argv)
			argv = generic_argv;

		exec_program_v(wd, NULL, NULL, HIDDENMODE, argv);
	}

	free(wd);
}

static void menu_citool(struct git_data *this_, UINT id)
{
	char *wd = wd_from_path(this_->name, NULL);
	const char **argv;
	const char *generic_argv[] = { "git", "citool", NULL };

	argv = menu_get_platform_argv(MENU_CITOOL, NULL);
	if (!argv)
		argv = generic_argv;

	exec_program_v(wd, NULL, NULL, HIDDENMODE, argv);
	free(wd);
}

static void menu_addall(struct git_data *this_, UINT id)
{
	char *wd = wd_from_path(this_->name, NULL);
	const char **argv;
	const char *generic_argv[] = { "git", "add", "--all", NULL };

	argv = menu_get_platform_argv(MENU_ADDALL, NULL);
	if (!argv)
		argv = generic_argv;

	exec_program_v(wd, NULL, NULL, HIDDENMODE, argv);
	free(wd);
}

static void menu_branch(struct git_data *this_, UINT id)
{
	int status;
	char *wd = wd_from_path(this_->name, NULL);
	struct strbuf err;
	const char *menu_item_text;
	const char **argv;
	const char *generic_argv[] = { "git", "checkout", NULL, NULL };

	menu_item_text = get_menu_item_text(id);
	generic_argv[2] = menu_item_text;

	argv = menu_get_platform_argv(MENU_BRANCH, menu_item_text);
	if (!argv)
		argv = generic_argv;

	strbuf_init(&err, 0);

	status = exec_program_v(wd, NULL, &err, HIDDENMODE, argv);

	/* if nothing, terribly wrong happened, show the confirmation */
	if (-1 != status)
		/* strangely enough even success message is on stderr */
		debug_git_mbox(err.buf);

	free(wd);
}

static BOOL build_branch_menu(struct git_data *data,
			      const struct menu_item *item,
			      void *platform)
{
	void *submenu;

	int status;
	char *wd = wd_from_path(data->name, NULL);

	struct strbuf output;
	struct strbuf **lines, **it;
	strbuf_init(&output, 0);

	status = exec_program(wd, &output, NULL, WAITMODE,
		"git", "branch", NULL);
	free(wd);
	if (status)
		return FALSE;

	submenu = start_submenu(data, item, platform);

	lines = strbuf_split(&output, '\n');
	for (it = lines; *it; it++) {
		struct menu_item item = {
			MENU_ITEM_CLEANUP,
			NULL, NULL,
			NULL, menu_branch
		};

		strbuf_rtrim(*it);
		item.string = strdup((*it)->buf + 2);
		item.helptext = strdup((*it)->buf + 2);
		if (build_item(data, &item, submenu)) {
			check_menu_item(submenu, '*' == (*it)->buf[0]);
			append_active_menu(item);
		} else
			/*
			 * if the platform failed to create an item
			 * there is no point to try other items
			 */
			break;
	}

	end_submenu(platform, submenu);

	/* technically, there is nothing to track for the menu engine */
	return FALSE;
}

UINT cheetah_menu_mask(struct git_data *this_)
{
	BOOL is_directory;
	char *wd = wd_from_path(this_->name, &is_directory);
	UINT selection = is_directory ? MENU_ITEM_DIR : MENU_ITEM_FILE;
	int status;

	struct strbuf output;
	char *eol;
	strbuf_init(&output, 0);

	status = exec_program(wd, &output, NULL, WAITMODE,
		"git", "rev-parse", "--show-prefix", NULL);
	eol = strchr(output.buf, '\n');
	if (eol)
		*eol = 0;

	if (status < 0) /* something went terribly wrong */
		selection = MENU_ITEM_LAST;
	else if (status)
		selection |= MENU_ITEM_NOREPO;
	else {
		char head_path[MAX_PATH] = "HEAD";
		if (!is_directory)
			sprintf(head_path, "HEAD:%s%s",
				output.buf,
				this_->name + strlen(wd) + 1);

		status = exec_program(wd, NULL, NULL, WAITMODE,
			"git", "rev-parse", "--verify", head_path, NULL);
		if (status < 0) /* something went terribly wrong */
			selection = MENU_ITEM_LAST;
		else
			selection |= MENU_ITEM_REPO |
				(status ?
					MENU_ITEM_NOTRACK : MENU_ITEM_TRACK);
	}

	strbuf_release(&output);
	free(wd);
	return selection;
}

const struct menu_item cheetah_menu[] = {
	{ MENU_ITEM_ALWAYS, NULL, NULL, build_separator, NULL },

	{ MENU_ITEM_REPO, "Git &Add all files now",
		"Add all files from this folder now",
		build_item, menu_addall },
	{ MENU_ITEM_REPO, "Git &Commit Tool",
		"Launch the GIT commit tool in the local or chosen directory.",
		build_item, menu_citool },
	{ MENU_ITEM_TRACK, "Git &History",
		"Show GIT history of the chosen file or directory.",
		build_item,
		menu_history },
	{ MENU_ITEM_TRACK | MENU_ITEM_FILE, "Git &Blame",
		"Start a blame viewer on the specified file.",
		build_item, menu_blame },

	{ MENU_ITEM_REPO, "Git &Gui",
		"Launch the GIT Gui in the local or chosen directory.",
		build_item, menu_gui },

	{ MENU_ITEM_REPO, "Git Bra&nch",
		"Checkout a branch",
		build_branch_menu, NULL },

	{ MENU_ITEM_NOREPO, "Git I&nit Here",
		"Initialize GIT repo in the local directory.",
		build_item, menu_init },
	{ MENU_ITEM_NOREPO | MENU_ITEM_DIR, "Git &Gui",
		"Launch the GIT Gui in the local or chosen directory.",
		build_item, menu_gui },

	{ MENU_ITEM_ALWAYS, "Git Ba&sh",
		"Start GIT shell in the local or chosen directory",
		build_item, menu_bash },
	{ MENU_ITEM_ALWAYS, NULL, NULL, build_separator, NULL },
};

void build_cheetah_menu(struct git_data *data, void *platform_data)
{
	reset_platform(platform_data);
	build_menu_items(data, cheetah_menu_mask,
		cheetah_menu,
		sizeof(cheetah_menu) / sizeof(cheetah_menu[0]),
		platform_data);
}