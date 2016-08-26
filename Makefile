CC ?= gcc
all:
	$(CC) -o multi-seat multi-seat.c `pkg-config --libs --cflags wayland-client`

debug:
	$(CC) -O0 -g -o multi-seat multi-seat.c `pkg-config --libs --cflags wayland-client`
