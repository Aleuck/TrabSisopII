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

uint32_t last_serv_id = 1;
server_t this_server = { 1, 0, 0, 0, 1, 0, {0} };
server_t master_server = { 0, 0, 0, 0, 0, 0, {0} };

struct linked_list server_list = { 0, 0, 0 };

pthread_mutex_t replication_mutex = {{0}};

int connect_to_master(in_addr_t host, in_port_t port);

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
  send_message(0, client_socket, &msg);
}

int receive_file(int sock, SSL *ssl_sock, FILE_INFO file, char *userid){
  char path[256];
  uint32_t received_total = 0;
  int32_t received_size = 0, aux_print = 0;
  MESSAGE msg = {0, 0, {0}};
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");
  sprintf (path, "%s/sisopBox/sync_dir_%s/%s",home_dir, userid, file.name);
  fprintf(stderr, "%s\n", path);
  file_handler = fopen(path,"w");
  bzero(&msg,sizeof(msg));
  if (file_handler == NULL) {
    logerror("(receive) Could not open file to write.");
    msg.code = TRANSFER_ERROR;
    aux_print = send_message(sock, ssl_sock, &msg);
    if (aux_print <= 0) return aux_print;
    return -1;
  } else {
    msg.code = TRANSFER_ACCEPT;
    aux_print = send_message(sock, ssl_sock, &msg);
    if (aux_print <= 0) return aux_print;
    flogdebug("(receive) (user %s) accepted file `%s` of size %d. waiting transfer...", userid, file.name, file.size);
    while (file.size - received_total > sizeof(msg.content)) {
      bzero(&msg,sizeof(msg));
      aux_print = recv_message(0, ssl_sock, &msg);
      if (aux_print <= 0) return aux_print;
      received_size = msg.length;
      if (msg.code != TRANSFER_OK) {
        logerror("Transfer not OK!!!");
        return -1;
      }
      fwrite(msg.content, 1, received_size, file_handler); // Escreve no arquivo
      received_total += received_size;
      flogdebug("(receive) %d/%d (%d to go)", received_total, file.size, (int) file.size - received_total);
    }
    if (received_total < file.size) {
      bzero(&msg, sizeof(msg));
      aux_print = recv_message(sock, ssl_sock, &msg);
      if (aux_print <= 0) return aux_print;
      received_size = msg.length;
      if (msg.code != TRANSFER_END) {
        logerror("Transfer not OK!!!");
        return -1;
      }
      fwrite(msg.content, 1, received_size, file_handler); // Escreve no arquivo
      received_total += received_size;
      flogdebug("(receive) %d/%d (%d to go)", received_total, file.size, (int) file.size - received_total);
    }
    fclose(file_handler);
    set_file_stats(path, &file);
    return 1;
  }
}

int send_file(int sock, SSL *ssl_sock, FILE_INFO file, char *userid){
  char path[512];
  MESSAGE msg = {0,0,{0}};
  int32_t send_size, aux_print;
  uint32_t total_sent = 0;
  FILE *file_handler;
  const char* home_dir = getenv ("HOME");

  sprintf (path, "%s/sisopBox/sync_dir_%s/%s", home_dir, userid,file.name);

  fprintf(stderr, "Sending file %s\n", path);
  file_handler = fopen(path, "r");
  if (file_handler == NULL) {
    printf("Error sending the file: %s \n", userid);
    msg.code = TRANSFER_ERROR;
    aux_print = send_message(sock, ssl_sock, &msg);
    return 0;
  }
  msg.code = TRANSFER_ACCEPT;
  get_file_stats(path, &file);
  fprint_file_info(stdout, &file);
  serialize_file_info(&file, msg.content);
  msg.length = FILE_INFO_BUFLEN;
  aux_print = send_message(sock, ssl_sock, &msg);
  flogdebug("(send) size_buffer = %d.", sizeof(msg.content));
  while (file.size - total_sent > sizeof(msg.content)) {
    bzero(&msg,sizeof(msg));
    msg.code = TRANSFER_OK;
    send_size = fread(msg.content, 1, sizeof(msg.content), file_handler);
    msg.length = send_size;
    aux_print = send_message(sock, ssl_sock, &msg);
    if (aux_print <= 0) {
      //TODO:
      logerror("(send) couldn't send file completely.");
      fclose(file_handler);
      return aux_print;
    }
    total_sent += send_size;
    flogdebug("(send) %d/%d (%d to go)", total_sent, file.size, file.size - total_sent);
  }
  if (total_sent < file.size) {
    bzero(&msg,sizeof(msg));
    msg.code = TRANSFER_END;
    send_size = fread(msg.content, 1, file.size - total_sent, file_handler);
    msg.length = send_size;
    aux_print = send_message(sock, ssl_sock, &msg);
    if (aux_print <= 0) {
      //TODO:
      logerror("(send) couldn't send file completely.");
      fclose(file_handler);
      return aux_print;
    }
    total_sent += send_size;
    flogdebug("(send) %d/%d (%d to go)", total_sent, file.size, (long) file.size - (long) total_sent);
  }
  fprintf(stderr, "(send) finished: %s\n", userid);
  fclose(file_handler);
  return 1;
}

