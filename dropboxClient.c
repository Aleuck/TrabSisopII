#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "dropboxUtil.h"
#include "dropboxClient.h"

int main(int argc, char* argv[]) {
    struct sockaddr_in serv_addr;
    int sock;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        exit(-1);
    }

    // create TCP socket
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        fprintf(stderr, "Error: Could not create socket.\n");
        exit(1);
    }

    // Initialize
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    exit(0);
}
