#ifndef __DROPBOX_UTIL
#define __DROPBOX_UTIL

// Maximum file name with null terminator
#define MAXNAME 100

#define REQUEST_LIST      1
#define REQUEST_UPLOAD    2
#define REQUEST_DOWNLOAD  3

typedef struct file_info {
  char name[MAXNAME];
  char extension[MAXNAME];
  char last_modified[MAXNAME];
  int size;
} FILE_INFO;

#endif
