CC ?= gcc
all:
	$(CC) -Wall -Wextra -Wno-unused-parameter -o multi-seat-wayland multi-seat-wayland.c `pkg-config --libs --cflags wayland-client eina`
	$(CC) -Wall -Wextra -Wno-unused-parameter -o multi-seat-vnc multi-seat-vnc.c `pkg-config --libs --cflags libvncserver evas eina`

debug:
	$(CC) -Wall -Wextra -Wno-unused-parameter -O0 -g -o multi-seat-wayland multi-seat-wayland.c `pkg-config --libs --cflags wayland-client eina`
	$(CC) -Wall -Wextra -Wno-unused-parameter -O0 -g -o multi-seat-vnc multi-seat-vnc.c `pkg-config --libs --cflags libvncserver evas eina`