void delete_file(FILE_INFO file, char *userid) {
  char path[512];
  //int32_t send_size, aux_print;
  const char* home_dir = getenv ("HOME");
  sprintf (path, "%s/sisopBox/sync_dir_%s/%s", home_dir, userid,file.name);
  fprintf(stderr, "Deleting file `%s`\n", path);
  remove(path);
}

int send_server_list(int sock, SSL *ssl_sock) {
  MESSAGE msg = {SERVER_LIST,0,{0}};

  int i = 0;
  server_t *info;
  struct ll_item *item = server_list.first;

  while (item != NULL) {
    info = item->value;
    info->priority = i + 1;
    serialize_server_info(info, msg.content+(SERV_INFO_LEN*i));
    item = item->next;
    i += 1;
  }

  msg.length = i;
  return send_message(sock, ssl_sock, &msg);
}

int send_file_replicas(FILE_INFO file, char *userid){
  MESSAGE msg = {0,0,{0}};
  struct ll_item *item = server_list.first;
  struct ll_item *next;
  server_t *server;
  int res;
  msg.code = CMD_UPLOAD;
  msg.length = 1;
  memcpy(msg.content, userid, MAXNAME);
  serialize_file_info(&file, msg.content+MAXNAME);
  while (item != NULL) {
    next = item->next;
    server = item->value;
    int attempts = 5;
    while (attempts > 0) {
      res = send_message(server->connection, NULL, &msg);
      if (res > 0) {
        res = send_file(server->connection, NULL, file, userid);
      }
      if (res == 0) {
        attempts = 0;
      }
      attempts -= 1;
      if (attempts <= 0) {
        ll_del(item->key, &server_list);
      }
    }
    item = next;
  }
  this_server.is_master = 1;
  this_server.priority = 0;
}

int recv_master_cmd(int sock) {
  MESSAGE msg = {0,0,{0}};
  int i;
  char str_id[10];
  char userid[MAXNAME];
  FILE_INFO file_info = {{0},{0},{0},0};
  int res = recv_message(sock, NULL, &msg);
  if (res <= 0) return res;
  switch (msg.code) {
    case SERVER_LIST:
      pthread_mutex_lock(&replication_mutex);
      server_t serv_info;
      ll_term(&server_list);
      ll_init(sizeof(server_t), &server_list);
      for (i = 0; i < msg.length; i += 1) {
        deserialize_server_info(&serv_info,msg.content+(i*SERV_INFO_LEN));
        sprintf(str_id, "%x", serv_info.id);
        ll_put(str_id, &serv_info, &server_list);
      }
      pthread_mutex_unlock(&replication_mutex);
      break;
    case CMD_UPLOAD:
      pthread_mutex_lock(&replication_mutex);
      memcpy(userid, msg.content, MAXNAME);
      deserialize_file_info(&file_info, msg.content+MAXNAME);
      create_server_dir_for(userid);
      res = receive_file(sock, NULL, file_info, userid);
      pthread_mutex_unlock(&replication_mutex);
      break;
    case CMD_DELETE:
      pthread_mutex_lock(&replication_mutex);
      memcpy(userid, msg.content, MAXNAME);
      deserialize_file_info(&file_info, msg.content+MAXNAME);
      create_server_dir_for(userid);
      delete_file(file_info, userid);
      pthread_mutex_unlock(&replication_mutex);
      break;
    default:
      break;
  }
  return res;
}

