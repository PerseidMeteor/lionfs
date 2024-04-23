CC=g++
CFLAGS=`pkg-config fuse3 --cflags --libs`
SRC=xmp.cc
TARGET=xmp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -Wall $(SRC) $(CFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)
