#ifndef DROPBOX_UTIL_H
#define DROPBOX_UTIL_H

#include <stdio.h>
#include <arpa/inet.h>

// Maximum file name with null terminator
#define MAXNAME 100

#define CMD_UPLOAD       1
#define CMD_DOWNLOAD     2
#define CMD_LIST         3
#define CMD_GET_SYNC_DIR 4
#define CMD_EXIT         5
#define CMD_UNDEFINED   -1

#define TRANSFER_ERROR   255
#define TRANSFER_DECLINE 0
#define TRANSFER_ACCEPT  1
#define TRANSFER_OK      2
#define TRANSFER_END     3

#define MSG_BUFFER_SIZE  5120

#define FILE_INFO_BUFLEN (3*MAXNAME + 4)

typedef struct file_info {
  char name[MAXNAME];
  char extension[MAXNAME];
  char last_modified[MAXNAME];
  uint32_t size;
} FILE_INFO;

// typedef struct request {
//   char command;
//   char file_info[FILE_INFO_BUFLEN];
// } REQUEST;
typedef struct message {
  char code;
  uint32_t length;
  char content[MSG_BUFFER_SIZE];
} MESSAGE;

void serialize_file_info(struct file_info *info, char *buf);
void deserialize_file_info(struct file_info *info, char *buf);
void fprint_file_info(FILE *stream, struct file_info *info);
void get_file_stats(char *path, FILE_INFO *file_info);
void set_file_stats(char *path, FILE_INFO *file_info);
#endif /* DROPBOX_UTIL_H */
