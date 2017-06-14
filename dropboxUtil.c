#include "dropboxUtil.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>

void serialize_file_info(struct file_info *info, char *buf) {
	memcpy(buf, info->name, MAXNAME);
	buf += MAXNAME;
	memcpy(buf, info->extension, MAXNAME);
	buf += MAXNAME;
	memcpy(buf, info->last_modified, MAXNAME);
	buf += MAXNAME;
	uint32_t size = htonl(info->size);
	memcpy(buf, (char *)&size, 4);
}

void deserialize_file_info(struct file_info *info, char *buf) {
	memcpy(info->name, buf, MAXNAME);
	buf += MAXNAME;
	memcpy(info->extension, buf, MAXNAME);
	buf += MAXNAME;
	memcpy(info->last_modified, buf, MAXNAME);
	buf += MAXNAME;
	uint32_t size;
	memcpy((char *)&size, buf, 4);
	info->size = ntohl(size);
}

void get_file_stats(const char *path, FILE_INFO *file_info) {
	struct stat file_stats;
	stat(path, &file_stats);
	sprintf(file_info->last_modified, "%li", file_stats.st_mtime);
	file_info->size = file_stats.st_size;
}

void set_file_stats(const char *path, const FILE_INFO *file_info) {
	struct utimbuf mod_stats;
	mod_stats.actime = atoi(file_info->last_modified);
	mod_stats.modtime = mod_stats.actime;
	utime(path, &mod_stats);
}

void fprint_file_info(FILE *stream, struct file_info *info) {
	fprintf(stream, "File:\tname=`%s` ext=`%s`\n\tmod=`%s` size=`%d`\n", info->name, info->extension, info->last_modified, info->size);
}
