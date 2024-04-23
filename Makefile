CC=g++
CFLAGS=$(shell pkg-config fuse3 --cflags) $(shell pkg-config libcurl --cflags)
LIBS=$(shell pkg-config fuse3 --libs) $(shell pkg-config libcurl --libs)
SRC=xmp.cc proxy.cc
TARGET=xmp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -Wall $(CFLAGS) $(SRC) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)
