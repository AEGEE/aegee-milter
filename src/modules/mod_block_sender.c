#include "prdr-mod.h"
#include <time.h>

extern GKeyFile* prdr_inifile;

int
mod_block_sender_LTX_prdr_mod_status (void* priv UNUSED)
{
  return MOD_RCPT;
}

int
mod_block_sender_LTX_prdr_mod_run (void* priv)
{
  struct privdata *cont = (struct privdata*) priv;
  char* sender = prdr_get_envsender(cont);
  if (strcasecmp(sender, "renekeijzer@mail.com") == 0) {
    prdr_set_response (priv, "550", "5.7.0", "Rene, your mailbox is hacked and until you resolve the issue AEGEE.org is not going to accept mails from it.\r\n\r\n   Dilyan // AEGEE Mail Team ");
    prdr_list_insert ("log", sender, "mod_block_sender", "blocked", 0);
  }
  return 0;
}

int
mod_block_sender_LTX_prdr_mod_equal (struct privdata* priv UNUSED, char* user1 UNUSED, char* user2 UNUSED) {
  return 1;
}
