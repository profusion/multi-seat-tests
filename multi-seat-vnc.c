#include <stdio.h>
#include <stdlib.h>
#include <Evas.h>
#include <Ecore.h>
#include <Evas_Engine_Buffer.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <limits.h>
#include <Eina.h>
#include <signal.h>

#define WIDTH (800)
#define HEIGHT (600)
#define RECT_DIMEN (100)

static unsigned seat = 1;
static rfbScreenInfoPtr server = NULL;
static Ecore_Animator *animator = NULL;

struct Client_Data {
   unsigned seat;
   Ecore_Fd_Handler *fd_handler;
};

static void
_client_gone(rfbClientRec *client)
{
   struct Client_Data *cd;

   cd = client->clientData;
   seat--;
   printf("Client on seat '%u' is gone\n", cd->seat);
   ecore_main_fd_handler_del(cd->fd_handler);
   free(cd);
}

static Eina_Bool
_client_activity(void *data, Ecore_Fd_Handler *fd_handler)
{
   rfbClientRec *client = data;

   rfbProcessClientMessage(client);
   if (client->sock == -1)
     {
        rfbClientConnectionGone(client);
        return ECORE_CALLBACK_DONE;
     }
   return ECORE_CALLBACK_RENEW;
}

static enum rfbNewClientAction
_new_client(rfbClientRec *client)
{
   struct Client_Data *cd;

   if (seat == UINT_MAX)
     {
        printf("Max clients");
        return RFB_CLIENT_REFUSE;
     }

   cd = malloc(sizeof(struct Client_Data));
   EINA_SAFETY_ON_NULL_RETURN_VAL(cd, RFB_CLIENT_REFUSE);
   client->clientData = cd;
   client->clientGoneHook = _client_gone;
   cd->fd_handler = ecore_main_fd_handler_add(client->sock, ECORE_FD_READ,
                                          _client_activity, client, NULL, NULL);
   EINA_SAFETY_ON_NULL_GOTO(cd->fd_handler, err_handler);
   printf("New client attached to seat '%u'\n", seat);
   cd->seat = seat++;
   return RFB_CLIENT_ACCEPT;

 err_handler:
   free(cd);
   return RFB_CLIENT_REFUSE;
}

static void
_keyboard_event(rfbBool down, rfbKeySym keySym, rfbClientRec *client)
{
   struct Client_Data *cd;

   cd = client->clientData;

   if (!down)
       printf("The client on seat '%u' pressed the key '%"PRIu32"'\n",
              cd->seat, keySym);
   else
       printf("The client on seat '%u' released the key '%"PRIu32"'\n",
              cd->seat, keySym);

   if (keySym == XK_Escape || keySym =='q' || keySym =='Q')
     rfbCloseClient(client);
}

static int
_get_button(int buttonMask)
{
    int i;
    for (i = 0; i < 32; i++)
        if (buttonMask >> i & 1)
            return i + 1;
    return 0;
}

static void
_pointer_event(int buttonMask, int x, int y, rfbClientPtr client)
{
   int button, buttonChanged;
   struct Client_Data *cd;

   cd = client->clientData;

   /* Apparently lastPtrX and Y wasn't updated, so maybe we need
      to keep positions on the program side. */
   printf("The client's cursor on seat '%u' is at X: %d Y: %d\n",
           cd->seat, x, y);
   /* Check if a mouse button was pressed or released */
   buttonChanged = buttonMask - client->lastPtrButtons;
   if (buttonChanged > 0) {
       button = _get_button(buttonChanged);
       printf("The client on seat '%u' pressed button: %d\n", cd->seat, button);
   } else if (buttonChanged < 0) {
       button = _get_button(-buttonChanged);
       printf("The client on seat '%u' released button: %d\n", cd->seat,
              button);
   }
}

static Evas *
_create_evas_frame(void *pixels)
{
   Evas *evas;
   Evas_Engine_Info_Buffer *einfo;
   int method;

   method = evas_render_method_lookup("buffer");
   EINA_SAFETY_ON_TRUE_RETURN_VAL(method == 0, NULL);

   evas = evas_new();
   EINA_SAFETY_ON_NULL_RETURN_VAL(evas, NULL);

   evas_output_method_set(evas, method);
   evas_output_size_set(evas, WIDTH, HEIGHT);
   evas_output_viewport_set(evas, 0, 0, WIDTH, HEIGHT);

   einfo = (Evas_Engine_Info_Buffer *)evas_engine_info_get(evas);
   EINA_SAFETY_ON_NULL_GOTO(einfo, err_einfo);

   einfo->info.depth_type = EVAS_ENGINE_BUFFER_DEPTH_ARGB32;
   einfo->info.dest_buffer = pixels;
   einfo->info.dest_buffer_row_bytes = WIDTH * sizeof(int);
   einfo->info.use_color_key = 0;
   einfo->info.alpha_threshold = 0;
   einfo->info.func.new_update_region = NULL;
   einfo->info.func.free_update_region = NULL;
   evas_engine_info_set(evas, (Evas_Engine_Info *)einfo);

   return evas;

 err_einfo:
   evas_free(evas);
   return NULL;
}

