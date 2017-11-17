
default: all

all:
	gcc -o server server.c io_utils.c -I. -Wall -g
	gcc -o client client.c io_utils.c -I. -Wall -g

clean:
	-rm -f server
	-rm -f client