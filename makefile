CC = g++

  # compiler flags:
  #  -g    adds debugging information to the executable file
  #  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall

  # the build target executable:
CLIENT = src/ClientMain
SERVER = src/ServerMain
SERVER_DPS = src/models/Client.cpp

all: Client Server

Client: #$(CLIENT)
		$(CC) $(CFLAGS) $(CLIENT).cpp -o client

Server: #$(SERVER)
		$(CC) $(CFLAGS) $(SERVER_DPS) $(SERVER).cpp -o server

clean:
	$(RM) client
	$(RM) server