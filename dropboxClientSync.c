#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/inotify.h>
#include "dropboxUtil.h"
#include "dropboxClient.h"
#include "dropboxClientSync.h"
#include "logging.h"
#include "list_dir.h"

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
    // check for changes on file system
    i = 0;
    len = read (fd, buf, BUF_LEN);
    if (len == -1 && errno != EAGAIN) {
      perror("read");
      exit(EXIT_FAILURE);
    } else {
      while (i < len) {
        event = (struct inotify_event *) &buf[i];
        switch (event->mask) {
          case IN_CREATE:
            flogdebug("(inotify) created %s (%u)\n", event->name, event->len);
            sprintf(file_path, "%s/%s",sync_dir_path, event->name);
            send_file(user_session, file_path);
            break;
          case IN_MODIFY:
            flogdebug("(inotify) modified %s (%u)\n", event->name, event->len);
            sprintf(file_path, "%s/%s",sync_dir_path, event->name);
            send_file(user_session, file_path);
            break;
          case IN_DELETE:
            flogdebug("(inotify) deleted %s (%u)\n", event->name, event->len);
            sprintf(file_path, "%s/%s",sync_dir_path, event->name);
            //delete_file(user_session, file_path);
            break;
          default:
            break;
        }
        if (event->len)
        i += EVENT_SIZE + event->len;
      }
    }
    filelist_server = request_file_list(user_session);
    update_list = need_update(&filelist_server, &user_session->files);
    struct ll_item *item = update_list.first;
    while (item != NULL) {
      struct file_info* info;
      info = (struct file_info *)item->value;
      flogdebug("should download `%s`",info->name);
      get_file(user_session, info->name, 1);
      item = item->next;
    }
    ll_term(&update_list);
    ll_term(&filelist_server);
    sleep(3);
  }
  return 0;
}
