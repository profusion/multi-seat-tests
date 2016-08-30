# About

Just a few tests playing with multi-seat
using Wayland / Weston support and remote clients
using VNC.

# Build

Build dependencies:

 * libvncserver
 * wayland

Build commands:

```sh
 $ make
```

# How to test

Run time dependencies:

 * weston compositor
 * a vnc client (like vinagre)

## multi-seat-wayland

If you have a laptop you could plug external mouse and keyboard
and define a seat.

You should add a rule to udev, something like:

```sh
sudo echo ENV{ID_VENDOR_ID}==\"046d\",ENV{ID_MODEL_ID}==\"c52b\",ENV{WL_SEAT}=\"whatever\" > /etc/udev/rules.d/98-wayland.rules
```

To find out about vendor and model id you could run
```sh
 $ lsusb
```

If no entry is added for a specific input, it will presume it's
part of 'default' seat.

After the rule is added, reloading udev rules and retriggering the
events are required. Or you may just reboot.

Run Weston, but not on X11, otherwise it will open a window
and use X11 backend, that doesn't support multi-seat.

So press ctrl+alt+F2, log and run weston:

```sh
 $ weston
```

Then open an Wayland terminal and run the test program:

```sh
 $ ./multi-seat-wayland
```

When moving the pointers over the launched window,
pressing or releasing mouse pointers some information
regarding the event will be pressed, including the seat
name you defined.

The same regarding keyboards, but the window must be focused
first.

When done testing, you may select back the terminal window,
press ctrl+c and kill weston:

```sh
 $ pkill weston
```

## multi-seat-vnc

Just run the test program, it connect on default TCP port.

```sh
 $ ./multi-seat-vnc
```

Connect a local machine using default display ":0" and
another machine in the same network using the host IP.
Then input information will be displayed.

If 'q' or ESC are pressed the program will quit.
