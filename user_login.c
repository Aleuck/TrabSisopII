#include <string.h>
#include <stdlib.h>

#include "dropboxServer.h"
#include "dropboxUtil.h"
#include "user_login.h"
#include "linked_list.h"
#include "logging.h"

static struct linked_list logged_users;
static pthread_mutex_t logged_users_mutex;

void die(const char *msg) {
	flogcritical("%s", msg);
	exit(1);
}

struct logged_user {
	struct client *cli;
	pthread_mutex_t *dir_mutex;
};

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

void rand_str(char *dest, size_t length) {
	char charset[] = "ABCDEFGH";

	while (length-- > 0) {
		size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
		*dest++ = charset[index];
	}
	*dest = '\0';
}

static void receive_username(int sockfd, char username[MAXNAME]) {
	rand_str(username, 1);
}

void ul_init() {
	ll_init(sizeof(struct logged_user), &logged_users);
	if (pthread_mutex_init(&logged_users_mutex, NULL) != 0) {
		die("ul_init(): mutex_init() failed.");
	}
}

void ul_term() {
	pthread_mutex_lock(&logged_users_mutex);

	struct ll_item *item = logged_users.first;
	while (item != NULL) {
		struct client *cli = ((struct logged_user *)item->value)->cli;
		pthread_mutex_t *mutex = ((struct logged_user *)item->value)->dir_mutex;
		int i;
		for (i = 0; i < MAX_LOGIN_COUNT; i++) {
			if (cli->devices[i] != -1) {
				int sockfd = cli->devices[i];
				char username[MAXNAME];
				strcpy(username, cli->userid);

				pthread_mutex_unlock(&logged_users_mutex);
				logout_user(sockfd, username);
				pthread_mutex_lock(&logged_users_mutex);
			}
		}
		free(cli);
		pthread_mutex_destroy(mutex);
		free(mutex);
		ll_del(cli->userid, &logged_users);
		item = item->next;
	}

	pthread_mutex_unlock(&logged_users_mutex);
}

int login_user(int sockfd, char username[MAXNAME], pthread_mutex_t **user_dir_mutex) {
	receive_username(sockfd, username);

	pthread_mutex_lock(&logged_users_mutex);

	if (logged_users.length >= MAX_CLIENTS) {
		floginfo("%s access denied. Too many logged in users.", username);
		pthread_mutex_unlock(&logged_users_mutex);
		return -1;
	}

	struct logged_user *user = ll_getref(username, &logged_users);

	if (user == NULL) {
		struct logged_user logging_user;

		logging_user.cli = new_client(username);

		logging_user.dir_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		if (logging_user.dir_mutex == NULL) {
			die("login_user(): malloc() failed");
		}

		if (pthread_mutex_init(logging_user.dir_mutex, NULL) != 0) {
			die("login_user(): mutex_init() failed.");
		}

		ll_put(username, &logging_user, &logged_users);
		user = ll_getref(username, &logged_users);
	} else if (user->cli->logged_in >= MAX_LOGIN_COUNT) {
		floginfo("%s access denied. Too many simulatneous logins.", username);
		pthread_mutex_unlock(&logged_users_mutex);
		return -1;
	}

	*user_dir_mutex = user->dir_mutex;
	int i;
	for (i = 0; i < MAX_LOGIN_COUNT; i++) {
		if (user->cli->devices[i] == -1) {
			user->cli->devices[i] = sockfd;
			break;
		}
	}
	user->cli->logged_in++;

	floginfo("User %s logged in at %d.", username, sockfd);

	pthread_mutex_unlock(&logged_users_mutex);

	return 0;
}

void logout_user(int sockfd, const char *username) {
	pthread_mutex_lock(&logged_users_mutex);

	struct logged_user *user = ll_getref(username, &logged_users);

	if (user == NULL) {
		flogwarning("Tried to logout %s at %d, and the user wasn't logged in.", username, sockfd);
		pthread_mutex_unlock(&logged_users_mutex);
		return;
	}

	int i = 0;
	while (i < MAX_LOGIN_COUNT && sockfd != user->cli->devices[i]) {
		i++;
	}

	if (i == MAX_LOGIN_COUNT) {
		flogwarning("Tried to logout %s at %d, and the user wasn't logged in.", username, sockfd);
		pthread_mutex_unlock(&logged_users_mutex);
		return;
	}

	user->cli->devices[i] = -1;
	user->cli->logged_in--;

	pthread_mutex_unlock(&logged_users_mutex);

	floginfo("User %s at %d logged out.", username, sockfd);
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
	pthread_mutex_lock(&logged_users_mutex);

	if (!detailed) {
		ll_fprint(stream, &logged_users);
	} else {
		struct ll_item *item = logged_users.first;
		while (item != NULL) {
			fprint_client(stream, ((struct logged_user *)item->value)->cli);
			fprintf(stderr, "\n");
			item = item->next;
		}
	}

	pthread_mutex_unlock(&logged_users_mutex);
}
