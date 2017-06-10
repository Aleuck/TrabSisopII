#ifndef DROPBOX_CLIENT_HEADER
#define DROPBOX_CLIENT_HEADER
#include <pthread.h>

typedef struct session {
  char userid[MAXNAME];
  struct sockaddr_in server;
  int connecion;    // tcp socket
  pthread_mutex_t connection_mutex;
} SESSION;

#endif
