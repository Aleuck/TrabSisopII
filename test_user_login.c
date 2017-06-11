#include <stdlib.h>
#include <unistd.h>
#include "user_login.h"
#include "dropboxUtil.h"
#include "dropboxServer.h"
#include <pthread.h>

#define N_THREADS 10

void *same_login(void *args) {
	int sockfd = rand();
	struct user *user;
	usleep(rand()%100000);
	if (login_user(sockfd, &user) == 0) {
		usleep(rand()%100000);
		logout_user(sockfd, user);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	struct user *user;

	ul_init();

	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (login_user(1, &user) == 0) {
			logout_user(1, user);
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

	return 0;
}
