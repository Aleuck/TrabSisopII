#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include "dropboxUtil.h"
#include "dropboxServer.h"


#define  get_sync_dir_OPTION  0
#define  send_file_OPTION 1
#define  recive_file_OPTION 2


struct Client user_list[10];

void *client_handler(void *client_id);

void sync_server(){

  return;
}

void recieve_file(char *file){

  return;
}

void send_file(char *file){

  return;
}
//Returns 1 if dir was created, 0 if dir already exists

int create_dir_for(char *user_name){
  struct stat st = {0};
  const char* home_dir = getenv ("HOME");
  char path [256];
  printf("user name recived = %s\n",user_name);
  sprintf (path, "%s/Desktop/sync_dir_%s",home_dir,user_name);

  if (stat(path, &st) == -1) {
    mkdir(path, 07777);
    return 1;
  }

  return 0;
}


int main(int argc, char* argv[]) {
    struct sockaddr_in server_addr, client_addr[10];
    int server_sock, client_sock, sock_size, *aux_sock;
    int client_count = 0;
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
    if(bind(server_sock,(struct sockaddr *)&server_addr , sock_size) < 0) {
      fprintf(stderr, "Error: Could not bind socket.\n");
      exit(-1);
    }
    // Listen
    printf("Start listening\n");
    listen(server_sock , 5);
    // Accept
    while(user_list[client_count].devices[0] = accept(server_sock, (struct sockaddr*)&client_addr[client_count], (socklen_t*)&sock_size)) {
        printf("Connection accepted for client %d\n", user_list[client_count].devices[0]);

        pthread_t socket_handler;
        aux_sock = malloc(1);

        *aux_sock = client_count;
        client_count += 1;
        user_list[client_count].logged_in = 1;

        printf("Creating new thread\n");
        if( pthread_create( &socket_handler , NULL,  client_handler , (void *) aux_sock) < 0) {
          fprintf(stderr, "Error: Could not create thread.\n");
          exit(-1);
        }
        printf("New thread assigned\n");
    }
    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }
	exit(0);
}

void *client_handler(void *client_id){

  int client_number = *(int*)client_id;
  int message_size;
  char client_message[100];
  int server_response = 1, client_request = 0, user_name[MAXNAME], response_buffer;
  int printable;
  if(printable = recv(user_list[client_number].devices[0],user_list[client_number].userid,sizeof(user_name), 0) < 0){
    printf("error on recv\n");
    return 0;
  }

  printf("%d / %d / user recived %s\n", client_number, printable ,user_list[client_number].userid);
  if(create_dir_for(user_list[client_number].userid) == 0){
    printf("User already has directory\n");
    server_response = 0;
  }else{
    printf("User directory created\n");
  }

  send(user_list[client_number].devices[0], &server_response,sizeof(server_response), 0);
  // Now that the client is connected successfully connected execute commands
  while (client_request != 100) {
    if (recv(user_list[client_number].devices[0], &client_request ,sizeof(client_request), 0) == 0) {
      return 0;
    }
    printf("client_request = %d for client = %s\n",client_request,user_list[client_number].userid);
    response_buffer = client_request;
    send(user_list[client_number].devices[0], &response_buffer,sizeof(response_buffer), 0);
    switch (client_request) {
      case 0:
        //sync_server();
        break;
      case 1:
        //send_file()
        break;
      case 2:
        //recieve_file()
        break;
    }
  }
  printf("Client connection lost for client %s\n", user_list[client_number].userid);
  close(user_list[client_number].devices[0]);
  return 0;

}
