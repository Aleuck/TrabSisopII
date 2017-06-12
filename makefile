# usage:
# make LOGLEVEL=5 for debug
# make LOGLEVEL=4 for info
# make LOGLEVEL=3 for warning (default)
# make LOGLEVEL=2 for error
# make LOGLEVEL=1 for critical

# default, will be overwritten by argument given to make in the command line.
LOGLEVEL := 2

CC = gcc
CFLAGS = -W -Wall -D LOGLEVEL=$(LOGLEVEL)
LIBDIR =.
LDFLAGS = -L$(LIBDIR) -pthread -lpthread
SRC = dropboxServer.c dropboxClient.c
OBJ = $(SRC:.c=.o)

all: dropboxServer dropboxClient

dropboxServer: linked_list.o list_dir.o user_login.o logging.o dropboxUtil.o dropboxServer.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

dropboxClient: linked_list.o list_dir.o dropboxClientSync.o logging.o dropboxUtil.o dropboxClient.o dropboxClientCli.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf *.o dropboxClient dropboxServer $(LIBDIR)/dropboxUtil.so
