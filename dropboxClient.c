#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "dropboxUtil.h"
#include "dropboxClient.h"

#ifndef MSG_SIZE

#define MSG_SIZE 200

#endif

int main(int argc, char* argv[]) {

    struct sockaddr_in server_addr;
    int client_sock;
    char response_buffer[MSG_SIZE], input_buffer[MSG_SIZE];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        exit(-1);
    }

    // create TCP socket
	client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client_sock < 0) {
        fprintf(stderr, "Error: Could not create socket.\n");
        exit(1);
    }

    // Initialize

    memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if (connect(client_sock, (struct sockaddr *) &server_addr, sizeof (server_addr)) < 0) {
      printf("Failed to connect to server\n");
      return -1;
    }
    //SENDS MESSAGE TO SERVER AND RECIEVES COPY
    printf("Connected successfully - Please enter string\n");
    while(fgets(input_buffer, MSG_SIZE , stdin)!=NULL) {

      send(client_sock,input_buffer,strlen(input_buffer),0);

      if(recv(client_sock,response_buffer,MSG_SIZE,0)==0)
          printf("Error");
      else
          fputs(response_buffer,stdout);
      bzero(response_buffer,MSG_SIZE);//to clean buffer-->IMP otherwise previous word characters also came
}
    close(client_sock);


    exit(0);
}
