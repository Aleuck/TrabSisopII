#ifndef DROPBOX_SERVER_H
#define DROPBOX_SERVER_H

#include "dropboxUtil.h"

#define MAX_LOGIN_COUNT 2
#define MAX_CLIENTS 10

struct client {
  int devices[2];
  char userid[MAXNAME];
  int logged_in;
};

#endif /* ifndef DROPBOX_SERVER_H */
