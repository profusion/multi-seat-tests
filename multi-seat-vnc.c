#include <stdio.h>
#include <stdlib.h>
#include <Evas.h>
#include <Evas_Engine_Buffer.h>
#include <rfb/rfb.h>
#include <rfb/keysym.h>
#include <limits.h>
#include <Eina.h>
#include <signal.h>

#define WIDTH (800)
#define HEIGHT (600)

static unsigned seat = 1;
static rfbScreenInfoPtr server = NULL;

static void
_client_gone(rfbClientRec *client)
{
   unsigned *s;

   seat--;
   s = client->clientData;
   printf("Client on seat '%u' is gone\n", *s);
   free(client->clientData);
}

static enum rfbNewClientAction
_new_client(rfbClientRec *client)
{
   unsigned *s;

   if (seat == UINT_MAX)
     {
        printf("Max clients");
        return RFB_CLIENT_REFUSE;
     }

   printf("New client attached to seat '%u'\n", seat);
   s = malloc(sizeof(unsigned));
   EINA_SAFETY_ON_NULL_RETURN_VAL(s, RFB_CLIENT_REFUSE);
   client->clientData = s;
   client->clientGoneHook = _client_gone;
   *s = seat++;
   return RFB_CLIENT_ACCEPT;
}

static void
_keyboard_event(rfbBool down, rfbKeySym keySym, rfbClientRec *client)
{
   unsigned *s;

   s = client->clientData;
   if (!down)
       printf("The client on seat '%u' pressed the key '%"PRIu32"'\n",
              *s, keySym);
   else
       printf("The client on seat '%u' released the key '%"PRIu32"'\n",
              *s, keySym);

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
   unsigned *s;
   int button, buttonChanged;

   s = client->clientData;

   /* Apparently lastPtrX and Y wasn't updated, so maybe we need
      to keep positions on the program side. */
   printf("The client's cursor on seat '%u' is at X: %d Y: %d\n",
           *s, x, y);
   /* Check if a mouse button was pressed or released */
   buttonChanged = buttonMask - client->lastPtrButtons;
   if (buttonChanged > 0) {
       button = _get_button(buttonChanged);
       printf("The client on seat '%u' pressed button: %d\n", *s, button);
   } else if (buttonChanged < 0) {
       button = _get_button(-buttonChanged);
       printf("The client on seat '%u' released button: %d\n", *s, button);
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

static int
_draw_objects(Evas *evas)
{
   Evas_Object *bg, *txt;

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

   return 0;
}

static void
_sig_action(int signum)
{
   rfbShutdownServer(server, TRUE);
}

int
main(int argc, char *argv[])
{
   Evas *evas;
   int r = -1;

   struct sigaction sa;

   sa.sa_handler = _sig_action;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_RESETHAND;
   sigaction(SIGINT, &sa, NULL);

   EINA_SAFETY_ON_TRUE_RETURN_VAL(evas_init() == 0, -1);

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
   rfbRunEventLoop(server, -1, FALSE);
   r = 0;
 err_draw:
   evas_free(evas);
 err_evas:
   free(server->frameBuffer);
 err_buffer:
   rfbScreenCleanup(server);
 err_server:
   evas_shutdown();
   return r;
}
