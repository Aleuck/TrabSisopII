#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/inotify.h>
#include "dropboxUtil.h"
#include "dropboxClient.h"
#include "dropboxClientSync.h"

/* size of the event structure, not counting name */
#define EVENT_SIZE  (sizeof (struct inotify_event))

/* reasonable guess as to size of 1024 events */
#define BUF_LEN        (1024 * (EVENT_SIZE + 16))

void *client_sync(void *session_arg) {
  SESSION *user_session = (SESSION *) session_arg;
  int fd, wd;
  char buf[BUF_LEN];
  int len, i = 0;
  struct inotify_event *event;
  fd = inotify_init ();
  if (fd < 0)
    perror ("inotify_init");
  wd = inotify_add_watch (fd, "/home/agesly/Desktop/client_dir_aleuck", IN_MODIFY | IN_CREATE | IN_DELETE);
  if (wd < 0)
    perror ("inotify_add_watch");
  wd = inotify_init();
  while (user_session->keep_running) {
    i = 0;
    len = read (fd, buf, BUF_LEN);
    if (len < 0) {
      perror ("read");
    }
    while (i < len) {
      event = (struct inotify_event *) &buf[i];
      printf ("wd=%d mask=%u cookie=%u len=%u\n", event->wd, event->mask, event->cookie, event->len);
      if (event->len)
        printf ("name=%s\n", event->name);
      i += EVENT_SIZE + event->len;
    }
  }
  return 0;
}
