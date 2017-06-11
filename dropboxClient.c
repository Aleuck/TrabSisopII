#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include "dropboxUtil.h"
#include "dropboxClient.h"

#ifndef MSG_SIZE

#define MSG_SIZE 100

#endif

int create_dir_for(char *user_name) {
  struct stat st = {0};
  const char* home_dir = getenv ("HOME");
  char path [256];
  printf("user name recived = %s\n", user_name);
  sprintf (path, "%s/Desktop/client_dir_%s",home_dir, user_name);
  if (stat(path, &st) == -1) {
    mkdir(path, 07777);
    return 1;
  }

  return 0;
}

int connect_to_server(SESSION *user_session, const char *host, const char *port) {
  // create TCP socket
  user_session->connection = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (user_session->connection < 0) {
    fprintf(stderr, "Error: Could not create socket.\n");
    return 0;
  }

  // Initialize
  memset((char *) &user_session->server, 0, sizeof(user_session->server));
  user_session->server.sin_addr.s_addr = inet_addr(host);
  user_session->server.sin_family = AF_INET;
  user_session->server.sin_port = htons(atoi(port));

  if (connect(user_session->connection, (struct sockaddr *) &user_session->server, sizeof (user_session->server)) < 0) {
    printf("Failed to connect to server.\n");
    return -1;
  }
  return 1;
}

void disconnect(SESSION *user_session) {
  close(user_session->connection);
};

int login(SESSION *user_session) {
  char server_response_byte = 0;
  send(user_session->connection, user_session->userid, MAXNAME, 0);
  if (recv(user_session->connection, &server_response_byte, sizeof(char), 0) == 0){
    printf("Server cap reached\n");
    return 0;
  }
  if (server_response_byte == 1) {
    printf("Login successful.\n");
    return 1;
  }
  if (server_response_byte == -1) {
    printf("Too many connections\n");
    return 0;
  }
  return 0;
}

int main(int argc, char* argv[]) {
  struct stat st = {0};
  int server_response_int, client_request = 1;
  char response_buffer[MSG_SIZE], input_buffer[MSG_SIZE], *user_name;
  pthread_t thread_cli, thread_sync;
  SESSION user_session = {0};

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <fulano> <host> <port>\n", argv[0]);
    exit(-1);
  }

  strncpy(user_session.userid, argv[1], MAXNAME-1);
  if (!connect_to_server(&user_session, argv[2], argv[3])) {
    exit(1);
  }

  if (!login(&user_session)) {
    exit(1);
  }

  pthread_mutex_init(&user_session.connection_mutex, NULL);

  printf("Connected successfully to server: %d\n", server_response_int);
  create_dir_for(user_session.userid);

  // while (client_request != 100) {
  //
  //   scanf("%d", &client_request);
  //
  //   switch (client_request) {
  //     default:
  //       send(user_session.connection, &client_request, sizeof(int), 0);
  //       recv(user_session.connection, &server_response_int, sizeof(int), 0);
  //       printf("server_response %d\n", server_response_int);
  //       break;
  //   }
  //}
  disconnect(&user_session);
  exit(0);
}
