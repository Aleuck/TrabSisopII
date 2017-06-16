#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "linked_list.h"
#include "list_dir.h"
#include "dropboxUtil.h"
#include "dropboxClient.h"
#include "dropboxClientCli.h"
#include "dropboxClientSync.h"
#include "logging.h"

#ifndef MSG_SIZE

#define MSG_SIZE 100

#endif

int create_dir_for(char *user_name) {
  struct stat st = {0};
  const char* home_dir = getenv ("HOME");
  char path [256];
  printf("user name recived = %s\n", user_name);
  sprintf (path, "%s/sync_dir_%s",home_dir, user_name);
  if (stat(path, &st) == -1) {
    mkdir(path, 07777);
    return 1;
  }

  return 0;
}

int connect_to_server(SESSION *user_session, const char *host, const char *port) {
  // create TCP socket
  user_session->connection = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (user_session->connection < 0) {
    fprintf(stderr, "Error: Could not create socket.\n");
    return 0;
  }

  // Initialize
  memset((char *) &user_session->server, 0, sizeof(user_session->server));
  user_session->server.sin_addr.s_addr = inet_addr(host);
  user_session->server.sin_family = AF_INET;
  user_session->server.sin_port = htons(atoi(port));

  if (connect(user_session->connection, (struct sockaddr *) &user_session->server, sizeof (user_session->server)) < 0) {
    printf("Failed to connect to server.\n");
    return -1;
  }
  return 1;
}

void disconnect_from_server(SESSION *user_session) {
  close(user_session->connection);
};

int login(SESSION *user_session) {
  char server_response_byte = 0;
  int size_received;
  MESSAGE msg;
  memset(&msg, 0, sizeof(msg));
  strcpy(msg.content, user_session->userid);
  msg.length = htonl(strlen(user_session->userid)+1);
  msg.code = LOGIN_REQUEST;
  fprintf(stderr, "chegou\n");
  send(user_session->connection, (char*)&msg, sizeof(msg), 0);
  fprintf(stderr, "chegou\n");

  if ((size_received = recv(user_session->connection, &server_response_byte, sizeof(char), 0)) == 0){
    fprintf(stderr, "size_received : %d", size_received);
    printf("Server cap reached\n");
    return 0;
  }
  fprintf(stderr, "size_received : %d", size_received);
  fprintf(stderr, "size_received : %d, size of buffer: %d\n",size_received, server_response_byte );
  if (server_response_byte == 1) {
    printf("Login successful.\n");
    return 1;
  }
  if (server_response_byte == -1) {
    printf("Too many connections\n");
    return 0;
  }
  return 0;
}

void init_session(SESSION * user_session) {
  pthread_mutex_init(&user_session->connection_mutex, NULL);
  user_session->keep_running = 1;
}

void end_session(SESSION * user_session) {
  // this will signal threads to stop running as soon as they can;
  user_session->keep_running = 0;
}

