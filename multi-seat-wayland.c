#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <wayland-client.h>

#define SEAT_INTERFACE_VERSION (4)
#define COMPOSITOR_INTERFACE_VERSION (1)
#define SHELL_INTERFACE_VERSION (1)
#define SHM_INTERFACE_VERSION (1)

#define HEIGHT (600)
#define WIDTH (800)
#define STRIDE ((WIDTH) * (4))
#define BUFFER_SIZE ((STRIDE) * (HEIGHT))

struct SeatItem {
   struct wl_seat *seat;
   struct wl_pointer *pointer;
   struct wl_keyboard *keyboard;
   char *name;
   uint32_t id;
   uint32_t cap;
   struct wl_list link;
};

struct Context {
   struct wl_compositor *compositor;
   struct wl_buffer *buffer;
   struct wl_shell *shell;
   struct wl_list seats;
   struct wl_shm *shm;
};

static void
_pointer_moved(void *data,
               struct wl_pointer *wl_pointer,
               uint32_t time,
               wl_fixed_t surface_x,
               wl_fixed_t surface_y)
{
   struct SeatItem *item = data;

   printf("The pointer from seat '%s' has moved to X:%d Y:%d\n", item->name,
          wl_fixed_to_int(surface_x), wl_fixed_to_int(surface_y));
}

static void
_pointer_enter(void *data,
               struct wl_pointer *wl_pointer,
               uint32_t serial,
               struct wl_surface *surface,
               wl_fixed_t surface_x,
               wl_fixed_t surface_y)
{
}

static void
_pointer_leave(void *data,
               struct wl_pointer *wl_pointer,
               uint32_t serial,
               struct wl_surface *surface)
{
}

static void
_pointer_button(void *data,
                struct wl_pointer *wl_pointer,
                uint32_t serial,
                uint32_t time,
                uint32_t button,
                uint32_t state)
{
   struct SeatItem *item = data;

   printf("The pointer from seat '%s' pressed the button '%"PRIu32"'\n",
          item->name, button);
}

static void
_pointer_axis(void *data,
              struct wl_pointer *wl_pointer,
              uint32_t time,
              uint32_t axis,
              wl_fixed_t value)
{
}

static void
_pointer_frame(void *data,
               struct wl_pointer *wl_pointer)
{
}

static void
_pointer_axis_source(void *data,
                     struct wl_pointer *wl_pointer,
                     uint32_t axis_source)
{
}

static void
_pointer_axis_stop(void *data,
                   struct wl_pointer *wl_pointer,
                   uint32_t time,
                   uint32_t axis)
{
}

static void
_pointer_axis_discrete(void *data,
                       struct wl_pointer *wl_pointer,
                       uint32_t axis,
                       int32_t discrete)
{
}

static const struct wl_pointer_listener _pointer_listener = {
  .enter = _pointer_enter,
  .leave = _pointer_leave,
  .motion = _pointer_moved,
  .button = _pointer_button,
  .axis = _pointer_axis,
  .frame = _pointer_frame,
  .axis_source = _pointer_axis_source,
  .axis_stop = _pointer_axis_stop,
  .axis_discrete = _pointer_axis_discrete
};

static void
_keyboard_key_pressed(void *data,
             struct wl_keyboard *wl_keyboard,
             uint32_t serial,
             uint32_t time,
             uint32_t key,
             uint32_t state)
{
   struct SeatItem *item = data;

   printf("Keyboard from seat '%s' pressed the key '%"PRIu32"'\n", item->name,
          key);
}

static void
_keymap(void *data,
        struct wl_keyboard *wl_keyboard,
        uint32_t format,
        int32_t fd,
        uint32_t size)
{
}

static void
_keyboard_enter(void *data,
                struct wl_keyboard *wl_keyboard,
                uint32_t serial,
                struct wl_surface *surface,
                struct wl_array *keys)
{
}

static void
_keyboard_leave(void *data,
                struct wl_keyboard *wl_keyboard,
                uint32_t serial,
                struct wl_surface *surface)
{
}

static void
_keyboard_modifiers(void *data,
                    struct wl_keyboard *wl_keyboard,
                    uint32_t serial,
                    uint32_t mods_depressed,
                    uint32_t mods_latched,
                    uint32_t mods_locked,
                    uint32_t group)
{
}

static void
_keyboard_repeat_info(void *data,
                      struct wl_keyboard *wl_keyboard,
                      int32_t rate,
                      int32_t delay)
{
}

