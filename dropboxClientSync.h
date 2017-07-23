#ifndef DROPBOX_CLIENT_SYNC_HEADER
#define DROPBOX_CLIENT_SYNC_HEADER

time_t getTimeServer(SESSION* user_session);

void *client_sync(void *s_arg);

#endif
