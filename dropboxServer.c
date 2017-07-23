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
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "logging.h"
#include "user_login.h"
#include "list_dir.h"

#define QUEUE_SIZE 5
#define  get_sync_dir_OPTION  0
#define  send_file_OPTION 1
#define  recive_file_OPTION 2
#define MAXUSER 10
#define MAXINPUT 256

struct handler_info {
  int index;
  int device;
};

int keep_running = 1;
struct client user_list[MAXUSER];
server_t this_server = { 1, 0, 0, 0, 1, 0, {0} };
server_t master_server = { 1, 0, 0, 0, 1, 0, {0} };
struct linked_list server_list = { 0, 0, 0 };
uint32_t last_serv_id = 1;

pthread_mutex_t server_list_mutex = {{0}};

void init_ssl(){
	OpenSSL_add_all_algorithms();
	SSL_library_init();
	SSL_load_error_strings();
}

void sendTimeServer(SSL *client_socket) {
  time_t server_time;
  time(&server_time);
  MESSAGE msg = {0,0,{0}};
  msg.code = TRANSFER_TIME;
  msg.length = sizeof(server_time);
  sprintf(msg.content,"%ld", server_time);
  send_message(client_socket, &msg);
}

void receive_file(SSL *client_socket, FILE_INFO file, struct user *user){
  char path[256];
  uint32_t received_total = 0;
  int32_t received_size = 0, aux_print = 0;
  MESSAGE msg = {0, 0, {0}};
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");
  pthread_mutex_lock(user->cli_mutex);
  FILE_INFO *server_file;
  FILE_INFO *deleted_file;
  server_file = ll_getref(file.name, &user->cli->files);
  if (server_file != NULL && server_file->size == file.size && atol(server_file->last_modified) == atol(file.last_modified)) {
    // server_file already updated
    msg.code = TRANSFER_DECLINE;
    send_message(client_socket, &msg);
  } else if (user->cli->files.length < MAXFILES) {
    // user can send file
    sprintf (path, "%s/sisopBox/sync_dir_%s/%s",home_dir, user->cli->userid, file.name);
    fprintf(stderr, "%s\n", path);
    file_handler = fopen(path,"w");
    bzero(&msg,sizeof(msg));
    if (file_handler == NULL) {
      logerror("(receive) Could not open file to write.");
      msg.code = TRANSFER_ERROR;
      aux_print = send_message(client_socket, &msg);
    } else {
      msg.code = TRANSFER_ACCEPT;
      aux_print = send_message(client_socket, &msg);
      floginfo("(receive) (user %s) accepted file `%s` of size %d. waiting transfer...", user->cli->userid, file.name, file.size);
      while (file.size - received_total > sizeof(msg.content)) {
        bzero(&msg,sizeof(msg));
        aux_print = recv_message(client_socket, &msg);
        received_size = msg.length;
        if (msg.code != TRANSFER_OK) {
          logerror("Transfer not OK!!!");
        }
        fwrite(msg.content, 1,received_size, file_handler); // Escreve no arquivo
        received_total += received_size;
        flogdebug("(receive) %d/%d (%d to go)", received_total, file.size, (int) file.size - received_total);
      }
      if (received_total < file.size) {
        bzero(&msg, sizeof(msg));
        aux_print = recv_message(client_socket, &msg);
        received_size = msg.length;
        if (msg.code != TRANSFER_END) {
          logerror("Transfer not OK!!!");
        }
        fwrite(msg.content, 1, received_size, file_handler); // Escreve no arquivo
        received_total += received_size;
        flogdebug("(receive) %d/%d (%d to go)", received_total, file.size, (int) file.size - received_total);
      }
      fclose(file_handler);
      set_file_stats(path, &file);
      // remove from deleted
      deleted_file = ll_getref(file.name, &user->cli->deleted_files);
      if (deleted_file != NULL) {
        ll_del(file.name, &user->cli->deleted_files);
      }
      // add to file list
      if (server_file == NULL) {
        ll_put(file.name, &file, &user->cli->files);
      } else {
        *server_file = file;
      }
      fprint_file_info(stdout, &file);
    }
  } else {
    logdebug("(receive) declined file.");
    msg.code = TRANSFER_DECLINE;
    send_message(client_socket, &msg);
    // response = TRANSFER_DECLINE;
  }
  pthread_mutex_unlock(user->cli_mutex);
}