static const struct wl_keyboard_listener _keyboard_listener = {
  .keymap = _keymap,
  .enter = _keyboard_enter,
  .leave = _keyboard_leave,
  .key = _keyboard_key_pressed,
  .modifiers = _keyboard_modifiers,
  .repeat_info = _keyboard_repeat_info
};

static void
_print_seat_cap(struct SeatItem *item, bool changed)
{
   if (changed)
     printf("The seat '%s' capabilities changed\n", item->name);
   else
     printf("The seat '%s' has the following capabilities\n", item->name);

   if (!item->cap)
     {
        printf("\t* No capabilities\n");
        return;
     }

   if ((item->cap & WL_SEAT_CAPABILITY_POINTER) && !item->pointer) {
      printf("\t* Mouse\n");
      item->pointer = wl_seat_get_pointer(item->seat);
      assert(item->pointer);
      wl_pointer_add_listener(item->pointer, &_pointer_listener, item);
   } else if (!(item->cap & WL_SEAT_CAPABILITY_POINTER) && item->pointer) {
      wl_pointer_release(item->pointer);
      item->pointer = NULL;
   }

   if ((item->cap & WL_SEAT_CAPABILITY_KEYBOARD) && !item->keyboard) {
      printf("\t* Keyboard\n");
      item->keyboard = wl_seat_get_keyboard(item->seat);
      assert(item->keyboard);
      wl_keyboard_add_listener(item->keyboard, &_keyboard_listener,
                               item);
   } else if (!(item->cap & WL_SEAT_CAPABILITY_KEYBOARD) && item->keyboard) {
      wl_keyboard_release(item->keyboard);
      item->keyboard = NULL;
   }
}

static void
_seat_name(void *data, struct wl_seat *seat, const char *name)
{
   struct SeatItem *item = data;

   item->name = strdup(name);
   assert(item->name);

   _print_seat_cap(item, false);
}

static void
_seat_capabilities(void *data,
                   struct wl_seat *wl_seat,
                   uint32_t capabilities)
{
   //This callback is called first
   struct SeatItem *item = data;
   item->cap = capabilities;

   if (item->name)
     {
        _print_seat_cap(item, true);
     }
}

static const struct wl_seat_listener _seat_listener = {
  .capabilities = _seat_capabilities,
  .name = _seat_name
};

static void
_registry_global_add(void *data,
                     struct wl_registry *wl_registry,
                     uint32_t id,
                     const char *interface,
                     uint32_t version)
{
   struct SeatItem *item;
   struct Context *ctx = data;

   if (!strcmp(interface, wl_seat_interface.name))
     {

        printf("Found the seat interface with id '%"PRIu32"'\n", id);
        item = calloc(1, sizeof(struct SeatItem));
        if (!item)
          {
             fprintf(stderr, "Could not alloc memory for the seat item\n");
             return;
          }

        item->seat = wl_registry_bind(wl_registry, id, &wl_seat_interface,
                                      SEAT_INTERFACE_VERSION);
        assert(item->seat);
        item->id = id;

        wl_seat_add_listener(item->seat, &_seat_listener, item);
        wl_list_insert(&ctx->seats, &item->link);
     }
   else if (!strcmp(interface, wl_compositor_interface.name))
     {
        ctx->compositor = wl_registry_bind(wl_registry, id,
                                           &wl_compositor_interface,
                                           COMPOSITOR_INTERFACE_VERSION);
        assert(ctx->compositor);
     }
   else if (!strcmp(interface, wl_shell_interface.name))
     {
        ctx->shell = wl_registry_bind(wl_registry, id, &wl_shell_interface,
                                      SHELL_INTERFACE_VERSION);
        assert(ctx->shell);
     }
   else if (!strcmp(interface, wl_shm_interface.name))
     {
        ctx->shm = wl_registry_bind(wl_registry, id, &wl_shm_interface,
                                    SHM_INTERFACE_VERSION);
        assert(ctx->shm);
     }
}

static void
_registry_global_remove(void *data,
                        struct wl_registry *wl_registry,
                        uint32_t id)
{
   struct Context *ctx = data;
   struct SeatItem *item;
   struct wl_list link;

   wl_list_for_each(item, &ctx->seats, link) {
      if (item->id == id)
        {
           wl_seat_destroy(item->seat);
           wl_list_remove(&item->link);
           free(item->name);
           free(item);
           break;
        }
   }
}

static const struct wl_registry_listener _registry_listener = {
  .global = _registry_global_add,
  .global_remove = _registry_global_remove
};

static void
_shell_surface_ping(void *data, struct wl_shell_surface *shell_surface,
            uint32_t serial)
{
   wl_shell_surface_pong(shell_surface, serial);
}

