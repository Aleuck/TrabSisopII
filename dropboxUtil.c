#include "dropboxUtil.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>

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

void fprint_file_info(FILE *stream, struct file_info *info) {
	fprintf(stream, "%s\n", info->name);
	fprintf(stream, "%s\n", info->extension);
	fprintf(stream, "%s\n", info->last_modified);
	fprintf(stream, "%d\n", info->size);
}
