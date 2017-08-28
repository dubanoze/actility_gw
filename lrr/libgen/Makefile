
include libgen/build.mk

#ARCH := ARM
ARCH := LINUX

ifeq ($(ARCH), ARM)
	AR = /opt/toolchains/arm-2011.03-wirma2/bin/arm-none-linux-gnueabi-ar
	CC = /opt/toolchains/arm-2011.03-wirma2/bin/arm-none-linux-gnueabi-gcc
	STRIP = /opt/toolchains/arm-2011.03-wirma2/bin/arm-none-linux-gnueabi-strip
else ifeq ($(ARCH), LINUX)
	AR = ar
	CC = gcc
	STRIP = strip
	CFLAGS += -m32
endif

CFLAGS += -O3 -g -Wall -Werror

%.o: %.c
	$(CC) $(LDFLAGS) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJECTS)
	$(CC) -shared $(CFLAGS) -o $(TARGET) $(OBJECTS)

#INCLUDE = -L./libgen/ -llibgen
#LDFLAGS =

keygen: keygen.o $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^
	$(STRIP) --strip-unneeded -s -R .comment -R  .gnu.version keygen
	rm -f *.o libgen/*.o

all: $(TARGET) keygen

clean:
	rm -f keygen libgen/ibgen.so libgen/libgen.a

