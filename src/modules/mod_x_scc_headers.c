#include "prdr-mod.h"


//these defines facilise dlpreopen later with libtool

int
mod_x_scc_headers_LTX_prdr_mod_status (void* priv UNUSED)
{
  return MOD_BODY;
}

#include <libmilter/mfapi.h>

int
mod_x_scc_headers_LTX_prdr_mod_run (void* priv)
{
  struct privdata *cont = (struct privdata*) priv;
  const char** x_scc_spam_level = prdr_get_header(priv, "X-SCC-Spam-Level");
  if (x_scc_spam_level) {
    prdr_list_insert ("log", prdr_get_recipient (cont),
	    "mod_x_scc_headers, X-SCC-Spam-Level:", *x_scc_spam_level, 0);
    g_free(x_scc_spam_level);
  }
  smfi_chgheader (cont->ctx, "X-SCC-Debug-Zeilenzahl", 1, NULL);
  smfi_chgheader (cont->ctx, "X-Scan-Signature", 1, NULL);
  smfi_chgheader (cont->ctx, "X-Scan-Server", 1, NULL);
  smfi_chgheader (cont->ctx, "X-SCC-Spam-Level", 1, NULL);
  smfi_chgheader (cont->ctx, "X-SCC-Spam-Status", 1, NULL);
  return 0;
}

int
mod_x_scc_headers_LTX_prdr_mod_equal (struct privdata* priv UNUSED , char* user1 UNUSED, char* user2 UNUSED)
{
  return 1;
}
