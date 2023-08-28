CC = cc
RPC_SYSTEM = rpc.o
SERVER_SOURCES = server.c
CLIENT_SOURCES = client.c

.PHONY: format all clean

all: $(RPC_SYSTEM)

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) -c -o $@ $<

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f $(RPC_SYSTEM)