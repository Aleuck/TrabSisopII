#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "dropboxUtil.h"
#include "dropboxServer.h"

#include "logging.h"

#define  get_sync_dir_OPTION  0
#define  send_file_OPTION 1
#define  recive_file_OPTION 2
#define MAXUSER 10

struct handler_info {
  int index;
  int device;
};

struct client user_list[MAXUSER];

void *client_handler(void *client_id);

void sync_server(){

  return;
}

void receive_file(int client_socket, FILE_INFO file, char *username){

//add to user files

//

  char buffer[SEG_SIZE], path[100];
  REQUEST client_request;
  ssize_t received_size;
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");

  sprintf (path, "%s/sisopBox/sync_dir_%s/%s",home_dir, username, file.name);
  fprintf(stderr, "%s\n", path);
  file_handler = fopen(path,"w");
  bzero(buffer,SEG_SIZE);
  while ((received_size = recv(client_socket, buffer, sizeof(buffer), 0)) > 0){
      fwrite(buffer, 1,received_size, file_handler); // Escreve no arquivo
      bzero(buffer, SEG_SIZE);
      if(received_size < SEG_SIZE){ // Se o pacote que veio, for menor que o tamanho total, eh porque o arquivo acabou
          fprintf(stderr, "arquivo recebido: %d\n", received_size);
          fclose(file_handler);
          return;
        }
      }
}

void send_file(int client_socket, FILE_INFO file, char *username){
  int i = 0,  sent_size, aux_print;
  FILE *file_handler;
  REQUEST user_request;

  const char* home_dir = getenv ("HOME");
  char path[100], buffer[SEG_SIZE], filename[MAXNAME];

  sprintf (path, "%s/sisopBox/sync_dir_%s/%s",home_dir, username,file.name);

  fprintf(stderr, "Sending file %s\n", path);
  if ((file_handler = fopen(path, "r")) == NULL) {
        printf("Error sending the file to user: %s \n", username);
        return;
    }

    while ((sent_size = fread(buffer, 1,sizeof(buffer), file_handler)) > 0){
      if ((aux_print = send(client_socket,buffer,sent_size,0)) < sent_size) {
          fprintf(stderr,"Error sending the file to user: %s %d \n", username, aux_print);
          return;
      }
      fprintf(stderr, "send result : %d\n",aux_print);
      bzero(buffer, SEG_SIZE); // Reseta o buffer
  }
  fprintf(stderr, "send finished to client: %s\n", username);
  fclose(file_handler);
  return;
}

int isLoggedIn(char *user_name) {
  int i, count = 0;
  for (i = 0; i < MAXUSER; i++) {
    if (strcmp(user_name,user_list[i].userid) == 0) {
      return i;
    }
  }
  return -1;
}
//Returns 1 if dir was created, 0 if dir already exists
int create_server_dir() {
  struct stat st = {0};
  char path[100];
  const char* home_dir = getenv ("HOME");
  sprintf (path, "%s/sisopBox",home_dir);

  if (stat(path, &st) == -1) {
    mkdir(path, 07777);
    return 1;
  }

  return 0;
}

int create_dir_for(char *user_name) {
  struct stat st = {0};
  const char* home_dir = getenv ("HOME");
  char path [256];
  printf("user name recived = %s\n",user_name);
  sprintf (path, "%s/sisopBox/sync_dir_%s",home_dir,user_name);

  if (stat(path, &st) == -1) {
    mkdir(path, 07777);
    return 1;
  }

  return 0;
}