static void
_shell_surface_popup_done(void *data,
                          struct wl_shell_surface *wl_shell_surface)
{
}

static void
_shell_surface_configure(void *data,
                         struct wl_shell_surface *wl_shell_surface,
                         uint32_t edges,
                         int32_t width,
                         int32_t height)
{
}

static const struct wl_shell_surface_listener _ss_listener = {
  .ping = _shell_surface_ping,
  .configure = _shell_surface_configure,
  .popup_done = _shell_surface_popup_done
};

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
   struct Context *ctx = data;
   wl_buffer_destroy(buffer);
   ctx->buffer = NULL;
   printf("buffer destroy\n");
}

static const struct wl_buffer_listener _buffer_listener = {
  .release = buffer_release
};

static void
_setup_buffer(struct Context *ctx, struct wl_surface *surface)
{
   int fd, r;
   //FIXME: Hardcoded /tmp path
   static const char template[] = "/tmp/multi-seat-XXXXXX";
   char final_path[sizeof(template)];
   struct wl_shm_pool *pool;
   struct wl_buffer *buffer;

   snprintf(final_path, sizeof(final_path), "%s", template);
   fd = mkostemp(final_path, O_CLOEXEC);
   assert(fd != -1);
   unlink(final_path);

   r = ftruncate(fd, BUFFER_SIZE);
   assert(r == 0);

   pool = wl_shm_create_pool(ctx->shm, fd, BUFFER_SIZE);
   assert(pool);

   buffer = wl_shm_pool_create_buffer(pool, 0,
                                      WIDTH, HEIGHT,
                                      STRIDE,
                                      WL_SHM_FORMAT_XRGB8888);
   assert(buffer);
   wl_surface_attach(surface, buffer, 0, 0);
   wl_buffer_add_listener(buffer, &_buffer_listener, ctx);
   ctx->buffer = buffer;
   wl_shm_pool_destroy(pool);
   close(fd);
}

static bool stop = false;

static void
_sig_action(int signum)
{
   stop = true;
}

int
main(int argc, char *argv[])
{
   int r;
   struct Context ctx;
   struct wl_display *display;
   struct wl_registry *registry;
   struct wl_shell_surface *shell_surface;
   struct wl_surface *surface;
   struct SeatItem *item, *tmp;
   struct sigaction sa;

   wl_list_init(&ctx.seats);

   sa.sa_handler = _sig_action;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_RESETHAND;
   sigaction(SIGINT, &sa, NULL);

   printf("Trying to connect to Wayland\n");
   display = wl_display_connect(NULL);
   if (!display) {
      fprintf(stderr, "Can't connect to display!\n");
      return -1;
   }

   registry = wl_display_get_registry(display);
   assert(registry);

   wl_registry_add_listener(registry, &_registry_listener, &ctx);

   wl_display_dispatch(display);
   wl_display_roundtrip(display);
   wl_registry_destroy(registry);

   surface = wl_compositor_create_surface(ctx.compositor);
   assert(surface);
   shell_surface = wl_shell_get_shell_surface(ctx.shell, surface);
   assert(shell_surface);

   wl_shell_surface_add_listener(shell_surface, &_ss_listener, NULL);
   wl_shell_surface_set_toplevel(shell_surface);
   wl_surface_damage(surface, 0, 0, 800, 600);
   _setup_buffer(&ctx, surface);
   wl_surface_commit(surface);

   while (!stop) {
      struct pollfd pfd;

      r = wl_display_dispatch_pending(display);
      assert(r == 0);
      r = wl_display_flush(display);
      assert(r >= 0);

      pfd.fd = wl_display_get_fd(display);
      pfd.events = POLLIN;
      pfd.revents = 0;
      poll(&pfd, 1, -1);
      if (pfd.revents & POLLIN)
         wl_display_dispatch(display);
   }

   wl_list_for_each_safe(item, tmp, &ctx.seats, link) {
      if (item->pointer)
        wl_pointer_destroy(item->pointer);
      if (item->keyboard)
        wl_keyboard_destroy(item->keyboard);
      if (item->seat)
        wl_seat_destroy(item->seat);
      free(item->name);
      free(item);
   }

   /* it may be destroyed on buffer_release already */
   if (ctx.buffer)
       wl_buffer_destroy(ctx.buffer);
   wl_shell_destroy(ctx.shell);
   wl_shm_destroy(ctx.shm);
   wl_compositor_destroy(ctx.compositor);
   wl_shell_surface_destroy(shell_surface);
   wl_surface_destroy(surface);
   wl_display_disconnect(display);
   printf("Disconnected from display\n");

   return 0;
}
