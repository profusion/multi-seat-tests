CC ?= gcc
all:
	$(CC) -o multi-seat-wayland multi-seat-wayland.c `pkg-config --libs --cflags wayland-client`

debug:
	$(CC) -O0 -g -o multi-seat-wayland multi-seat-wayland.c `pkg-config --libs --cflags wayland-client`
