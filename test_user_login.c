#include <stdlib.h>
#include <unistd.h>
#include "user_login.h"
#include "dropboxUtil.h"
#include "dropboxServer.h"
#include <pthread.h>

#define N_THREADS 10

void *same_login(void *args) {
	pthread_mutex_t *user_dir_mutex;
	int sockfd = rand();
	char username[MAXNAME];
	usleep(rand()%100000);
	if (login_user(sockfd, username, &user_dir_mutex) == 0) {
		usleep(rand()%100000);
		logout_user(sockfd, username);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	char username[MAXNAME];
	pthread_mutex_t *user_dir_mutex;

	ul_init();

	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (login_user(1, username, &user_dir_mutex) == 0) {
			logout_user(1, username);
		}
	}
	fprintf(stderr, "\nUsers logged in:\n");
	fprint_logged_users(stderr, 1);

	fprintf(stderr, "\n-----\n\n");

	pthread_t threads[N_THREADS];
	for (i = 0; i < N_THREADS/2; i++) {
		pthread_create(&threads[i], NULL, &same_login, NULL);
	}
	usleep(100000);
	for (i = N_THREADS/2; i < N_THREADS; i++) {
		pthread_create(&threads[i], NULL, &same_login, NULL);
	}

	for (i = 0; i < N_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	fprintf(stderr, "\nUsers logged in:\n");
	fprint_logged_users(stderr, 1);

	fprintf(stderr, "\nTerminating...\n");
	ul_term();

	return 0;
}
