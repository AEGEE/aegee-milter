#include "src/prdr-mod.h"

//these defines facilise dlpreopen later with libtool

#define prdr_mod_status	mod_relayed_LTX_prdr_mod_status
#define prdr_mod_run	mod_relayed_LTX_prdr_mod_run
#define prdr_mod_equal	mod_relayed_LTX_prdr_mod_equal

int
prdr_mod_status (void* priv UNUSED)
{
  return MOD_BODY;
}

int
prdr_mod_run (void* priv)
{
  if (prdr_get_stage (priv) < MOD_BODY)
    return -1;
  //  g_printf("stage = %i\n", prdr_get_stage(priv));
  const char *mail_from = prdr_get_envsender (priv);
  const char *rcpt_to = prdr_get_recipient (priv);
  prdr_list_insert ("log", rcpt_to, "mod_relayed, envelope from:",
		    mail_from, 0);
  prdr_list_insert ("relayed", rcpt_to /* to */, mail_from /* from */,
		    "" /*value */, 0 /* never expire */ );
  return 0;
}

int
prdr_mod_equal (const struct privdata* const priv UNUSED, const char* const user1 UNUSED, const char* const user2 UNUSED)
{
  return 1;
}
