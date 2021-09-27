USER=$(shell whoami)
CC = gcc

all: server client

server: ./Server/uftp_server.c
	$(CC) ./Server/uftp_server.c -o ./Server/server

client: ./Client/uftp_client.c
	$(CC) ./Client/uftp_client.c -o ./Client/client

clean:
	rm ./Server/server ./Client/client