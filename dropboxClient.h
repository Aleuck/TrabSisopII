#ifndef DROPBOX_CLIENT_HEADER
#define DROPBOX_CLIENT_HEADER
#include <pthread.h>
#include <netinet/in.h>
#include "linked_list.h"

#define FILE_INFO_BUFLEN (3*MAXNAME + 4)

typedef struct session {
  char userid[MAXNAME];
  struct sockaddr_in server;
  struct linked_list files;
  int connection;    // tcp socket
  pthread_mutex_t connection_mutex;
  int keep_running;
} SESSION;

void end_session(SESSION * user_session);
void send_file(SESSION *user_session, char *filename);
void get_file(SESSION *user_session, char *filename, int to_sync_folder);
struct linked_list request_file_list(SESSION *user_session);
char* get_dir_path(const char *userid);

#endif