void send_file(SESSION *user_session, char *file_path) {
  char name_aux[200];
  uint32_t total_sent = 0;
  int32_t aux_print, send_size;
  MESSAGE msg = {0, 0, {0}};
  FILE *file_handler;
  FILE_INFO file_to_send;
  const char s[2] = "/";
  char *token;
  memset(&file_to_send,0,sizeof(FILE_INFO));
  pthread_mutex_lock(&(user_session->connection_mutex));
  strcpy(name_aux, file_path);
  flogdebug("%s\n", name_aux);
  token = strtok(name_aux, s);
  while( token != NULL ) {
    strcpy(file_to_send.name,token);
    token = strtok(NULL, s);
  }
  flogdebug("%s\n", file_to_send.name);
  file_handler = fopen(file_path, "r");
  if (file_handler == NULL) {
    logerror("(send) Could not open file.");
  } else {
    // get file stats
    get_file_stats(file_path, &file_to_send);
    flogdebug("send file of size %d.", file_to_send.size);
    fprint_file_info(stdout, &file_to_send);
    // send request to send file
    serialize_file_info(&file_to_send ,msg.content);
    msg.code = CMD_UPLOAD;
    msg.length = htonl(FILE_INFO_BUFLEN);
    send(user_session->connection,(char *)&msg,sizeof(msg),0);
    // get response
    recv(user_session->connection, &msg, sizeof(msg), 0);
    if (msg.code != TRANSFER_ACCEPT) {
      // server refused
      flogwarning("server refused file.");
    } else {
      // server accepted
      while (file_to_send.size - total_sent > sizeof(msg.content)) {
        memset(&msg, 0, sizeof(msg));
        msg.code = TRANSFER_OK;
        send_size = fread(msg.content, 1, sizeof(msg.content), file_handler);
        msg.length = htonl(send_size);
        aux_print = send(user_session->connection, (char*) &msg, sizeof(msg), 0);
        if (aux_print == -1) {
          //TODO: error
          logerror("couldn't send file completely.");
          pthread_mutex_unlock(&(user_session->connection_mutex));
          end_session(user_session);
          exit(-1);
        }
        total_sent += send_size;
        flogdebug("(send) %d/%d (%d to go)", total_sent, file_to_send.size, (long) file_to_send.size - (long) total_sent);
      }
      if (total_sent < file_to_send.size) {
        memset(&msg, 0, sizeof(msg));
        msg.code = TRANSFER_END;
        send_size = fread(msg.content, 1, (int) file_to_send.size - total_sent, file_handler);
        msg.length = htonl(send_size);
        aux_print = send(user_session->connection, (char*)&msg, sizeof(msg), 0);
        if (send_size != (long) file_to_send.size - (long) total_sent)
          logwarning("File size does not match, did it change during transfer?");
        aux_print = send(user_session->connection, &msg, sizeof(msg), 0);
        total_sent += send_size;
        flogdebug("(send) %d/%d (%d to go)", total_sent, file_to_send.size, (long) file_to_send.size - (long) total_sent);
      }
      if (total_sent == file_to_send.size)
        logdebug("sent size matches file size.");
      logdebug("finished sending to server.");
    }
    fclose(file_handler);
  }
  pthread_mutex_unlock(&(user_session->connection_mutex));
  return;
}

void get_file(SESSION *user_session, char *filename, int to_sync_folder) {
  char path[512];
  MESSAGE msg = {0, 0, {0}};
  int32_t received_size = 0, aux_print = 0;
  uint32_t received_total = 0;
  FILE *file_handler = 0;
  FILE_INFO file_to_get;
  memset(&file_to_get, 0, sizeof(FILE_INFO));
  strcpy(file_to_get.name, filename);
  if (to_sync_folder) {
    const char* home_dir = getenv ("HOME");
  	sprintf (path, "%s/sync_dir_%s/%s",home_dir,user_session->userid,filename);
  } else {
    strcpy(path, filename);
  }
  file_handler = fopen(path,"w");
  if (file_handler == NULL) {
    logerror("(get) Could not open file.");
    return;
  }
  pthread_mutex_lock(&(user_session->connection_mutex));
  logdebug("(get) requesting_file.");
  msg.code = CMD_DOWNLOAD;
  msg.length = htonl(FILE_INFO_BUFLEN);
  serialize_file_info(&file_to_get, msg.content);
  aux_print = send(user_session->connection,(char *)&msg,sizeof(msg),0);
  sleep(1);
  aux_print = recv(user_session->connection,(char *)&msg,sizeof(msg),0);
  flogdebug("MSGCODE %d\nMSGLEN %d\nMSG:\n%0.256s", msg.code, ntohl(msg.length),msg.content);
  if (msg.code != TRANSFER_ACCEPT) {
    fclose(file_handler);
    remove(path);
    pthread_mutex_unlock(&(user_session->connection_mutex));
    return;
  }
  deserialize_file_info(&file_to_get, msg.content);
  flogdebug("(get) receive file size (%u).", file_to_get.size);
  logdebug("(get) start to receive file.");
  flogdebug("(get) size_buffer = %u.", sizeof(msg.content));
  while (file_to_get.size - received_total > sizeof(msg.content)){
    aux_print = recv(user_session->connection,(char* )&msg,sizeof(msg),0);
    if (aux_print <= 0) {
      // conexao perdida
      fclose(file_handler);
      remove(path);
      pthread_mutex_unlock(&(user_session->connection_mutex));
      end_session(user_session);
      return;
    }
    if (msg.code != TRANSFER_OK) {
      fclose(file_handler);
      remove(path);
      flogdebug("MSGCODE %d\nMSGLEN %d\nMSG:\n%0.256s", msg.code, ntohl(msg.length),msg.content);
      logerror("Transfer not OK!!!");
      pthread_mutex_unlock(&(user_session->connection_mutex));
      return;
    }
    received_size = ntohl(msg.length);
    fwrite(msg.content, 1, received_size, file_handler); // Escreve no arquivo
    received_total += received_size;
    flogdebug("(get) %d/%u (%d to go)", received_total, file_to_get.size, (int) file_to_get.size - received_total);
  }
  if (received_total < file_to_get.size) {
    logdebug("(get) going get more bytes.");
    aux_print = recv(user_session->connection, (char*)&msg, sizeof(msg), 0);
    if (aux_print <= 0) {
      // conexao perdida
      fclose(file_handler);
      remove(path);
      pthread_mutex_unlock(&(user_session->connection_mutex));
      end_session(user_session);
      return;
    }
    if (msg.code != TRANSFER_END) {
      logerror("Transfer not OK!!!");
    }
    received_size = ntohl(msg.length);
    fwrite(msg.content, 1,received_size, file_handler); // Escreve no arquivo
    flogdebug("(get) received %u bytes.", received_size);
    received_total += received_size;
    flogdebug("(get) %d/%u (%u to go)", received_total, file_to_get.size, file_to_get.size - received_total);
  }
  set_file_stats(path, &file_to_get);
  fprint_file_info(stdout, &file_to_get);
  flogdebug("(get) END: received %d bytes in total.", received_total);
  fclose(file_handler);
  pthread_mutex_unlock(&(user_session->connection_mutex));
}

