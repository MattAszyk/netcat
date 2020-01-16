CC=gcc
CFLAGS= -Wall -lpthread

nc: netcat.c netcat.h
	${CC} ${CFLAGS} netcat.h netcat.c -o nc
clean:
	rm nc