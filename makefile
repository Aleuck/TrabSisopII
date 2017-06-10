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

all: dropboxServer dropboxClient $(LIBDIR)/dropboxUtil.so

dropboxServer: logging.o dropboxServer.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

dropboxClient: dropboxClient.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(LIBDIR)/dropboxUtil.so: dropboxUtil.o
	$(CC) -shared -o $@ $< $(CFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -rf *.o $(LIBDIR)/dropboxUtil.so
