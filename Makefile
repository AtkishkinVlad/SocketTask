all: result

CFLAGS=-c -W -Wall -O3 
#CFLAGS=-c -W -Wall -g3 

result: main.o
	gcc -o result main.o -lm

main.o: main.c
	gcc ${CFLAGS} main.c

.PHONY: all clean

clean:
	rm -f *.o result
