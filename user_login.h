#ifndef USER_LOGIN_H
#define USER_LOGIN_H

#include <pthread.h>
#include <stdio.h>

#include "dropboxUtil.h"

/* Init and terminate user login module. */
void ul_init();
void ul_term();

/* Log in user at `sockfd`, put the username in `username` and the user's
 * directory mutex in `mutex`.
 *
 * Return 0 if successful, or -1 if login was denied. */
int login_user(int sockfd, char username[MAXNAME], pthread_mutex_t **mutex);

/* Log out user with `username` and free resources associated with that
 * user if he's not logged in from any other devices. */
void logout_user(int sockfd, const char *username);

void fprint_logged_users(FILE *stream, char detailed);

#endif /* USER_LOGIN_H */
