#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "linked_list.h"
#include "dropboxUtil.h"
#include "dropboxClient.h"
#include "dropboxClientCli.h"
#include "dropboxClientSync.h"

#define MAXINPUT 256

int parse_command(char *command) {
  if (0 == strcmp(command, "upload"  )) return CMD_UPLOAD;
  if (0 == strcmp(command, "download")) return CMD_DOWNLOAD;
  if (0 == strcmp(command, "list"    )) return CMD_LIST;
  if (0 == strcmp(command, "exit"    )) return CMD_EXIT;
  if (0 == strcmp(command, "time"    )) return CMD_TIME;
  return CMD_UNDEFINED;
}

void *client_cli(void *session_arg) {
  SESSION *user_session = (SESSION *) session_arg;
  char command[MAXINPUT];
  char *comm;
  char *filepath;
  while (user_session->keep_running) {
    printf(">> ");
    fgets(command, MAXINPUT, stdin);
    trim(command);
    if (strlen(command) == 0) continue;
    comm = strtok(command, " \t\n");
    switch (parse_command(comm)) {
      case CMD_TIME:

        fprintf(stderr, "Server time: %ld\n", getTimeServer(user_session));
        
        break;
      case CMD_UPLOAD:
        filepath = strtok(NULL, " \t\n");
        if (!filepath) {
          printf("Missing filepath\n");
          continue;
        }
        printf("send_file: %s\n", filepath);
        send_file(user_session, filepath);
        break;
      case CMD_DOWNLOAD:
        filepath = strtok(NULL, " \t\n");
        if (!filepath) {
          printf("Missing filepath\n");
          continue;
        }
        printf("get_file: %s\n", filepath);
        get_file(user_session, filepath, 0);
        break;
      case CMD_LIST:;
        struct linked_list list;
        request_file_list(user_session, &list, NULL);
    		struct ll_item *item = list.first;
    		while (item != NULL) {
    			struct file_info* info;
    			info = (struct file_info *)item->value;
    			fprint_file_info(stdout, info);
    			item = item->next;
    		}
    		ll_term(&list);
        break;
      case CMD_EXIT:
        end_session(user_session);
        break;
      default:
        break;
    }
  }
  return 0;
}
