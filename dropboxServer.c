#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "dropboxUtil.h"
#include "dropboxServer.h"

int main(int argc, char* argv[]) {
    struct sockaddr_in server_addr, client_addr;
    int server_sock, client_sock, sock_size, *aux_sock;

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
    printf("1\n");
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
    //printf("4\n");
    // Accept
    while(client_sock = accept(server_sock, (struct sockaddr*)&client_addr, (socklen_t*)&sock_size)) {
        printf("Connection accepted\n");

        pthread_t socket_handler;
        aux_sock = malloc(1);
        *aux_sock = client_sock;
        printf("Creating new thread\n");
        if( pthread_create( &socket_handler , NULL,  client_handler , (void *) aux_sock) < 0) {
          fprintf(stderr, "Error: Could not create thread.\n");
          exit(-1);
        }
        printf("New thread assigned\n");
    }
    //printf("8\n");
    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }
	exit(0);
}

void *client_handler(void *client_socket){
  int socket_desc = *(int*)client_socket;
  int message_size;
  char server_response[100], client_message[2000];

    while((message_size = recv(socket_desc ,client_message,sizeof(client_message),0)) > 0){
      send(socket_desc ,client_message, message_size,0);
    }
    close(socket_desc);

    if(message_size == 0)
    {
      puts("Client Disconnected");
    }
    else
    {
      perror("recv failed");
    }
  return 0;

}
