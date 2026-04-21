CC = g++
CFLAGS = -Wall -g

SERVER_SRCS = server.cpp world.cpp protocol.cpp
CLIENT_SRCS = client.cpp world.cpp protocol.cpp

SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)

all: server client

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f server client $(SERVER_OBJS) $(CLIENT_OBJS)
