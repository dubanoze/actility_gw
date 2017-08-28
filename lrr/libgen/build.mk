
TARGET = libgen/libgen.so
LDFLAGS += -fPIC

SOURCES = $(shell echo libgen/*.c)
HEADERS = $(shell echo libgen/*.h)
OBJECTS = $(SOURCES:.c=.o)


