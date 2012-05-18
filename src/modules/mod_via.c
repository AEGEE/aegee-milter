#include "prdr-mod.h"
#include <time.h>

int
mod_via_LTX_load ()
{
  return 0;
}

int
mod_via_LTX_prdr_mod_status (void* priv UNUSED)
{
  return MOD_HEADERS;
}

static inline char*
received_via (char * received)
{
  char *start = strchr (received, '[');
  if (start) {
    start++;
    char *end = strchr (start, ']');
    if (end)
      return g_strndup (start, end - start);
  }
  return NULL;
}

int
mod_via_LTX_prdr_mod_run (void* priv)
{
  struct privdata *cont = (struct privdata*) priv;
  char** received = prdr_get_header (cont, "Received");
  if (received && received[0]) {
    char *ip = received_via (received[0]);
    if (ip) {
      prdr_list_insert ("log", prdr_get_recipient (cont), "mod_via", ip, 0);
      g_free(ip);
    }
  }
  if (received) g_free (received);
  return 0;
}

int
mod_via_LTX_prdr_mod_equal (struct privdata* priv UNUSED, char* user1 UNUSED, char* user2 UNUSED) {
  return 1;
}
/*
int
mod_via_LTX_prdr_mod_init_msg (void* private)
{
  //time_t *i = g_malloc(sizeof(time_t));
  //time(i);
  //prdr_set_priv_msg(private, i);
}

int
mod_via_LTX_prdr_mod_destroy_msg (void *private)
{
  //time_t *i = (time_t*) prdr_get_priv_msg(private);
  //if (i) g_free(i);
}
*/
