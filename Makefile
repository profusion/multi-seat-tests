CC ?= gcc
all:
	$(CC) -o multi-seat-wayland multi-seat-wayland.c `pkg-config --libs --cflags wayland-client`
	$(CC) -o multi-seat-vnc multi-seat-vnc.c `pkg-config --libs --cflags libvncserver evas`

debug:
	$(CC) -O0 -g -o multi-seat-wayland multi-seat-wayland.c `pkg-config --libs --cflags wayland-client`
	$(CC) -O0 -g -o multi-seat-vnc multi-seat-vnc.c `pkg-config --libs --cflags libvncserver evas`
