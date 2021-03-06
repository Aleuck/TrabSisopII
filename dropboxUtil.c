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

int get_file_stats(const char *path, FILE_INFO *file_info) {
	struct stat file_stats;
	if (stat(path, &file_stats) == -1) {
		flogwarning("get_file_stats(): stat() failed with %s", path);
		return -1;
	}
	if (!S_ISREG(file_stats.st_mode)) {
		/* not a regular file */
		return -1;
	}
	sprintf(file_info->last_modified, "%li", file_stats.st_mtime);
	file_info->size = file_stats.st_size;
	return 0;
}

void set_file_stats(const char *path, const FILE_INFO *file_info) {
	struct utimbuf mod_stats;
	mod_stats.actime = atol(file_info->last_modified);
	mod_stats.modtime = mod_stats.actime;
	utime(path, &mod_stats);
}

ssize_t send_message(int sock, MESSAGE *msg) {
	char buffer[MSG_BUFFER_SIZE];
	char *cur = buffer;
	int bytes_sent;
	ssize_t total_sent = 0;
	memcpy(cur, &msg->code, 1);
	cur += 1;
	uint32_t length = htonl(msg->length);
	memcpy(cur, (char*) &length, 4);
	cur += 4;
	memcpy(cur, msg->content, MSG_MAX_LENGTH);
	cur = buffer;
	while (total_sent < MSG_BUFFER_SIZE) {
		bytes_sent = send(sock, cur, MSG_BUFFER_SIZE - total_sent, 0);
		if (bytes_sent == 0) {
			return 0;
		}
		if (bytes_sent < 0) {
			return -1;
		}
		total_sent += bytes_sent;
		cur = buffer+total_sent;
	}
	return total_sent;
}

ssize_t recv_message(int sock, MESSAGE *msg) {
	char buffer[MSG_BUFFER_SIZE];
	char *cur = buffer;
	int bytes_received = 0;
	ssize_t total_received = 0;
	while (bytes_received < MSG_BUFFER_SIZE) {
		bytes_received = recv(sock, cur, MSG_BUFFER_SIZE - total_received,0);
		if (bytes_received == 0) {
			return 0;
		}
		if (bytes_received < 0) {
			return -1;
		}
		total_received += bytes_received;
		cur = buffer+total_received;
	}
	cur = buffer;
	memcpy(&msg->code, cur, 1);
	cur += 1;
	uint32_t length;
	memcpy((char *)&length, cur, 4);
	msg->length = ntohl(length);
	cur += 4;
	memcpy(msg->content, cur, MSG_MAX_LENGTH);
	return total_received;
}

void fprint_file_info(FILE *stream, struct file_info *info) {
	fprintf(stream, " `%20s`  size: %9d   mod: %10li\n", info->name, info->size, atol(info->last_modified));
}
