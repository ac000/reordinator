CC	= gcc
CFLAGS	= -Wall -Wextra -g -O2 -std=c99 \
	  $(shell pkg-config --cflags gtk+-3.0 glib-2.0 gmodule-2.0)
LDFLAGS	=
LIBS	= $(shell pkg-config --libs gtk+-3.0 glib-2.0 gmodule-2.0)

reordinator: reordinator.c
	$(CC) $(CFLAGS) ${LDFLAGS} ${LIBS} -o $@ $<

clean:
	rm -f reordinator
