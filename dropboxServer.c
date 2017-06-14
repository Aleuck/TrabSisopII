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
  char path[256];
  uint32_t received_total = 0;
  int32_t received_size = 0, aux_print = 0;
  MESSAGE msg = {0, 0, {0}};
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");
  pthread_mutex_lock(user->cli_mutex);

  if (user->cli->files.length < MAXFILES) {
    // user can send file
    sprintf (path, "%s/sisopBox/sync_dir_%s/%s",home_dir, user->cli->userid, file.name);
    fprintf(stderr, "%s\n", path);
    file_handler = fopen(path,"w");
    bzero(&msg,sizeof(msg));
    if (file_handler == NULL) {
      logerror("(receive) Could not open file to write.");
      msg.code = TRANSFER_ERROR;
      aux_print = send(client_socket,(char*)&msg,sizeof(msg),0);
      // response = TRANSFER_ERROR;
      // send(client_socket,&response,sizeof(response),0);
    } else {
      msg.code = TRANSFER_ACCEPT;
      aux_print = send(client_socket,(char*)&msg,sizeof(msg),0);
      // response = TRANSFER_ACCEPT;
      // send(client_socket,&response,sizeof(response),0);
      floginfo("(receive) (user %s) accepted file `%s` of size %d. waiting transfer...", user->cli->userid, file.name, file.size);
      while (received_total < file.size - sizeof(msg.content)) {
        bzero(&msg,sizeof(msg));
        aux_print = recv(client_socket, (char *) &msg, sizeof(msg), 0);
        received_size = ntohl(msg.length);
        if (msg.code != TRANSFER_OK) {
          logerror("Transfer not OK!!!");
        }
        fwrite(msg.content, 1,received_size, file_handler); // Escreve no arquivo
        bzero(&msg, sizeof(msg));
        received_total += received_size;
        flogdebug("(receive) %d/%d (%d to go)", received_total, file.size, (int) file.size - received_total);
      }
      if (received_total < file.size) {
        aux_print = recv(client_socket, (char *) &msg, sizeof(msg), 0);
        received_size = ntohl(msg.length);
        if (msg.code != TRANSFER_END) {
          logerror("Transfer not OK!!!");
        }
        fwrite(msg.content, 1, received_size, file_handler); // Escreve no arquivo
        received_total += received_size;
        flogdebug("(receive) %d/%d (%d to go)", received_total, file.size, (int) file.size - received_total);
      }
      fclose(file_handler);
    }
  } else {
    logdebug("(receive) declined file.");
    msg.code = TRANSFER_DECLINE;
    send(client_socket,(char*)&msg,sizeof(msg),0);
    // response = TRANSFER_DECLINE;
    // send(client_socket,&response,sizeof(response),0);
  }
  pthread_mutex_unlock(user->cli_mutex);
}


void send_file(int client_socket, FILE_INFO file, struct user *user){
  int32_t send_size, aux_print;
  uint32_t total_sent = 0, filesize;
  FILE *file_handler;
  MESSAGE msg;
  const char* home_dir = getenv ("HOME");
  char path[256];

  sprintf (path, "%s/sisopBox/sync_dir_%s/%s", home_dir, user->cli->userid,file.name);

  pthread_mutex_lock(user->cli_mutex);

  fprintf(stderr, "Sending file %s\n", path);
  file_handler = fopen(path, "r");
  if (file_handler == NULL) {
    printf("Error sending the file to user: %s \n", user->cli->userid);
  } else {
    bzero(&msg, sizeof(msg));
    fseek(file_handler, 0L, SEEK_END);
    filesize = ftell(file_handler);
    flogdebug("(send) file of size %d.", filesize);
    rewind(file_handler);
    file.size = filesize;
    msg.code = TRANSFER_ACCEPT;
    serialize_file_info(&file, msg.content);
    aux_print = send(client_socket, (char *) &msg, sizeof(msg), 0);
    flogdebug("(send) size_buffer = %d.", sizeof(msg.content));
    while (total_sent < filesize - sizeof(msg.content)) {
      bzero(&msg,sizeof(msg));
      msg.code = TRANSFER_OK;
      send_size = fread(msg.content, 1, sizeof(msg.content), file_handler);
      msg.length = htonl(send_size);
      aux_print = send(client_socket, (char *) &msg, sizeof(msg), 0);
      if (aux_print <= 0) {
        //TODO: error
        logerror("(send) couldn't send file completely.");
        pthread_mutex_unlock(user->cli_mutex);
        fclose(file_handler);
        return;
      }
      total_sent += send_size;
      flogdebug("(send) %d/%d (%d to go)", total_sent, filesize, filesize - total_sent);
    }
    if (total_sent < filesize) {
      bzero(&msg,sizeof(msg));
      msg.code = TRANSFER_END;
      send_size = fread(msg.content, 1, filesize - total_sent, file_handler);
      msg.length = htonl(send_size);
      aux_print = send(client_socket, (char *) &msg, sizeof(msg), 0);
      total_sent += send_size;
      flogdebug("(send) %d/%d (%d to go)", total_sent, filesize, (long) filesize - (long) total_sent);
    }
    fprintf(stderr, "(send) finished to client: %s\n", user->cli->userid);
    fclose(file_handler);
  }
  pthread_mutex_unlock(user->cli_mutex);
  return;
}

void send_file_list(int client_socket, struct user *user) {
  struct ll_item *item;
  FILE_INFO *info;
  MESSAGE msg;
  pthread_mutex_lock(user->cli_mutex);
  msg.code = TRANSFER_ACCEPT;
  msg.length = htonl(user->cli->files.length);
  // send num files
  send(client_socket, (char *)&msg, sizeof(msg), 0);
  item = user->cli->files.first;
  while (item != NULL) {
    msg.code = TRANSFER_OK;
    info = (struct file_info *) item->value;
    serialize_file_info(info, msg.content);
    send(client_socket, (char *) &msg, sizeof(msg), 0);
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
void procces_command(struct user *current_user, MESSAGE user_msg, int client_socket){
  FILE_INFO f_info;
  switch(user_msg.code){
    case CMD_DOWNLOAD:
      deserialize_file_info(&f_info, user_msg.content);
      logdebug("(procces_command) send_file started.");
      send_file(client_socket, f_info, current_user);
      logdebug("(procces_command) send_file finished.");
      break;
    case CMD_UPLOAD:
      deserialize_file_info(&f_info, user_msg.content);
      logdebug("(procces_command) receive_file started.");
      receive_file(client_socket, f_info, current_user);
      logdebug("(procces_command) receive_file finished.");
      break;
    case CMD_LIST:
      logdebug("(procces_command) send_file_list started.");
      send_file_list(client_socket, current_user);
      break;
  }
}
void *treat_client(void *client_sock){
  int socket = *(int *)client_sock, sent;
  struct user *new_user;
  MESSAGE client_msg = {0,0,{0}};
  MESSAGE response = {0,0,{0}};
  fprintf(stderr, "%d\n", socket);

  if(login_user(socket, &new_user) < 0){
    close(socket);
    return (void*) -1;
  }
  response.code = 1;
  sent = send(socket, &response,sizeof(response),0);
  fprintf(stderr, "sent: %d\n", sent);
  while(recv(socket, &client_msg, sizeof(client_msg),0) != 0){
    procces_command(new_user, client_msg, socket);
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