int main(int argc, char* argv[]) {
  struct sockaddr_in server_addr, client_addr[10];
  char user_name[MAXNAME], server_response;
  struct handler_info info;
  int server_sock, client_sock, sock_size, *aux_sock, temp_sock, aux_index, *index;
  int client_count = 0;

  create_server_dir();

  loginfo("server started...");
  flogdebug("%d arguments given", argc);
  flogdebug("program name is %s", argv[0]);

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(-1);
  }

  // create TCP socket
  server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (server_sock < 0) {
    fprintf(stderr, "Error: Could not create socket.\n");
    exit(-1);
  }
  // Initialize
  sock_size = sizeof(struct sockaddr_in);

  memset((char *) &server_addr, 0, sock_size);
  memset((char *) &client_addr, 0, sock_size);

  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(atoi(argv[1]));
  server_addr.sin_family = AF_INET;
  printf("Binding socket\n");
  // Bind
  if (bind(server_sock, (struct sockaddr *)&server_addr, sock_size) < 0) {
    fprintf(stderr, "Error: Could not bind socket.\n");
    exit(-1);
  }
  // Listen
  printf("Start listening\n");
  listen(server_sock , 5);

  // Accept
  while (temp_sock = accept(server_sock, (struct sockaddr *) &client_addr[client_count], (socklen_t *) &sock_size)) {
    printf("temp_sock %d\n", temp_sock);
    if (recv(temp_sock, user_name, sizeof(user_name), 0) < 0){
      printf("error on recv\n");
      return 0;
    }
    aux_index = isLoggedIn(user_name);
    server_response = 1;
    printf("aux_index %d \n", aux_index);
    printf("logged %d\n", user_list[aux_index].logged_in);
    if (aux_index != -1) {
      if (user_list[aux_index].logged_in == 2) {
        printf("User device limit reached for user: %s\n", user_name);
        server_response = -1;
      } else if (user_list[aux_index].devices[0] == 0) {
        user_list[aux_index].devices[0] = temp_sock;
        info.device = 0;
        user_list[aux_index].logged_in++;
      } else {
        user_list[aux_index].devices[1] = temp_sock;
        info.device = 1;
        user_list[aux_index].logged_in++;
      }
    } else {
      strcpy(user_list[client_count].userid, user_name);
      user_list[client_count].devices[0] = temp_sock;
      info.device = 0;
      user_list[client_count].logged_in++;
      aux_index = client_count;
    }
    if (create_dir_for(user_name) == 0){
      printf("User already has directory\n");
    } else {
      printf("User directory created\n");
    }

    send(temp_sock, &server_response,sizeof(server_response), 0);
    printf("server_response %d \n", server_response);
    if (server_response != -1){
      pthread_t socket_handler;
      info.index = aux_index;
      client_count += 1;

      printf("Creating new thread\n");
      if ( pthread_create( &socket_handler, NULL,  client_handler, (void *) &info) < 0) {
        fprintf(stderr, "Error: Could not create thread.\n");
        exit(-1);
      }
      printf("New thread assigned\n");
    }
  }
  if (client_sock < 0) {
    perror("accept failed");
    return 1;
  }
  exit(0);
}

void *client_handler(void *client_info) {
  struct handler_info *info = client_info;
  printf("%d\n", info -> index);
  printf("%d\n", info -> device);
  int client_number = info->index;
  int client_device = info->device;
  int message_size;
  char client_message[100];
  int server_response = 1, response_buffer;
  REQUEST client_request;

  // Now that the client is connected successfully connected execute commands
  while (client_request.command != 'a') {
    if (recv(user_list[client_number].devices[client_device], &client_request, sizeof(client_request), 0) == 0) {
      user_list[client_number].logged_in--;
      close(user_list[client_number].devices[client_device]);
      user_list[client_number].devices[client_device] = 0;
      printf("ended here\n");
      return 0;
    }
    printf("client_request = %d for client = %s\n", client_request.command, user_list[client_number].userid);

    switch (client_request.command) {
      case 0:
        break;
      case CMD_UPLOAD:
        fprintf(stderr ,"chegou\n");
        receive_file(user_list[client_number].devices[client_device],client_request.file_info, user_list[client_number].userid);
        break;

      case 2:
        fprintf(stderr ,"chegou\n");
        send_file(user_list[client_number].devices[client_device],client_request.file_info, user_list[client_number].userid);
      //break;
        break;
    }
  }
  printf("Client connection lost for client %s\n", user_list[client_number].userid);
  user_list[client_number].logged_in--;

  close(user_list[client_number].devices[client_device]);
  user_list[client_number].devices[client_device] = 0;
  return 0;
}