void send_file(SSL *client_socket, FILE_INFO file, struct user *user){
  char path[512];
  MESSAGE msg = {0,0,{0}};
  int32_t send_size, aux_print;
  uint32_t total_sent = 0;
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");

  pthread_mutex_lock(user->cli_mutex);
  sprintf (path, "%s/sisopBox/sync_dir_%s/%s", home_dir, user->cli->userid,file.name);

  fprintf(stderr, "Sending file %s\n", path);
  file_handler = fopen(path, "r");
  if (file_handler == NULL) {
    printf("Error sending the file to user: %s \n", user->cli->userid);
    msg.code = TRANSFER_ERROR;
    aux_print = send_message(client_socket, &msg);
    pthread_mutex_unlock(user->cli_mutex);
    return;
  }
  msg.code = TRANSFER_ACCEPT;
  get_file_stats(path, &file);
  fprint_file_info(stdout, &file);
  serialize_file_info(&file, msg.content);
  msg.length = FILE_INFO_BUFLEN;
  aux_print = send_message(client_socket, &msg);
  flogdebug("(send) size_buffer = %d.", sizeof(msg.content));
  while (file.size - total_sent > sizeof(msg.content)) {
    bzero(&msg,sizeof(msg));
    msg.code = TRANSFER_OK;
    send_size = fread(msg.content, 1, sizeof(msg.content), file_handler);
    msg.length = send_size;
    aux_print = send_message(client_socket, &msg);
    if (aux_print <= 0) {
      //TODO:
      logerror("(send) couldn't send file completely.");
      pthread_mutex_unlock(user->cli_mutex);
      fclose(file_handler);
      return;
    }
    total_sent += send_size;
    flogdebug("(send) %d/%d (%d to go)", total_sent, file.size, file.size - total_sent);
  }
  if (total_sent < file.size) {
    bzero(&msg,sizeof(msg));
    msg.code = TRANSFER_END;
    send_size = fread(msg.content, 1, file.size - total_sent, file_handler);
    msg.length = send_size;
    aux_print = send_message(client_socket, &msg);
    total_sent += send_size;
    flogdebug("(send) %d/%d (%d to go)", total_sent, file.size, (long) file.size - (long) total_sent);
  }
  fprintf(stderr, "(send) finished to client: %s\n", user->cli->userid);
  fclose(file_handler);
  pthread_mutex_unlock(user->cli_mutex);
  return;
}

void delete_file(SSL *client_socket, FILE_INFO file, struct user *user) {
  char path[512];
  MESSAGE msg = {0,0,{0}};
  //int32_t send_size, aux_print;
  const char* home_dir = getenv ("HOME");
  sprintf (path, "%s/sisopBox/sync_dir_%s/%s", home_dir, user->cli->userid,file.name);
  pthread_mutex_lock(user->cli_mutex);
  fprintf(stderr, "Deleting file `%s`\n", path);
  struct file_info *localfile = ll_getref(file.name, &user->cli->files);
  if (localfile != NULL) {
    ll_put(file.name, localfile, &user->cli->deleted_files);
    ll_del(file.name, &user->cli->files);
  }
  remove(path);
  pthread_mutex_unlock(user->cli_mutex);
}

void send_server_list(SSL *client_socket) {}

