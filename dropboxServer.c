#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "dropboxUtil.h"
#include "dropboxServer.h"

int main(int argc, char* argv[]) {
    struct sockaddr_in serv_addr, cli_addr;
    int sock;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
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
    memset((char *) &cli_addr, 0, sizeof(cli_addr));
	exit(0);
}
