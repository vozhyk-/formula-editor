CFLAGS += -std=c99 -D_POSIX_C_SOURCE=200809L `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0`

all: formula-ed

#formula-ed: formula-ed.c
#	$(CC) -o triangle triangle.c
