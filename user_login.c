#include <string.h>
#include <stdlib.h>

#include "user_login.h"
#include "dropboxServer.h"
#include "dropboxUtil.h"
#include "linked_list.h"
#include "logging.h"

static struct linked_list users;
static pthread_mutex_t users_mutex;

void die(const char *msg) {
	flogcritical("%s", msg);
	exit(1);
}

static struct client *new_client(const char *username) {
		struct client *new = (struct client *)malloc(sizeof(struct client));
		if (new == NULL) {
			die("new_client(): malloc() failed");
		}

		strcpy(new->userid, username);
		int i;
		for (i = 0; i < MAX_LOGIN_COUNT; i++) {
			new->devices[i] = -1;
		}
		new->logged_in = 0;

		return new;
}

static void rand_str(char *dest, size_t length) {
	char charset[] = "ABCDEFGH";

	while (length-- > 0) {
		size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
		*dest++ = charset[index];
	}
	*dest = '\0';
}

static void receive_username(int sockfd, char username[MAXNAME]) {
	char new_username[MAXNAME];
	if(recv(sockfd, new_username, MAXNAME, 0) == 0){
	}
	strcpy(username, new_username);

}

void ul_init() {
	ll_init(sizeof(struct user), &users);
	if (pthread_mutex_init(&users_mutex, NULL) != 0) {
		die("ul_init(): mutex_init() failed.");
	}
}

int login_user(int sockfd, struct user **user) {
	char username[MAXNAME];
	receive_username(sockfd, username);
	pthread_mutex_lock(&users_mutex);

	if (users.length >= MAX_CLIENTS) {
		floginfo("%s access denied. Too many logged in users.", username);
		pthread_mutex_unlock(&users_mutex);
		return -1;
	}
	*user = (struct user *)ll_getref(username, &users);
	if (*user == NULL) {
		struct user logging_user;

		logging_user.cli = new_client(username);

		logging_user.cli_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		if (logging_user.cli_mutex == NULL) {
			die("login_user(): malloc() failed");
		}

		if (pthread_mutex_init(logging_user.cli_mutex, NULL) != 0) {
			die("login_user(): mutex_init() failed.");
		}
		logging_user.num_files = 0;

		ll_put(username, &logging_user, &users);
		*user = ll_getref(username, &users);
	} else if ((*user)->cli->logged_in >= MAX_LOGIN_COUNT) {
		floginfo("%s access denied. Too many simulatneous logins.", username);
		pthread_mutex_unlock(&users_mutex);
		return -1;
	}


	pthread_mutex_lock((*user)->cli_mutex);

	int i;
	for (i = 0; i < MAX_LOGIN_COUNT; i++) {
		if ((*user)->cli->devices[i] == -1) {
			(*user)->cli->devices[i] = sockfd;
			break;
		}
	}
	(*user)->cli->logged_in++;
	pthread_mutex_unlock((*user)->cli_mutex);
	floginfo("User %s logged in at %d.", username, sockfd);
	pthread_mutex_unlock(&users_mutex);

	return 0;
}

void logout_user(int sockfd, struct user *user) {
	pthread_mutex_lock(&users_mutex);
	pthread_mutex_lock(user->cli_mutex);

	int i = 0;
	while (i < MAX_LOGIN_COUNT && sockfd != user->cli->devices[i]) {
		i++;
	}

	if (i == MAX_LOGIN_COUNT) {
		flogwarning("Tried to logout %s at %d, and the user wasn't logged in.", user->cli->userid, sockfd);
		pthread_mutex_unlock(user->cli_mutex);
		pthread_mutex_unlock(&users_mutex);
		return;
	}

	user->cli->devices[i] = -1;
	user->cli->logged_in--;

	floginfo("User %s at %d logged out.", user->cli->userid, sockfd);

	pthread_mutex_unlock(user->cli_mutex);
	pthread_mutex_unlock(&users_mutex);
}

static void fprint_client(FILE *stream, struct client *cli) {
	int i;
	fprintf(stream, "userid: %s\n", cli->userid);
	fprintf(stream, "logged_in: %d\n", cli->logged_in);
	fprintf(stream, "devices: ");
	for (i = 0; i < MAX_LOGIN_COUNT; i++) {
		fprintf(stream, "%d ", cli->devices[i]);
	}
	fprintf(stream, "\n");
}

void fprint_logged_users(FILE *stream, char detailed) {
	pthread_mutex_lock(&users_mutex);

	if (!detailed) {
		ll_fprint(stream, &users);
	} else {
		struct ll_item *item = users.first;
		while (item != NULL) {
			fprint_client(stream, ((struct user *)item->value)->cli);
			fprintf(stderr, "\n");
			item = item->next;
		}
	}

	pthread_mutex_unlock(&users_mutex);
}
