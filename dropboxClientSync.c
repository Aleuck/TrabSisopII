#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include "dropboxUtil.h"
#include "dropboxClient.h"
#include "dropboxClientSync.h"
#include "logging.h"
#include "list_dir.h"
#include "linked_list.h"

/* size of the event structure, not counting name */
#define EVENT_SIZE  (sizeof (struct inotify_event))

/* reasonable guess as to size of 1024 events */
#define BUF_LEN        (1024 * (EVENT_SIZE + 16))

void *client_sync(void *session_arg) {
  SESSION *user_session = (SESSION *) session_arg;
  const char* home_dir = getenv ("HOME");
  char sync_dir_path[256];
  char file_path[256];
  char buf[BUF_LEN];
  int fd, wd;
  int len, i = 0;
  struct inotify_event *event;
  struct linked_list filelist_server, update_list;
  // Initialize inotify
  fd = inotify_init1(IN_NONBLOCK);
  if (fd < 0) {
    logerror("could not start inotify");
    end_session(user_session);
    exit(EXIT_FAILURE);
  }

  sprintf (sync_dir_path, "%s/sync_dir_%s",home_dir, user_session->userid);
  flogdebug("inotify watching `%s`\n", sync_dir_path);
  wd = inotify_add_watch (fd, sync_dir_path, IN_MODIFY | IN_CREATE | IN_DELETE);
  if (wd < 0) {
    flogerror("could not watch `%s`", sync_dir_path);
    end_session(user_session);
    exit(EXIT_FAILURE);
  }

  while (user_session->keep_running) {
    logdebug("-- start sync --");
    // check for changes on file system
    len = read (fd, buf, BUF_LEN);
    if (len == -1 && errno != EAGAIN) {
      perror("read");
      exit(EXIT_FAILURE);
    } else {
      printf("inotify run\n");
      i = 0;
      while (i < len) {
        FILE_INFO *list_file = NULL;
        FILE_INFO event_file;
        event = (struct inotify_event *) &buf[i];
        switch (event->mask) {
          case IN_CREATE:
            flogdebug("(inotify) created %s (%u)\n", event->name, event->len);
            memset(&event_file,0,sizeof(FILE_INFO));
            strcpy(event_file.name, event->name);
            sprintf(file_path, "%s/%s",sync_dir_path, event->name);
            if (get_file_stats(file_path, &event_file) == -1) {
              // this is a dir, ignore
              break;
            }
            list_file = ll_getref(event_file.name, &user_session->files);
            if (list_file == NULL) {
              ll_put(event_file.name, &event_file.name, &user_session->files);
            } else {
              *list_file = event_file;
            }
            send_file(user_session, file_path);
            flogdebug("(inotify) finished send_file %s (%u)\n", event->name, event->len);
            break;
          case IN_MODIFY:
            flogdebug("(inotify) modified %s (%u)\n", event->name, event->len);
            memset(&event_file,0,sizeof(FILE_INFO));
            strcpy(event_file.name, event->name);
            sprintf(file_path, "%s/%s",sync_dir_path, event->name);
            if (get_file_stats(file_path, &event_file) == -1) {
              // this is a dir, ignore
              break;
            }
            list_file = ll_getref(event_file.name, &user_session->files);
            if (list_file == NULL) {
              ll_put(event_file.name, &event_file.name, &user_session->files);
            } else {
              *list_file = event_file;
            }
            //struct ll_item *item = find(event->name, &user_session->files);
            send_file(user_session, file_path);
            flogdebug("(inotify) finished send_file %s (%u)\n", event->name, event->len);
            break;
          case IN_DELETE:
            flogdebug("(inotify) deleted %s (%u)\n", event->name, event->len);
            sprintf(file_path, "%s/%s",sync_dir_path, event->name);
            //delete_file(user_session, filename);
            break;
          default:
            break;
        }
        if (event->len)
          i += EVENT_SIZE + event->len;
      }
    }
    memset(&filelist_server,0,sizeof(struct linked_list));
    memset(&update_list,0,sizeof(struct linked_list));
    filelist_server = request_file_list(user_session);

    // should create file list mutex instead
    pthread_mutex_lock(&(user_session->connection_mutex));
    update_list = need_update(&filelist_server, &user_session->files);
    pthread_mutex_unlock(&(user_session->connection_mutex));

    struct ll_item *item = update_list.first;
    while (item != NULL) {
      struct file_info* info;
      info = (struct file_info *)item->value;
      flogdebug("should download `%s`",info->name);
      get_file(user_session, info->name, 1);
      item = item->next;
    }
    logdebug("--  end sync  --");
    ll_term(&update_list);
    ll_term(&filelist_server);
    sleep(3);
  }
  return 0;
}