static Eina_Bool
_anim(void *data)
{
   rfbClientIteratorPtr itr;
   rfbClientRec *client;
   static enum { RIGHT, LEFT } direction = LEFT;
   static const int speed = 20;
   int x, y;
   Evas_Object *rect = data;
   Eina_List *updates, *n;
   Eina_Rectangle *update;

   evas_object_geometry_get(rect, &x, &y, NULL, NULL);
   if (direction == LEFT)
     {
        x -= speed;
        if (x <= 0)
          {
             x = 0;
             direction = RIGHT;
          }
     }
   else
     {
        x += speed;
        if (x >= WIDTH)
          {
             direction = LEFT;
             x = WIDTH;
          }
     }

   evas_object_move(rect, x, y);

   updates = evas_render_updates(evas_object_evas_get(rect));
   EINA_LIST_FOREACH(updates, n, update)
      rfbMarkRectAsModified(server, update->x, update->y, update->w, update->h);
   evas_render_updates_free(updates);

   itr = rfbGetClientIterator(server);
   while ((client = rfbClientIteratorNext(itr)) != NULL) {
      rfbUpdateClient(client);

      if (client->sock == -1)
         rfbClientConnectionGone(client);
   }
   rfbReleaseClientIterator(itr);

   return ECORE_CALLBACK_RENEW;
}

static int
_draw_objects(Evas *evas)
{
   Evas_Object *bg, *txt, *rect;

   bg = evas_object_rectangle_add(evas);
   EINA_SAFETY_ON_NULL_RETURN_VAL(bg, -1);
   evas_object_color_set(bg, 255, 255, 255, 255);
   evas_object_move(bg, 0, 0);
   evas_object_resize(bg, WIDTH, HEIGHT);
   evas_object_show(bg);

   txt = evas_object_text_add(evas);
   EINA_SAFETY_ON_NULL_RETURN_VAL(txt, -1);
   evas_object_color_set(txt, 0, 0, 0, 255);
   evas_object_text_style_set(txt, EVAS_TEXT_STYLE_PLAIN);
   evas_object_text_font_set(txt, "Sans", 15);
   evas_object_text_text_set(txt,
      "Move your mouse and press your keyboard keys. Press ESC to exit. ");
   evas_object_move(txt, 0, 0);
   evas_object_show(txt);

   rect = evas_object_rectangle_add(evas);
   EINA_SAFETY_ON_NULL_RETURN_VAL(rect, -1);
   evas_object_color_set(rect, 255, 0, 0, 255);
   evas_object_resize(rect, RECT_DIMEN, RECT_DIMEN);
   evas_object_move(rect, (WIDTH - RECT_DIMEN) /2, (HEIGHT- RECT_DIMEN)/2);
   evas_object_show(rect);

   animator = ecore_animator_add(_anim, rect);
   EINA_SAFETY_ON_NULL_RETURN_VAL(animator, -1);

   return 0;
}

static void
_sig_action(int signum)
{
   ecore_main_loop_quit();
}

static Eina_Bool
_socket_activity(void *data, Ecore_Fd_Handler *fd_handler)
{
   rfbProcessNewConnection(data);
   return ECORE_CALLBACK_RENEW;
}

int
main(int argc, char *argv[])
{
   Evas *evas;
   int r = -1;
   Ecore_Fd_Handler *fd_handler, *fd_handler6;
   struct sigaction sa;

   sa.sa_handler = _sig_action;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_RESETHAND;
   sigaction(SIGINT, &sa, NULL);

   EINA_SAFETY_ON_TRUE_RETURN_VAL(evas_init() == 0, -1);
   EINA_SAFETY_ON_TRUE_GOTO(ecore_init() == 0, err_ecore);

   server = rfbGetScreen(&argc, argv, WIDTH, HEIGHT, 8, 3, 4);
   EINA_SAFETY_ON_NULL_GOTO(server, err_server);

   server->frameBuffer = malloc(WIDTH * HEIGHT * 4);
   EINA_SAFETY_ON_NULL_GOTO(server->frameBuffer, err_buffer);

   evas = _create_evas_frame(server->frameBuffer);
   EINA_SAFETY_ON_NULL_GOTO(evas, err_evas);

   r = _draw_objects(evas);
   EINA_SAFETY_ON_TRUE_GOTO(r == -1, err_draw);
   evas_render_updates_free(evas_render_updates(evas));

   server->newClientHook = _new_client;
   server->kbdAddEvent = _keyboard_event;
   server->ptrAddEvent = _pointer_event;
   server->alwaysShared = TRUE;

   rfbInitServer(server);

   fd_handler = ecore_main_fd_handler_add(server->listenSock, ECORE_FD_READ,
                                          _socket_activity, server, NULL, NULL);
   EINA_SAFETY_ON_NULL_GOTO(fd_handler, err_handler);
   fd_handler6 = ecore_main_fd_handler_add(server->listen6Sock, ECORE_FD_READ,
                                          _socket_activity, server, NULL, NULL);
   EINA_SAFETY_ON_NULL_GOTO(fd_handler6, err_handler6);

   ecore_main_loop_begin();
   r = 0;

   ecore_main_fd_handler_del(fd_handler6);

 err_handler6:
   ecore_main_fd_handler_del(fd_handler);
 err_handler:
   ecore_animator_del(animator);
 err_draw:
   evas_free(evas);
 err_evas:
   free(server->frameBuffer);
 err_buffer:
   rfbScreenCleanup(server);
 err_server:
   ecore_shutdown();
 err_ecore:
   evas_shutdown();
   return r;
}
