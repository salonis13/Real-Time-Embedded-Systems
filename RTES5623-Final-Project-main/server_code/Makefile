CFLAGS = -O0 -g

PRODUCT = server

CFILES = server.c

SRCS = ${CFILES}
OBJS = ${CFILES:.c=.o}

all:	${PRODUCT}

clean:
	-rm -f *.o *.NEW *~ *.d *.pgm
	-rm -f ${PRODUCT} ${GARBAGE}

server:	server.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ server.o 

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<

