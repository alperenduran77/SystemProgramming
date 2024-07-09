CC = gcc
CFLAGS = -Wall -pthread

SERVER_SRC = server.c pseudo.c
CLIENT_SRC = client.c
SERVER_BIN = PideShop
CLIENT_BIN = HungryVeryMuch
SERVER_TXT = server_log.txt 
CLIENT_TXT = client_log.txt

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) pseudo.h
	$(CC) $(CFLAGS) -lm -o $(SERVER_BIN) $(SERVER_SRC)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) $(SERVER_TXT) $(CLIENT_TXT)
