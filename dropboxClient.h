#ifndef DROPBOX_CLIENT_HEADER
#define DROPBOX_CLIENT_HEADER
#include <pthread.h>
#include <netinet/in.h>

typedef struct session {
  char userid[MAXNAME];
  struct sockaddr_in server;
  int connection;    // tcp socket
  pthread_mutex_t connection_mutex;
  int keep_running;
} SESSION;

void send_file(SESSION *user_session, char *filename);
void get_file(SESSION *user_session, char *filename);

#endif
