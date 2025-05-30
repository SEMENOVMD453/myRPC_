CC = gcc
CFLAGS = -Wall -Imysyslog
LDFLAGS = -ljson-c
BUILD = build
BIN = bin
DEB = deb

CLIENT_SRC = client/myRPC-client.c
CLIENT_OBJ = $(BUILD)/myRPC-client.o
CLIENT_BIN = $(BIN)/myRPC-client

SERVER_SRC = server/myRPC-server.c
SERVER_OBJ = $(BUILD)/myRPC-server.o
SERVER_BIN = $(BIN)/myRPC-server

SYSLOG_SRC = mysyslog/mysyslog.c
SYSLOG_OBJ = $(BUILD)/mysyslog.o

.PHONY: all clean deb

all: $(CLIENT_BIN) $(SERVER_BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(BIN):
	mkdir -p $(BIN)

$(DEB):
	mkdir -p $(DEB)

$(CLIENT_OBJ): $(CLIENT_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(CLIENT_BIN): $(CLIENT_OBJ) | $(BIN)
	$(CC) $^ -o $@ $(LDFLAGS)

$(SERVER_OBJ): $(SERVER_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(SYSLOG_OBJ): $(SYSLOG_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER_BIN): $(SERVER_OBJ) $(SYSLOG_OBJ) | $(BIN)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD) $(BIN) client/build-deb server/build-deb

deb: all $(DEB)
	# Client deb
	mkdir -p client/build-deb/usr/local/bin client/build-deb/DEBIAN
	cp $(CLIENT_BIN) client/build-deb/usr/local/bin/
	echo "Package: myRPC-client" > client/build-deb/DEBIAN/control
	echo "Version: 1.0" >> client/build-deb/DEBIAN/control
	echo "Architecture: amd64" >> client/build-deb/DEBIAN/control
	echo "Maintainer: support@example.com" >> client/build-deb/DEBIAN/control
	echo "Description: RPC client" >> client/build-deb/DEBIAN/control
	chmod 755 client/build-deb/DEBIAN
	mkdir -p deb
	fakeroot dpkg-deb --build client/build-deb $(DEB)/myRPC-client_1.0_amd64.deb

	# Server deb
	mkdir -p server/build-deb/usr/local/bin server/build-deb/DEBIAN
	cp $(SERVER_BIN) server/build-deb/usr/local/bin/
	echo "Package: myRPC-server" > server/build-deb/DEBIAN/control
	echo "Version: 1.0" >> server/build-deb/DEBIAN/control
	echo "Architecture: amd64" >> server/build-deb/DEBIAN/control
	echo "Maintainer: support@example.com" >> server/build-deb/DEBIAN/control
	echo "Description: RPC server daemon" >> server/build-deb/DEBIAN/control
	chmod 755 server/build-deb/DEBIAN
	fakeroot dpkg-deb --build server/build-deb $(DEB)/myRPC-server_1.0_amd64.deb
