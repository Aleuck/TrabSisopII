#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include "user_login.h"
#include "list_dir.h"

#define QUEUE_SIZE 5
#define  get_sync_dir_OPTION  0
#define  send_file_OPTION 1
#define  recive_file_OPTION 2
#define MAXUSER 10

struct handler_info {
  int index;
  int device;
};

struct client user_list[MAXUSER];

void sync_server(){

  return;
}

void receive_file(int client_socket, FILE_INFO file, struct user *user){
  char buffer[SEG_SIZE], path[256], response;
  ssize_t received_size;
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");
  pthread_mutex_lock(user->cli_mutex);
  if (user->cli->files.length < MAXFILES) {
    response = CMD_ACCEPT;
    send(client_socket,&response,sizeof(response),0);

    sprintf (path, "%s/sisopBox/sync_dir_%s/%s",home_dir, user->cli->userid, file.name);
    fprintf(stderr, "%s\n", path);
    file_handler = fopen(path,"w");
    bzero(buffer,SEG_SIZE);

    while ((received_size = recv(client_socket, buffer, sizeof(buffer), 0)) > 0){
      fwrite(buffer, 1,received_size, file_handler); // Escreve no arquivo
      bzero(buffer, SEG_SIZE);
      if(received_size < SEG_SIZE){ // Se o pacote que veio, for menor que o tamanho total, eh porque o arquivo acabou
        fprintf(stderr, "arquivo recebido: %d\n", (int) received_size);
        break;
      }
    }
    fclose(file_handler);
  } else {
    response = CMD_DECLINE;
    send(client_socket,&response,sizeof(response),0);
  }
  pthread_mutex_unlock(user->cli_mutex);
}

void send_file(int client_socket, FILE_INFO file, struct user *user){
  int send_size, aux_print, total_sent, filesize;
  FILE *file_handler;
  char bufinfo[FILE_INFO_BUFLEN];
  const char* home_dir = getenv ("HOME");
  char path[256], buffer[SEG_SIZE];

  sprintf (path, "%s/sisopBox/sync_dir_%s/%s", home_dir, user->cli->userid,file.name);

  pthread_mutex_lock(user->cli_mutex);

  fprintf(stderr, "Sending file %s\n", path);
  file_handler = fopen(path, "r");
  if (file_handler == NULL) {
    printf("Error sending the file to user: %s \n", user->cli->userid);
  } else {
    fseek(file_handler, 0L, SEEK_END);
    filesize = ftell(file_handler);
    flogdebug("send file of size %d.", filesize);
    rewind(file_handler);
    file.size = filesize;
    serialize_file_info(&file, bufinfo);
    send(client_socket, bufinfo, FILE_INFO_BUFLEN, 0);
    while (total_sent < (filesize - (int) sizeof(buffer))) {
      send_size = fread(buffer, 1, sizeof(buffer), file_handler);
      aux_print = send(client_socket, buffer, send_size, 0);
      if (aux_print == -1) {
        //TODO: error
        logerror("couldn't send file completely.");
        exit(-1);
      }
      total_sent += aux_print;
      bzero(buffer, SEG_SIZE); // Reseta o buffer
    }
    send_size = fread(buffer, 1, filesize - total_sent, file_handler);
    aux_print = send(client_socket, buffer, send_size, 0);
    fprintf(stderr, "send finished to client: %s\n", user->cli->userid);
    fclose(file_handler);
  }
  pthread_mutex_unlock(user->cli_mutex);
  return;
}

void send_file_list(int client_socket, struct user *user) {
  struct ll_item *item;
  char buf[FILE_INFO_BUFLEN];
  FILE_INFO *info;
  uint32_t uintbuf;
  pthread_mutex_lock(user->cli_mutex);
  uintbuf = htonl(user->cli->files.length);
  // send num files
  send(client_socket, (char *)&uintbuf, sizeof(uintbuf), 0);
  item = user->cli->files.first;
  while (item != NULL) {
    info = (struct file_info *) item->value;
    serialize_file_info(info, buf);
    send(client_socket, buf, FILE_INFO_BUFLEN, 0);
    item = item->next;
  }
  pthread_mutex_unlock(user->cli_mutex);
}

int isLoggedIn(char *user_name) {
  int i;
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

int get_socket(int port){
  int server_sock;
  struct sockaddr_in server_addr;
  socklen_t sock_size;

  sock_size = sizeof(struct sockaddr_in);
  memset((char *) &server_addr, 0, sock_size);

  // create TCP socket
  server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (server_sock < 0) {
    fprintf(stderr, "Error: Could not create socket.\n");
    return -1;
  }

  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  server_addr.sin_family = AF_INET;

  if (bind(server_sock, (struct sockaddr *)&server_addr, sock_size) < 0) {
    fprintf(stderr, "Error: Could not bind socket.\n");
    return -1;
  }
  if(listen(server_sock , QUEUE_SIZE) < 0)
    return -1;
  return server_sock;
}

int connect_to_client(int server_sock){
  int client_sock;
  struct sockaddr_in client_info;
  socklen_t sock_size;

  sock_size = sizeof(struct sockaddr_in);
  memset((char *) &client_info, 0, sock_size);

  if((client_sock = accept(server_sock,(struct sockaddr *)&client_info, &sock_size)) < 0){
    return -1;
  }
  return client_sock;
}
void procces_command(struct user *current_user, REQUEST user_request, int client_socket){
  switch(user_request.command){
    case CMD_DOWNLOAD:
      send_file(client_socket, user_request.file_info, current_user);
      break;
    case CMD_UPLOAD:
      receive_file(client_socket, user_request.file_info, current_user);
      break;
    case CMD_LIST:
      send_file_list(client_socket, current_user);
      break;
  }
}
void *treat_client(void *client_sock){
  int socket = *(int *)client_sock, sent;
  struct user *new_user;
  char response_buffer = 0;
  REQUEST client_request;
  fprintf(stderr, "%d\n", socket);

  if(login_user(socket, &new_user) < 0){
    close(socket);
    return (void*) -1;
  }
  response_buffer = 1;
  sent = send(socket, &response_buffer,sizeof(char),0);
  fprintf(stderr, "sent: %d\n", sent);
  while(recv(socket, &client_request, sizeof(REQUEST),0) != 0){
    procces_command(new_user, client_request, socket);
  }
  logout_user(socket ,new_user);
  close(socket);
  return 0;
}


int main(int argc, char* argv[]) {
  int server_sock, client_sock;
  ul_init();
  create_server_dir();

  loginfo("server started...");
  flogdebug("%d arguments given", argc);
  flogdebug("program name is %s", argv[0]);

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(-1);
  }


  // Initialize
  server_sock = get_socket(atoi(argv[1]));


  // Accept
  while(1) {
    client_sock = connect_to_client(server_sock);
    if(client_sock > 0){
      pthread_t client_thread;
      int *aux_sock = malloc(sizeof(int));
      *aux_sock = client_sock;

      pthread_create(&client_thread, NULL, treat_client,(void *)aux_sock);

    }


  }
  if (client_sock < 0) {
    perror("accept failed");
    return 1;
  }
  exit(0);
}
