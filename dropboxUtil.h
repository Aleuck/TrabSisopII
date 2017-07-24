#ifndef DROPBOX_UTIL_H
#define DROPBOX_UTIL_H

#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "logging.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

// Maximum file name with null terminator
#define MAXNAME 100

#define CMD_UNDEFINED     0
#define CMD_UPLOAD        1
#define CMD_DOWNLOAD      2
#define CMD_LIST          3
#define CMD_GET_SYNC_DIR  4
#define CMD_DELETE        5
#define CMD_TIME          6
#define CMD_SERV_LIST     7
#define CMD_EXIT          9

#define LOGIN_REQUEST    10
#define LOGIN_ACCEPT     11
#define LOGIN_DENY       12

#define TRANSFER_DECLINE 20
#define TRANSFER_ACCEPT  21
#define TRANSFER_OK      22
#define TRANSFER_END     23
#define TRANSFER_ERROR   24
#define TRANSFER_TIME    25

#define FILE_OK          31
#define FILE_DELETED     32

#define SERVER_REDIRECT  40

#define MASTER_CONNECT   50
#define MASTER_ACCEPT    51
#define MASTER_DECLINE   52

#define SERVER_LIST      60

#define STATUS_DEAD       0
#define STATUS_PDEAD      1
#define STATUS_ALIVE      2

#define MSG_MAX_LENGTH  1250
#define MSG_BUFFER_SIZE (1 + 4 + MSG_MAX_LENGTH)

#define SERV_INFO_LEN  15

#define FILE_INFO_BUFLEN (3*MAXNAME + 4)

typedef struct file_info {
  char name[MAXNAME];
  char extension[MAXNAME];
  char last_modified[MAXNAME];
  uint32_t size;
} FILE_INFO;

// typedef struct request {
//   char command;
//   char file_info[FILE_INFO_BUFLEN];
// } REQUEST;
typedef struct message {
  char code;
  uint32_t length;
  char content[MSG_MAX_LENGTH];
} MESSAGE;

typedef struct server {
  uint32_t id;
  uint32_t priority;
  in_addr_t addr;
  in_port_t port;
  char is_master;
  int connection;
  struct sockaddr_in sockaddr;
} server_t;

void serialize_file_info(struct file_info *info, char *buf);
void deserialize_file_info(struct file_info *info, char *buf);
void serialize_server_info(server_t *info, char *buf);
void deserialize_server_info(server_t *info, char *buf);
int get_file_stats(const char *path, FILE_INFO *file_info);
void set_file_stats(const char *path, const FILE_INFO *file_info);
void fprint_file_info(FILE *stream, struct file_info *info);
void fprint_server_info(FILE *stream, server_t *info);
ssize_t recv_message(int sock, SSL *ssl_sock, MESSAGE *msg);
ssize_t send_message(int sock, SSL *ssl_sock, MESSAGE *msg);
void trim(char *str);

#endif /* DROPBOX_UTIL_H */
