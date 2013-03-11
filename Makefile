CC=gcc
CFLAGS=-Wall -g -O2 -std=c99
LDFLAGS=
LIBS=`pkg-config --libs gtk+-3.0 glib-2.0 gmodule-2.0`
INCS=`pkg-config --cflags gtk+-3.0 glib-2.0 gmodule-2.0`

reordinator: reordinator.o
	$(CC) ${LDFLAGS} -o reordinator reordinator.o ${LIBS}

reordinator.o: reordinator.c
	$(CC) $(CFLAGS) -c reordinator.c ${INCS}

clean:
	rm -f reordinator *.o
