#ifndef DROPBOX_UTIL_H
#define DROPBOX_UTIL_H

// Maximum file name with null terminator
#define MAXNAME 100

#define CMD_UPLOAD       1
#define CMD_DOWNLOAD     2
#define CMD_LIST         3
#define CMD_GET_SYNC_DIR 4
#define CMD_EXIT         5

#define SEG_SIZE  1250

typedef struct file_info {
  char name[MAXNAME];
  char extension[MAXNAME];
  char last_modified[MAXNAME];
  int size;
} FILE_INFO;

#endif /* DROPBOX_UTIL_H */
