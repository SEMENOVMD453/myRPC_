NAME = myRPC-client
SRC = myRPC-client.c
OBJ = ../../build/$(NAME).o
BIN = ../../bin/$(NAME)
CC = gcc
CFLAGS = -Wall
LDFLAGS = -ljson-c

all: ../../bin ../../build $(BIN)

../../bin:
	mkdir -p ../../bin

../../build:
	mkdir -p ../../build

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

clean: 
	rm -f $(OBJ) $(BIN)
	rm -rf build_deb

deb: all
	mkdir -p build-deb/usr/local/bin
	cp $(BIN) build-deb/usr/local/bin/
	mkdir -p build-deb/DEBIAN
	echo "Package: $(NAME)" > build-deb/DEBIAN/control
	echo "Version: 1.0" >> build-deb/DEBIAN/control
	echo "Section: base" >> build-deb/DEBIAN/control
	echo "Priority: optional" >> build-deb/DEBIAN/control
	echo "Architecture: amd64" >> build-deb/DEBIAN/control
	echo "Maintainer: SemenovMD <www.semyonov@mail.ru>" >> build-deb/DEBIAN/control
	echo "Description: myRPC-client program" >> build-deb/DEBIAN/control
	chmod 0755 build-deb/DEBIAN
	chmod g-s build-deb/DEBIAN
	chmod 755 build-deb/DEBIAN
	chmod 644 build-deb/DEBIAN/control
	mkdir -p ../../deb
	fakeroot dpkg-deb --build build-deb ../../deb/$(NAME)_1.0_amd64.deb
