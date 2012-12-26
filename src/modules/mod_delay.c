#include <glib/gstdio.h>
#include <time.h>
#include <sys/time.h>
#include "src/prdr-mod.h"

static int delay_ehlo = 0;
static int delay_mail = 10;
static int delay_rcpt = 5;
static int delay_data = 30;
extern GKeyFile* prdr_inifile;
extern char* prdr_section;

int
mod_delay_LTX_load ()
{
  char **array = g_key_file_get_keys (prdr_inifile, prdr_section, NULL, NULL);
  int i = 0;
  while (array[i])
    if (strcmp (array[i++], "ehlo") == 0)
      delay_ehlo = g_key_file_get_integer (prdr_inifile, prdr_section,
					   "ehlo", NULL); else
    if (strcmp (array[i-1], "mail") == 0)
      delay_mail = g_key_file_get_integer (prdr_inifile, prdr_section,
					   "mail", NULL); else
    if (strcmp (array[i-1], "rcpt") == 0)
      delay_rcpt = g_key_file_get_integer (prdr_inifile, prdr_section,
					   "rcpt", NULL); else
    if (strcmp (array[i-1], "data") == 0)
      delay_data = g_key_file_get_integer (prdr_inifile, prdr_section,
					   "data", NULL); else {
      g_printf ("option %s in section [mod_delay] is not recognized\n",
		array[i-1]);
      g_strfreev (array);
      return -1;
    }
  g_strfreev (array);
  return 0;
}

int
mod_delay_LTX_prdr_mod_status (UNUSED void* priv)
{
  return MOD_EHLO | MOD_MAIL | MOD_RCPT | MOD_BODY;
}

int
mod_delay_LTX_prdr_mod_run (void* priv)
{
  struct privdata *cont = (struct privdata*) priv;

  time_t *last_time = (time_t*)prdr_get_priv_msg(cont);
  if (last_time == NULL) return 0;
  time_t current_time = time(NULL);
  long diff = (long) difftime(current_time, *last_time);
  switch (prdr_get_stage(cont)) {
  case MOD_EHLO:
    diff = delay_ehlo - diff;
    break;
  case MOD_MAIL:
    diff = delay_mail - diff;
    break;
  case MOD_RCPT:
    diff = delay_rcpt - diff;
    break;
  case MOD_BODY:
    diff = delay_data - diff;
    break;
  }
  if (diff > 0) {
    struct timeval timeout = {diff, 0};
    select(0, NULL, NULL, NULL, &timeout);
  }
  if (prdr_get_stage(cont) == MOD_BODY) {
    g_free(last_time);
    prdr_set_priv_msg(priv,NULL);
  } else 
    time(last_time);
  return 0;
}

int
mod_delay_LTX_prdr_mod_equal (UNUSED struct privdata* priv, UNUSED char* user1, UNUSED char* user2) {
  return 1;
}

int
mod_delay_LTX_prdr_mod_init_msg (void* private)
{
  time_t *i = g_malloc(sizeof(time_t));
  time(i);
  prdr_set_priv_msg(private, i);
  return 0;
}

int
mod_delay_LTX_prdr_mod_destroy_msg (void *private)
{
  time_t *i = (time_t*) prdr_get_priv_msg(private);
  if (i) g_free(i);
  return 0;
}
