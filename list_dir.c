#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "dropboxUtil.h"
#include "list_dir.h"
#include "linked_list.h"
#include "logging.h"

struct linked_list get_file_list(const char *dir_path) {
	struct linked_list list;
	ll_init(sizeof(struct file_info), &list);

	DIR *dp;
	struct dirent *entry;
	dp = opendir(dir_path);
	if (dp == NULL) {
		flogwarning("get_file_list(): couldn't open directory %s", dir_path);
		return list;
	}

	int dir_path_len = strlen(dir_path);
	char *root = (char *)malloc(dir_path_len + 2);
	strcpy(root, dir_path);
	/* check if dir_path had a trailing slash */
	if (root[dir_path_len - 1] != '/') {
		root[dir_path_len] = '/';
		root[dir_path_len + 1] = '\0';
	}
	flogdebug("get_file_list(): dir is %s", root);

	char *full_path = (char *)malloc(strlen(root) + MAXNAME + 1);
	struct file_info file;
	struct stat statbuf;
	while (entry = readdir(dp)) {
		strcpy(file.name, entry->d_name);
		strcpy(file.extension, "\0");

		strcpy(full_path, root);
		strcat(full_path, file.name);
		if (stat(full_path, &statbuf) == -1) {
			flogwarning("get_file_list(): stat() failed with %s", full_path);
			continue;
		}

		sprintf(file.last_modified, "%lld", (long long)statbuf.st_mtime);
		file.size = statbuf.st_size;

		ll_put(file.name, &file, &list);
	}

	free(full_path);
	free(root);
	closedir(dp);
	return list;
}

void frpint_file_list(FILE *stream, struct linked_list *list) {
	struct ll_item *item = list->first;

	while (item != NULL) {
		struct file_info *info = item->value;

		fprintf(stream, "%s, mod: %s, size: %d\n", info->name, info->last_modified, info->size);

		item = item->next;
	}
}
