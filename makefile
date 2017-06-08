CC = gcc
CFLAGS = -W -Wall
LIBDIR =.
LDFLAGS = -L$(LIBDIR) -pthread -lpthread
SRC = dropboxServer.c dropboxClient.c
OBJ = $(SRC:.c=.o)

all: dropboxServer dropboxClient

dropboxServer: dropboxServer.o $(LIBDIR)/dropboxUtil.so
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)
dropboxClient: dropboxClient.o $(LIBDIR)/dropboxUtil.so
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

$(LIBDIR)/dropboxUtil.so: dropboxUtil.o
	$(CC) -shared -o $@ $< $(CFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm *.o $(LIBDIR)/dropboxUtil.so