void send_file_list(SSL *client_socket, struct user *user) {
  struct ll_item *item;
  FILE_INFO *info;
  MESSAGE msg;
  msg.code = TRANSFER_ACCEPT;
  msg.length = user->cli->files.length + user->cli->deleted_files.length;
  // send num files
  send_message(0, client_socket, &msg);
  item = user->cli->files.first;
  while (item != NULL) {
    msg.code = FILE_OK;
    info = (struct file_info *) item->value;
    serialize_file_info(info, msg.content);
    send_message(0, client_socket, &msg);
    item = item->next;
  }
  item = user->cli->deleted_files.first;
  while (item != NULL) {
    msg.code = FILE_DELETED;
    info = (struct file_info *) item->value;
    serialize_file_info(info, msg.content);
    send_message(0, client_socket, &msg);
    item = item->next;
  }
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

  this_server.addr = 0; // unknown yet
  this_server.port = port;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);
  server_addr.sin_family = AF_INET;

  if (bind(server_sock, (struct sockaddr *)&server_addr, sock_size) < 0) {
    fprintf(stderr, "Error: Could not bind socket.\n");
    return -1;
  }
  if (listen(server_sock , QUEUE_SIZE) < 0) {
    return -1;
  }
  return server_sock;
}

void master_election() {
  struct ll_item *item = server_list.first;
  struct ll_item *next;
  server_t *server;
  int res;
  while (item != NULL) {
    next = item->next;
    server = item->value;
    if (server->id == this_server.id) {
      this_server.is_master = 1;
      this_server.priority = 0;
      ll_del(item->key, &server_list);
      return;
    }
    int attempts = 5;
    while (attempts > 0) {
      res = connect_to_master(server->addr, REPLIC_PORT);
      if (res == 1) {
        ll_del(item->key, &server_list);
        return;
      }
      attempts -= 1;
      sleep(3);
    }
    ll_del(item->key, &server_list);
    item = next;
  }
  this_server.is_master = 1;
  this_server.priority = 0;
}

