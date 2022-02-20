CFLAGS = -O0 -g

PRODUCT = capture

CFILES = capture1.c

SRCS = ${CFILES}
OBJS = ${CFILES:.c=.o}

all:	${PRODUCT}

clean:
	-rm -f *.o *.NEW *~ *.d *.pgm
	-rm -f ${PRODUCT} ${GARBAGE}
	-rm -f frames/*

capture1:	capture1.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ capture1.o 

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<

