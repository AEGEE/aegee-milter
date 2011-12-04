#include "prdr-mod.h"
#include <glib/gstdio.h>


//these defines facilise dlpreopen later with libtool

#define prdr_mod_status	mod_simple_LTX_prdr_mod_status
#define prdr_mod_run	mod_simple_LTX_prdr_mod_run
#define prdr_mod_equal	mod_simple_LTX_prdr_mod_equal
#define prdr_mod_init_rcpt	mod_simple_LTX_prdr_mod_init_rcpt
#define prdr_mod_init_msg	mod_simple_LTX_prdr_mod_init_msg
#define prdr_mod_destroy_msg	mod_simple_LTX_prdr_mod_destroy_msg
#define prdr_mod_destroy_rcpt	mod_simple_LTX_prdr_mod_destroy_rcpt
#define load			mod_simple_LTX_load
#define unload			mod_simple_LTX_unload

int
prdr_mod_status (void* priv UNUSED)
{
  return MOD_RCPT | MOD_HEADERS;
}

int
prdr_mod_run (void* priv UNUSED)
{
  return 0;
}

int
prdr_mod_equal (struct privdata* priv UNUSED, char* user1 UNUSED, char* user2 UNUSED)
{
  return 1;
}

int
prdr_mod_init_rcpt (void* private)
{
  void *i = g_malloc(1024);
  g_printf("MOD SIMPLE prdr_mod_init_rcpt=%p\n", i);
  prdr_set_priv_rcpt(private, i);
  return 0;
}

int
prdr_mod_init_msg (void* private)
{
  void *i = g_malloc(1024);
  g_printf ("MOD SIMPLE prdr_mod_init_msg=%p\n", i);
  prdr_set_priv_msg (private, i);
  return 0;
}

int
prdr_mod_destroy_msg (void *private)
{
  void* i = prdr_get_priv_msg (private);
  g_free (i);
  g_printf ("MOD SIMPLE prdr_mod_destroy_msg = %p\n", i);
  return 0;
}

int
prdr_mod_destroy_rcpt (void *private)
{
  void* i = prdr_get_priv_rcpt (private);
  g_printf ("MOD SIMPLE prdr_mod_destroy_rcpt = %p\n", i);
  g_free (i);
  return 0;
}