int connect_to_master(in_addr_t host, in_port_t port) {
  MESSAGE msg = {0, 0, {0}};
  server_t master = master_server;
  server_t slave = this_server;
  struct sockaddr_in sockaddr;
  int connection = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (connection < 0) {
    logerror("(connect_to_master) Could not create socket.");
    return 0;
  }
  memset((char*) &sockaddr, 0, sizeof(sockaddr));
  master.addr = host;
  master.port = 0; // port clients should use is still unknown
  sockaddr.sin_addr.s_addr = htonl(host);
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(port);
  if (connect(connection, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0) {
    logerror("(connect_to_master) Could not connect to master.");
    return 0;
  }
  msg.code = MASTER_CONNECT;
  msg.length = 2;
  serialize_server_info(&master, msg.content);
  serialize_server_info(&slave, msg.content+SERV_INFO_LEN);
  ssize_t aux = send_message(connection, NULL, &msg);
  if (aux == 0) {
    // disconected
    return 0;
  }
  if (aux < 0) {
    // error
    return -1;
  }
  aux = recv_message(connection, NULL, &msg);
  if (aux == 0) {
    // disconected
    return 0;
  }
  if (aux < 0) {
    // error
    return -1;
  }
  if (msg.code == MASTER_DECLINE) {
    close(connection);
    return -1;
  }
  if (msg.code == SERVER_REDIRECT) {
    close(connection);
    deserialize_server_info(&master, msg.content);
    logdebug("REDIRECTED");
    return connect_to_master(master.addr, port);
  }
  if (msg.code != MASTER_ACCEPT) {
    close(connection);
    return -1;
  }
  deserialize_server_info(&master, msg.content);
  deserialize_server_info(&slave, msg.content+SERV_INFO_LEN);
  master.connection = connection;
  master.sockaddr = sockaddr;
  pthread_mutex_lock(&replication_mutex);
  master_server = master;
  this_server = slave;
  pthread_mutex_unlock(&replication_mutex);
  return 1;
}

int connect_to_slave(int my_sock) {
  MESSAGE msg = {0, 0, {0}};
  server_t master = {0,0,0,0,0,0,{0}};
  server_t slave = {0,0,0,0,0,0,{0}};
  int connection;
  struct sockaddr_in sockaddr;
  socklen_t sockaddr_size = sizeof(sockaddr);
  memset((char *) &sockaddr, 0, sizeof(sockaddr));
  connection = accept(my_sock, (struct sockaddr *)&sockaddr, &sockaddr_size);
  if (connection < 0) {
    // connection failed
    return -1;
  }
  ssize_t aux = recv_message(connection, NULL, &msg);
  if (aux == 0) {
    // disconected
    close(connection);
    return 0;
  }
  if (aux < 0) {
    // error
    close(connection);
    return -1;
  }
  if (msg.code != MASTER_CONNECT) {
    return -1;
  }
  if (!this_server.is_master) {
    memset(msg.content, 0, sizeof(msg.content));
    msg.code = SERVER_REDIRECT;
    msg.length = 1;
    serialize_server_info(&master_server, msg.content);
    aux = send_message(connection, NULL, &msg);
    close(connection);
    return 0;
  }
  deserialize_server_info(&master, msg.content);
  deserialize_server_info(&slave, msg.content+SERV_INFO_LEN);
  master.id = this_server.id;
  master.priority = this_server.priority;
  this_server.addr = master.addr;
  master.port = this_server.port;
  master.is_master = this_server.is_master;
  slave.id = ++last_serv_id;
  slave.is_master = 0;
  slave.addr = ntohl(sockaddr.sin_addr.s_addr);
  slave.connection = connection;
  slave.sockaddr = sockaddr;
  memset((char*) &msg, 0, sizeof(msg));
  msg.code = MASTER_ACCEPT;
  msg.length = 2;
  serialize_server_info(&this_server, msg.content);
  serialize_server_info(&slave, msg.content+SERV_INFO_LEN);
  aux = send_message(connection, NULL, &msg);
  if (aux == 0) {
    close(connection);
    return 0;
  }
  if (aux < 0) {
    // error
    close(connection);
    return -1;
  }
  char str_id[10];
  sprintf(str_id, "%x", slave.id);
  pthread_mutex_lock(&replication_mutex);
  ll_put(str_id, &slave, &server_list);
  pthread_mutex_unlock(&replication_mutex);
  fprintf(stderr, "slave added:\n");
  fprint_server_info(stderr, &slave);
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
  SSL_CTX *ctx;
  SSL *ssl;
  SSL_METHOD *method = SSLv23_server_method();
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
  MESSAGE msg = {0,0,{0}};
  int res;
  switch(user_msg.code){
    case CMD_DOWNLOAD:
      deserialize_file_info(&f_info, user_msg.content);
      logdebug("(procces_command) send_file started.");
      pthread_mutex_lock(current_user->cli_mutex);
      send_file(0, client_socket, f_info, current_user->cli->userid);
      pthread_mutex_unlock(current_user->cli_mutex);
      logdebug("(procces_command) send_file finished.");
      break;
    case CMD_UPLOAD:
      deserialize_file_info(&f_info, user_msg.content);
      pthread_mutex_lock(current_user->cli_mutex);
      FILE_INFO *server_file;
      server_file = ll_getref(f_info.name, &current_user->cli->files);
      if (server_file != NULL && server_file->size == f_info.size && atol(server_file->last_modified) == atol(f_info.last_modified)) {
        // server_file already updated
        msg.code = TRANSFER_DECLINE;
        send_message(0, client_socket, &msg);
        pthread_mutex_unlock(current_user->cli_mutex);
        break;
      }
      if (server_file != NULL && current_user->cli->files.length < MAXFILES) {
        // max_files
        msg.code = TRANSFER_DECLINE;
        send_message(0, client_socket, &msg);
        pthread_mutex_unlock(current_user->cli_mutex);
        break;
      }
      res = receive_file(0, client_socket, f_info, current_user->cli->userid);
      if (res == 1) {
        // remove from deleted
        FILE_INFO *deleted_file = ll_getref(f_info.name, &current_user->cli->deleted_files);
        if (deleted_file != NULL) {
          ll_del(f_info.name, &current_user->cli->deleted_files);
        }
        // add to file list
        if (server_file == NULL) {
          ll_put(f_info.name, &f_info, &current_user->cli->files);
        } else {
          *server_file = f_info;
        }
        send_file_replicas(f_info, current_user->cli->userid)
      }
      pthread_mutex_unlock(current_user->cli_mutex);
      break;
    case CMD_LIST:
      logdebug("(procces_command) send_file_list started.");
      pthread_mutex_lock(current_user->cli_mutex);
      send_file_list(client_socket, current_user);
      pthread_mutex_unlock(current_user->cli_mutex);
      logdebug("(procces_command) send_file_list finished.");
      break;
    case CMD_DELETE:
      deserialize_file_info(&f_info, user_msg.content);
      logdebug("(procces_command) delete_file started.");
      pthread_mutex_lock(current_user->cli_mutex);
      delete_file(f_info, current_user->cli->userid);
      struct file_info *localfile = ll_getref(f_info.name, &current_user->cli->files);
      if (localfile != NULL) {
        ll_put(f_info.name, localfile, &current_user->cli->deleted_files);
        ll_del(f_info.name, &current_user->cli->files);
      }
      pthread_mutex_unlock(current_user->cli_mutex);
      logdebug("(procces_command) delete_file finished.");
      break;
    case CMD_SERV_LIST:
      //TODO
      break;
    case CMD_TIME:
      logdebug("(procces_command) send_time started.");
      pthread_mutex_lock(current_user->cli_mutex);
      sendTimeServer(client_socket);
      pthread_mutex_unlock(current_user->cli_mutex);
      logdebug("(procces_command) send_time finished.");
      break;
    default:
      logdebug("(procces_command) unreconized message.");
  }
}
void *treat_client(void *client_sock){
  int socket = *(int *)client_sock, sent;
  free(client_sock);
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
    sent = send_message(0, ssl, &response);
    while(recv_message(0, ssl, &client_msg) != 0){
      procces_command(new_user, client_msg, ssl);
    }
  } else {
    response.code = SERVER_REDIRECT;
    response.length = 1;
    serialize_server_info(&master_server, response.content);
    sent = send_message(0, ssl, &response);
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
  char *comm, *ip_str;
  while (1) {
    printf("\n >> ");
    fgets(command, MAXINPUT, stdin);
    trim(command);
    if (strlen(command) == 0) continue;
    comm = strtok(command, " \t\n");
    switch (parse_command(comm)) {
      case MASTER_CONNECT:
        ip_str = strtok(NULL, " \t\n");
        connect_to_master(ntohl(inet_addr(ip_str)), REPLIC_PORT);
        break;
      case CMD_SERV_LIST:
        break;
      case CMD_EXIT:
        break;
    }
  }
}

void *get_slaves() {
  int my_sock = get_socket(REPLIC_PORT);
  int slave_sock;
  while (keep_running) {
    connect_to_slave(my_sock);
  }
  exit(0);
}

void *server_sync() {
  MESSAGE msg = {0,0,{0}};
  int res;
  while (keep_running) {
    if (this_server.is_master) {
      struct ll_item *cur_item = server_list.first;
      struct ll_item *next_item;
      while (cur_item != NULL) {
        next_item = cur_item->next;
        server_t *cur_slave = cur_item->value;
        int attempts = 5;
        while (attempts > 0) {
          res = send_server_list(cur_slave->connection, NULL);
          attempts -= 1;
          // if res is 0, connection is lost;
          if (attempts <= 0 || res == 0) {
            ll_del(cur_item->key, &server_list);
            attempts = 0;
          }
        }
        cur_item = next_item;
      }
      sleep(10);
    } else {
      res = recv_master_cmd(master_server.connection);
      if (res == 0) {
        master_server.id = 0;
        // master_disconnected;
        loginfo("connection to master lost");
        sleep(this_server.priority);
        // TODO: attempt to reconnect first;
        master_election();
      }
    }
  }
  exit(0);
}

int main(int argc, char* argv[]) {
  int server_sock, client_sock;
  pthread_mutex_init(&replication_mutex ,NULL);
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
  // Initialize
  server_sock = get_socket(atoi(argv[1]));
  ll_init(sizeof(server_t), &server_list);

  pthread_t get_slaves_thread;
  pthread_t server_cli_thread;
  pthread_create(&get_slaves_thread, NULL, get_slaves, NULL);
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
