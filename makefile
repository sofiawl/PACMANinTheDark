CC = g++
CFLAGS = -Wall -g -std=c++20
CLIENT_LDLIBS = -lncursesw

SERVER_SRCS = server.cpp world.cpp protocol.cpp log.cpp
CLIENT_SRCS = client.cpp client_view.cpp world.cpp protocol.cpp log.cpp

SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)

all: server client

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS) $(CLIENT_LDLIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f server client $(SERVER_OBJS) $(CLIENT_OBJS) *recv*