void send_file_list(SSL *client_socket, struct user *user) {
  struct ll_item *item;
  FILE_INFO *info;
  MESSAGE msg;
  pthread_mutex_lock(user->cli_mutex);
  msg.code = TRANSFER_ACCEPT;
  msg.length = user->cli->files.length + user->cli->deleted_files.length;
  // send num files
  send_message(client_socket, &msg);
  item = user->cli->files.first;
  while (item != NULL) {
    msg.code = FILE_OK;
    info = (struct file_info *) item->value;
    serialize_file_info(info, msg.content);
    send_message(client_socket, &msg);
    item = item->next;
  }
  item = user->cli->deleted_files.first;
  while (item != NULL) {
    msg.code = FILE_DELETED;
    info = (struct file_info *) item->value;
    serialize_file_info(info, msg.content);
    send_message(client_socket, &msg);
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

  this_server.addr = INADDR_ANY;
  this_server.port = htons(port);
  server_addr.sin_addr.s_addr = this_server.addr;
  server_addr.sin_port = this_server.port;
  server_addr.sin_family = AF_INET;

  if (bind(server_sock, (struct sockaddr *)&server_addr, sock_size) < 0) {
    fprintf(stderr, "Error: Could not bind socket.\n");
    return -1;
  }
  if(listen(server_sock , QUEUE_SIZE) < 0)
    return -1;
  return server_sock;
}

connect_to_master(const char *host, const char *port) {
  MESSAGE msg = {0, 0, {0}};

  master_server.connection = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (master_server.connection < 0) {
    logerror("(connect_to_master) Could not create socket.");
    return 0;
  }
  memset((char*)&master_server.sockaddr, 0, sizeof(master_server.sockaddr));
  master_server.sockaddr.sin_addr.s_addr = inet_addr(host);
  master_server.sockaddr.sin_family = AF_INET;
  master_server.sockaddr.sin_port = htons(atoi(port));
  if (connect(master_server.connection, (struct sockaddr *) &master_server.sockaddr, sizeof(master_server.sockaddr)) < 0) {
    logerror("(connect_to_master) Could not connect to master.");
    return 0;
  }
  msg.code = MASTER_CONNECT;
  msg.length = 1;
  serialize_server_info(&master_server, msg.content);
  return 1;
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

SSL *ssl_connect(int client_sock){
  SSL_METHOD *method;
  SSL_CTX *ctx;
  SSL *ssl;
  
    method = SSLv23_server_method();
  ctx = SSL_CTX_new(method);
  if(ctx == NULL){
    ERR_print_errors_fp(stderr);
    abort();
  }


   SSL_CTX_use_certificate_file(ctx, "CertFile.pem", SSL_FILETYPE_PEM);
   SSL_CTX_use_PrivateKey_file(ctx, "KeyFile.pem", SSL_FILETYPE_PEM);

   ssl = SSL_new(ctx);
   SSL_set_fd(ssl, client_sock);

   SSL_accept(ssl);

   X509 *cert;
   char *line;
   cert = SSL_get_peer_certificate(ssl);
   if (cert !=  NULL){
      line  = X509_NAME_oneline(
      X509_get_subject_name(cert),0,0);
      fprintf(stderr,"Subject:  %s\n",  line);
      free(line);
      line  = X509_NAME_oneline(X509_get_issuer_name(cert),0,0);
      fprintf(stderr,"Issuer: %s\n",  line);
      }
   
   return ssl;
}

void procces_command(struct user *current_user, MESSAGE user_msg, SSL *client_socket){
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
      logdebug("(procces_command) send_file_list finished.");
      break;
    case CMD_DELETE:
      deserialize_file_info(&f_info, user_msg.content);
      logdebug("(procces_command) delete_file started.");
      delete_file(client_socket, f_info, current_user);
      logdebug("(procces_command) delete_file finished.");
      break;
    case CMD_TIME:
      logdebug("(procces_command) send_time started.");
      sendTimeServer(client_socket);
      logdebug("(procces_command) send_time finished.");
    default:
      logdebug("(procces_command) unreconized message.");
  }
}
void *treat_client(void *client_sock){
  int socket = *(int *)client_sock, sent;
  struct user *new_user;
  MESSAGE client_msg = {0,0,{0}};
  MESSAGE response = {0,0,{0}};

  SSL *ssl = ssl_connect(socket);

  // SSL_write(ssl,"Hi :3\n",6); 

  if(login_user(socket, &new_user, ssl) < 0){
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(socket);
    return (void*) -1;
  }
  if (this_server.is_master) {
    response.code = LOGIN_ACCEPT;
    sent = send_message(ssl, &response);
    while(recv_message(ssl, &client_msg) != 0){
      procces_command(new_user, client_msg, ssl);
    }
  } else {
    response.code = SERVER_REDIRECT;
    response.length = 1;
    serialize_server_info(&master_server, response.content);
    sent = send_message(ssl, &response);
  }
  logout_user(socket ,new_user);
  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(socket);
  return 0;
}

int parse_command(char *command) {
  if (0 == strcmp(command, "conmaster" )) return MASTER_CONNECT;
  if (0 == strcmp(command, "servlist"  )) return CMD_SERV_LIST;
  if (0 == strcmp(command, "exit"      )) return CMD_EXIT;
  return CMD_UNDEFINED;
}

void *server_cli() {
  char command[MAXINPUT];
  char *comm, *ip;
  while (1) {
    printf("\n >> ");
    fgets(command, MAXINPUT, stdin);
    trim(command);
    if (strlen(command) == 0) continue;
    comm = strtok(command, " \t\n");
    switch (parse_command(comm)) {
      case MASTER_CONNECT:
        ip = strtok(NULL, " \t\n");
        connect_to_master(ip, REPLIC_PORT);
        break;
      case CMD_SERV_LIST:
        break;
      case CMD_EXIT:
        break;
    }
  }
}
void *heartbeat() {
  struct ll_item *replic = server_list.first;
  while (replic != NULL) {

    replic = replic->next;
  }
}
void *server_sync() {
  while (1) {
    while (this_server.is_master) {
      // I'M MASTER!
      logdebug("I'm master.");
      sleep(5);
    }
    while (!this_server.is_master) {
      // I'M SLAVE!
      logdebug("I'm slave.");
      sleep(5);
    }
  }
}

void *replication() {

}

int main(int argc, char* argv[]) {
  int server_sock, client_sock;
  ul_init();
  create_server_dir();
  init_ssl();
  loginfo("server started...");
  flogdebug("%d arguments given", argc);
  flogdebug("program name is %s", argv[0]);

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(-1);
  }
  this_server.port = atoi(argv[1]);
  // Initialize
  server_sock = get_socket(atoi(argv[1]));
  ll_init(sizeof(server_t), &server_list);

  pthread_t server_sync_thread;
  pthread_t server_cli_thread;
  pthread_create(&server_sync_thread, NULL, server_sync, NULL);
  pthread_create(&server_cli_thread, NULL, server_cli, NULL);

  // Accept
  while(keep_running) {
    client_sock = connect_to_client(server_sock);
    if(client_sock > 0){
      pthread_t *client_thread = malloc(sizeof(pthread_t));
      int *aux_sock = malloc(sizeof(int));
      *aux_sock = client_sock;
      pthread_create(client_thread, NULL, treat_client,(void *)aux_sock);
    }
  }
  exit(0);
}
