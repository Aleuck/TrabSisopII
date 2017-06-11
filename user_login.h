#ifndef USER_LOGIN_H
#define USER_LOGIN_H

#include <pthread.h>
#include <stdio.h>

#include "dropboxUtil.h"

struct user {
	struct client *cli;
	int num_files;
	pthread_mutex_t *cli_mutex;
};

/* Init user login module. */
void ul_init();

/* Log in user at `sockfd`, put the username in `username` and the user's
 * directory mutex in `mutex`.
 *
 * Return 0 if successful, or -1 if login was denied. */
int login_user(int sockfd, struct user **user);

/* Log out user with `username` and free resources associated with that
 * user if he's not logged in from any other devices. */
void logout_user(int sockfd, struct user *user);

void fprint_logged_users(FILE *stream, char detailed);

#endif /* USER_LOGIN_H */
