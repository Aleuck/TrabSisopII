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
#include "dropboxClientCli.h"

#ifndef MSG_SIZE

#define MSG_SIZE 100

#endif

int create_dir_for(char *user_name) {
  struct stat st = {0};
  const char* home_dir = getenv ("HOME");
  char path [256];
  printf("user name recived = %s\n", user_name);
  sprintf (path, "%s/sync_dir_%s",home_dir, user_name);
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

void disconnect_from_server(SESSION *user_session) {
  close(user_session->connection);
};

int login(SESSION *user_session) {
  char server_response_byte = 0;
  int size_received;
  send(user_session->connection, user_session->userid, MAXNAME, 0);
  if ((size_received = recv(user_session->connection, &server_response_byte, sizeof(char), 0)) == 0){
    printf("Server cap reached\n");
    return 0;
  }
  fprintf(stderr, "size_received : %d, size of buffer: %d\n",size_received, sizeof(char) );
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

void *client_sync(void *s_arg) {
  SESSION *user_session = (SESSION *) s_arg;
  while (user_session->keep_running) {
    sleep(5);
  }
  return 0;
}

void init_session(SESSION * user_session) {
  pthread_mutex_init(&user_session->connection_mutex, NULL);
  user_session->keep_running = 1;
}

void end_session(SESSION * user_session) {
  // this will signal threads to stop running as soon as they can;
  user_session->keep_running = 0;
}

void send_file(SESSION *user_session, char *filename) {
  char buffer[SEG_SIZE], name_aux[100];
  REQUEST client_request;
  ssize_t sent_size, aux_print;
  FILE *file_handler;
  FILE_INFO file_to_send;
  const char s[2] = "/";
  char *token;
  strcpy(name_aux, filename);
  fprintf(stderr, "%s\n", name_aux);
  token = strtok(name_aux, s);
   while( token != NULL )
   {
      strcpy(file_to_send.name,token);
      token = strtok(NULL, s);

   }
  fprintf(stderr, "%s\n", file_to_send.name);
  client_request.command = CMD_UPLOAD;
  client_request.file_info = file_to_send;
  send(user_session->connection,(char *)&client_request,sizeof(client_request),0);
  if ((file_handler = fopen(filename, "r")) == NULL) {
        printf("Error sending the file to server\n");
        return;
    }

    while ((sent_size = fread(buffer, 1,sizeof(buffer), file_handler)) > 0){
      if ((aux_print = send(user_session->connection,buffer,sent_size,0)) < sent_size) {
          fprintf(stderr,"Error sending the file to server: %d \n", aux_print);
          return;
      }
      //fprintf(stderr, "send result : %d\n",aux_print);
      bzero(buffer, SEG_SIZE); // Reseta o buffer
  }
  //fprintf(stderr, "send finished to server\n");
  fclose(file_handler);
  return;
  }
void get_file(SESSION *user_session, char *filename) {
  char buffer[SEG_SIZE], path[100];
  REQUEST client_request;
  ssize_t received_size;
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");

  sprintf (path, "%s/sync_dir_%s/%s",home_dir, user_session->userid,filename);
  strcpy(client_request.file_info.name, filename);
  client_request.command = CMD_DOWNLOAD;

  send(user_session->connection,(char *)&client_request,sizeof(client_request),0);
  file_handler = fopen(path,"w");
  bzero(buffer,SEG_SIZE);
  while ((received_size = recv(user_session->connection, buffer, sizeof(buffer), 0)) > 0){
      fwrite(buffer, 1,received_size, file_handler); // Escreve no arquivo
      bzero(buffer, SEG_SIZE);
      if(received_size < SEG_SIZE){ // Se o pacote que veio, for menor que o tamanho total, eh porque o arquivo acabou
          fprintf(stderr, "arquivo recebido: %d\n", received_size);
          fclose(file_handler);
          return;
        }
      }
}

int main(int argc, char* argv[]) {
  pthread_t thread_cli, thread_sync;
  int rc;
  SESSION user_session = {0};

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <fulano> <host> <port>\n", argv[0]);
    exit(-1);
  }

  init_session(&user_session);

  strncpy(user_session.userid, argv[1], MAXNAME-1);
  if (!connect_to_server(&user_session, argv[2], argv[3])) {
    exit(1);
  }

  if (!login(&user_session)) {
    exit(1);
  }

  create_dir_for(user_session.userid);

  rc = pthread_create(&thread_cli, NULL, client_cli, (void *) &user_session);
  if (rc != 0) {
    printf("Couldn't create threads.\n");
    exit(1);
  }
  rc = pthread_create(&thread_sync, NULL, client_sync, (void *) &user_session);
  if (rc != 0) {
    printf("Couldn't create threads.\n");
    exit(1);
  }
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
  pthread_join(thread_sync, NULL);
  pthread_join(thread_cli, NULL);
  disconnect_from_server(&user_session);
  exit(0);
}
