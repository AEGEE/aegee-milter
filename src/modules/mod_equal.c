#include "src/prdr-mod.h"

int
mod_equal_LTX_prdr_mod_status (void* priv UNUSED)
{
  return MOD_RCPT | MOD_BODY;
}

int
mod_equal_LTX_prdr_mod_run (void* priv)
{
  //  char str[100];
  //  g_sprintf(str, "%d", prdr_get_stage(priv));
  char *rcpt_to = prdr_get_recipient (priv);
  //  prdr_list_insert ("log", rcpt_to, "mod_equal, stage:", str, 0);
  if (prdr_get_stage (priv) < MOD_RCPT)
    return -1;
  //  g_printf("stage = %i\n", prdr_get_stage(priv));
  char *mail_from = prdr_get_envsender (priv);
  if (prdr_get_stage (priv) == MOD_RCPT) {
  //  g_printf("***%p mod equal %s,%s***\n", priv,  prdr_get_recipient(priv),  prdr_get_envsender(priv));
    if (g_ascii_strcasecmp (rcpt_to, mail_from) == 0 &&
	g_ascii_strcasecmp (mail_from, "mail@aegee.org") ) {
      prdr_list_insert ("log", rcpt_to, "mod_equal, reject from:",
			mail_from, 0);
      prdr_set_response (priv, "550", "5.1.0",
			 "Envelope sender and recipient cannot coinside in our domain (site policy).");
    }
  } else 
      if (strcmp (mail_from, "<>") == 0) {
	const GString *body = prdr_get_body (priv);
	if (strstr (body->str, "Envelope sender and recipient cannot coinside in our domain.") != NULL) {
	  prdr_list_insert ("log", rcpt_to,
			    "mod_equal, mail already rejected (env. sender and recipient coincide)",
			    mail_from, 0);
	  prdr_set_response (priv, "550", "5.1.0", 
			    "Message contains: \"Envelope sender and recipient cannot coincide in our\r\ndomain (site policy).\" -- we have already rejected this email.");
	} else
	  if (strstr (body->str, "host aegeeserv.aegee.uni-karlsruhe.de [129.13.131.8") != NULL) {
	    prdr_list_insert ("log", rcpt_to, "mod_equal, mail already rejected (host aegeeserv)", mail_from, 0);
	    prdr_set_response (priv, "550", "5.1.0",
			      "Message contains: \"host aegeeserv.aegee.uni-karlsruhe.de [129.13.131.80]:\"\r\n -- we have already rejected this email.");
	  }
      }
    //  g_printf("---%p mod equal reject---\n", priv);
  //  g_printf("---%p mod equal---\n", priv);
  return 0;
}

int
mod_equal_LTX_prdr_mod_equal (struct privdata* priv UNUSED, char* user1 UNUSED, char* user2 UNUSED)
{
  return 1;
}