struct linked_list request_file_list(SESSION *user_session) {
	struct linked_list list;
	ll_init(sizeof(struct file_info), &list);
	pthread_mutex_lock(&(user_session->connection_mutex));
  MESSAGE msg = {0,0,{0}};
  msg.code = CMD_LIST;

	int socket = user_session->connection;

	int send_len = send(socket, (char *)&msg, sizeof(msg), 0);
	if (send_len < 0) goto socket_error;
	else if (send_len == 0) goto socket_closed;

	int recv_len = recv(socket, (char *)&msg, sizeof(msg), 0);
	if (recv_len < 0) goto socket_error;
	else if (recv_len == 0) goto socket_closed;
	uint32_t num_files = ntohl(msg.length);
  flogdebug("(request_file_list) getting list of size %d.", num_files);
	uint32_t i;
	struct file_info info;
	for (i = 0; i < num_files; i++) {
		int recv_len = recv(socket, (char *)&msg, sizeof(msg), 0);
		if (recv_len < 0) goto socket_error;
		else if (recv_len == 0) goto socket_closed;

		deserialize_file_info(&info, msg.content);
		flogdebug("request_file_list(): received file_info %s", info.name);
		ll_put(info.name, &info, &list);
	}

  pthread_mutex_unlock(&(user_session->connection_mutex));
	return list;

socket_error:
	logerror("request_file_list(): socket error");
	pthread_mutex_unlock(&(user_session->connection_mutex));
	return list;

socket_closed:
	logerror("request_file_list(): socket closed");
	pthread_mutex_unlock(&(user_session->connection_mutex));
	return list;
}

int main(int argc, char* argv[]) {
  pthread_t thread_cli, thread_sync;
  SESSION user_session = {{0},{0},{0},0,{{0}},0};
  int rc;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <fulano> <host> <port>\n", argv[0]);
    exit(-1);
  }

  init_session(&user_session);

  strncpy(user_session.userid, argv[1], MAXNAME-1);
  if (!connect_to_server(&user_session, argv[2], argv[3])) {
    exit(1);
  }

  if (!login(&user_session)) {
    exit(1);
  }

  create_dir_for(user_session.userid);

  rc = pthread_create(&thread_cli, NULL, client_cli, (void *) &user_session);
  if (rc != 0) {
    printf("Couldn't create threads.\n");
    exit(1);
  }

  rc = pthread_create(&thread_sync, NULL, client_sync, (void *) &user_session);
  if (rc != 0) {
    printf("Couldn't create threads.\n");
    exit(1);
  }

  // while (client_request != 100) {
  //
  //   scanf("%d", &client_request);
  //
  //   switch (client_request) {
  //     default:
  //       send(user_session.connection, &client_request, sizeof(int), 0);
  //       recv(user_session.connection, &server_response_int, sizeof(int), 0);
  //       printf("server_response %d\n", server_response_int);
  //       break;
  //   }
  //}
  pthread_join(thread_sync, NULL);
  pthread_join(thread_cli, NULL);
  disconnect_from_server(&user_session);
  exit(0);
}
