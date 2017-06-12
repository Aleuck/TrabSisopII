#ifndef DROPBOX_UTIL_H
#define DROPBOX_UTIL_H

#include <stdio.h>

// Maximum file name with null terminator
#define MAXNAME 100
#define MAXFILES 10

#define CMD_UPLOAD       1
#define CMD_DOWNLOAD     2
#define CMD_LIST         3
#define CMD_GET_SYNC_DIR 4
#define CMD_EXIT         5
#define CMD_UNDEFINED   -1

#define SEG_SIZE  1250

#define FILE_INFO_BUFLEN (3*MAXNAME + 4)

typedef struct file_info {
  char name[MAXNAME];
  char extension[MAXNAME];
  char last_modified[MAXNAME];
  int size;
} FILE_INFO;

typedef struct request {
  char command;
  FILE_INFO file_info;
} REQUEST;

void serialize_file_info(struct file_info *info, char *buf);
void deserialize_file_info(struct file_info *info, char *buf);
void fprint_file_info(FILE *stream, struct file_info *info);

#endif /* DROPBOX_UTIL_H */
