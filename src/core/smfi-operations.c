#define MILTPRIV  struct privdata *priv = (struct privdata *) smfi_getpriv(ctx);
#include <stdlib.h>
#include <arpa/inet.h>
#include <libmilter/mfapi.h>
#include <glib/gprintf.h>
#include "src/prdr-milter.h"

extern int bounce_mode;
extern unsigned int num_so_modules;
extern struct so_module **so_modules;

//-----------------------------------------------------------------------------
static sfsistat
prdr_connect(SMFICTX *ctx,
	     char* hostname,
	     struct sockaddr *hostaddr)
{
  /*  static int invocations = 0;
  if (invocations % 5 == 0)
    g_printf ("%5d\n", invocations / 5);
  if (invocations++ == 250)
  smfi_stop();*/
  //  printf("***prdr_connect %p***\n", ctx);
  struct privdata *priv;
  priv = g_malloc0 (sizeof (struct privdata));
  priv->gstr = g_string_chunk_new (4096);
  priv->hostname = g_string_chunk_insert (priv->gstr, hostname);
  priv->domain_name = g_string_chunk_insert (priv->gstr,
					     smfi_getsymval (ctx, "j"));

  if (hostaddr != NULL) {
      switch(hostaddr->sa_family) {
          case AF_INET:
	      priv->hostaddr = g_malloc (sizeof (struct sockaddr_in));
              memcpy (priv->hostaddr, hostaddr, sizeof (struct sockaddr_in));
              break;
          case AF_INET6:
	      priv->hostaddr = g_malloc (sizeof(struct sockaddr_in6));
	      memcpy (priv->hostaddr, hostaddr, sizeof (struct sockaddr_in6));
              break;
      }
  } else
      priv->hostaddr = NULL;
  priv->ctx = ctx;
  priv->msgpriv = g_new0 (void *, num_so_modules);
  smfi_setpriv (ctx, priv);
  //printf("---prdr_connect %p---\n", ctx);
  return SMFIS_CONTINUE;
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_helo (SMFICTX *ctx, char* helohost)
{
  //printf ("***prdr_helo %p***\n", ctx);
  MILTPRIV
  priv->ehlohost = g_string_chunk_insert (priv->gstr, helohost);
  priv->stage = MOD_EHLO;
  for (priv->size = 0; priv->size < num_so_modules; priv->size++) {
    if (so_modules[priv->size]->init_msg) {
      if (so_modules[priv->size]->destroy_msg && priv->msgpriv[priv->size])
	so_modules[priv->size]->destroy_msg (priv);
      so_modules[priv->size]->init_msg (priv);
    }
    if (so_modules[priv->size]->status (priv) & priv->stage)
      so_modules[priv->size]->run (priv);
  } 
  priv->stage = MOD_MAIL;
  //printf("---prdr_helo %p, time=%li---\n", ctx, time(NULL));
  return SMFIS_CONTINUE;
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_envfrom (SMFICTX *ctx, char **argv)
{
  //  printf ("***prdr_envfrom %p : %s***\n", ctx, argv[0]);
  if (argv[0] == NULL) return SMFIS_TEMPFAIL;
  MILTPRIV
  clear_message (priv->msg);
  priv->msg = g_malloc0 (sizeof (struct message));
  priv->msg->envfrom = normalize_email (priv, argv[0]);
  priv->prdr_supported = 0;
  clear_recipients (priv);
  priv->queue_id = g_string_chunk_insert (priv->gstr,
					  smfi_getsymval (ctx, "i"));
  if (priv->stage != MOD_MAIL) {
    priv->stage = MOD_MAIL;
    for (priv->size = 0; priv->size < num_so_modules; priv->size++)
      if (so_modules[priv->size]->init_msg)
	so_modules[priv->size]->init_msg (priv);
  }
  for (priv->size = 0; priv->size < num_so_modules; priv->size++)
    if (so_modules[priv->size]->status (priv) & MOD_MAIL)
      so_modules[priv->size]->run (priv);
  priv->mime8bit = 0;
  int i = 1;
  priv->size = 0;
  //check for particular SMTP extensions that the client supports
  while (argv[i++])
    //check for PRDR extension
    if (!g_ascii_strcasecmp (argv[i-1], "PRDR"))
      priv->prdr_supported = 1;
    else
    //check for SIZE= extension
      if (g_ascii_strncasecmp (argv[i-1], "SIZE=", 5) == 0) {
	priv->size = atol (argv[i-1]+5);
	// printf ("SIZE=%i\n", priv->size);
      } else
	//check for BODY= extension
	if (g_ascii_strcasecmp (argv[i-1], "BODY=8BITMIME")==0)
	  priv->mime8bit = 1;
  priv->stage = MOD_RCPT;
  priv->msg->headers = g_ptr_array_new ();
  //printf("---prdr_envfrom %p---\n", ctx);
  priv->recipients = g_ptr_array_new ();
  return SMFIS_CONTINUE;
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_envrcpt (SMFICTX *ctx, char **argv)
{
  //  printf ("***prdr_envrcpt %p***\n", ctx);
  MILTPRIV
  priv->current_recipient = g_malloc0 (sizeof (struct recipient));
  priv->current_recipient->address = normalize_email (priv, argv[0]);
  priv->current_recipient->modules = (struct module**)g_new0 (struct module*,
							      num_so_modules);
  unsigned int k = 1;
  unsigned int temp_size = priv->size;
  while (argv[k])
    if (g_ascii_strcasecmp (argv[k++], "NOTIFY=NEVER") == 0)
      priv->current_recipient->flags |= RCPT_NOTIFY_NEVER;
  int j;
  for (k = 0; k < num_so_modules; k++) {
    priv->stage = MOD_EHLO;
    priv->size = k;
    for (j = 0; j < (int)priv->recipients->len; j++) {
      struct recipient *rec = g_ptr_array_index (priv->recipients, j);
      if (so_modules[k]->equal (priv, rec->address,
				priv->current_recipient->address)) {
	priv->current_recipient->modules[k] = rec->modules[k];
	j = -1;
	break;
      }
    }
    priv->stage = MOD_RCPT;
    priv->size = temp_size;
    if (j != -1 || priv->recipients->len == 0) {
      //      printf ("===modules are not equal===\n");
      priv->current_recipient->modules[k] = g_malloc0 (sizeof (struct module));
      priv->current_recipient->modules[k]->so_mod = so_modules[k];
      priv->current_recipient->modules[k]->msg = g_malloc0 (sizeof (struct
								    message));
      priv->current_recipient->current_module =
	priv->current_recipient->modules[k];
      //      priv->current_recipient->current_module->return_code = g_strdup ("250");
      if (so_modules[k]->init_rcpt)
	so_modules[k]->init_rcpt (priv);
      priv->module_pool = g_slist_prepend (priv->module_pool,
					   (gpointer)priv->current_recipient->modules[k]);
    }
  }//for (i = 0; i < num_so_modules; i++)
  int i = apply_modules (priv);
  if (i > 0) {//some module rejected the message, without prior fails
    inject_response (priv->ctx,
		     priv->current_recipient->modules[i-1]->return_code,
		     priv->current_recipient->modules[i-1]->return_dsn,
		     priv->current_recipient->modules[i-1]->return_reason);
    i = priv->current_recipient->modules[i-1]->smfi_const;
    clear_recipient (priv->current_recipient);
    priv->current_recipient = NULL;
    //printf ("---prdr_envrcpt REJECTED priv->current_recipient =%p, %p, time=%li---\n", priv->current_recipient, ctx, time(NULL));
    return i;
  } else { //a module failed (i ==-1), or has not rejected the recipient (i==0)
    j = SMFIS_CONTINUE;
    if (bounce_mode < 2 && priv->recipients->len > 0)
      //bounce mode is delayed, or pseudo-delayed, prdr is not supported and another recipient has been accepted so far
      for (k = 0; k < num_so_modules; k++) {
	struct recipient *rec = g_ptr_array_index (priv->recipients, 0);
	if ((rec->modules[k] != priv->current_recipient->modules[k])
	    && ((priv->current_recipient->flags & RCPT_NOTIFY_NEVER) == 0) ) {
	    //if the modules are not equivalent to the modules of the first accepted recipient
	  priv->current_recipient->flags |= RCPT_PRDR_REJECTED;
	  if (priv->prdr_supported == 0)
	    j = SMFIS_TEMPFAIL;
	  break;
	}
      }
    if ( bounce_mode != 0 || j == SMFIS_CONTINUE /*|| *priv->msg->envfrom == '\0' */) {
      g_ptr_array_add (priv->recipients, priv->current_recipient);
    } else {
      clear_recipient (priv->current_recipient);
      priv->current_recipient = NULL;
    }
    return j;
  }
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_header (SMFICTX *ctx,
	     char* headerf,
	     char* headerv)
{
  //printf("***prdr_header***\n");
  MILTPRIV
  int i = 0;
  while (headerf[i])
    if (headerf[i++] <= 0 )
      //header field contains non-ascii characters
      {
	char *t = g_malloc(39 + strlen (headerf));
	g_sprintf (t, "Header %s contains non-ascii characters", headerf);
	inject_response (ctx, "550", NULL, t);
	g_free (t);
	return SMFIS_REJECT;
      }
  struct header *h;
  h = g_malloc (sizeof (struct header));
  h->field = g_string_chunk_insert_const (priv->gstr, headerf);
  h->value = g_string_chunk_insert (priv->gstr, headerv);
  g_ptr_array_add (priv->msg->headers, h);
  //  printf("---prdr_header---\n");
  return SMFIS_CONTINUE;
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_eoh (SMFICTX *ctx)
{
  MILTPRIV
  //printf ("***prdr_eoh %p***\n", ctx);
  priv->stage = MOD_HEADERS;
  unsigned int i, k;
  int j=0;
  //if bounce_mode == pseudo_delayed & no Message-ID: -> delete the PRDR-rejected recipients
  /*
  if (bounce_mode == 1) {
    char **t = prdr_get_header (priv, "Message-ID");
    if (t) {
      if (t[0] == NULL) {
       //the recipient shall be ignored
	for (i = 0; i < priv->num_recipients; i++)
	  if (priv->recipients[i]->flags & RCPT_PRDR_REJECTED ) //remove the recipient
	    clear_recipient (priv->recipients[i]);
      };
      g_free(t);
    }
  }
  */
  for (i = 0; i < priv->recipients->len; i++) {
    priv->current_recipient = g_ptr_array_index (priv->recipients, i);
    j = apply_modules (priv);
    if (j == -1) break;//some module for this recipient failed
  }
  if (j > 0) { //no module failed and some module rejected the message
    set_responses (priv);
    clear_privdata (priv);
    //clear_recipients (priv); This shall not be here, but it seems _eom or _abort is not called, when _eoh returns REJECT
    //    printf ("---prdr_eoh %p SMFIS_REJECT---\n", ctx);
    return SMFIS_REJECT;
  }
  if (j == 0) //no module failed, message accepted
    for (i = 0; i < priv->recipients->len; i++) {
      priv->current_recipient = g_ptr_array_index (priv->recipients, i);
      for (k = 0; k < num_so_modules; k++) {
	priv->current_recipient->current_module =
	  priv->current_recipient->modules[k];
	if ((so_modules[k]->status (priv) & MOD_BODY) &&
	    prdr_get_activity (priv,
			       priv->current_recipient->current_module->so_mod->name) != 2) {
	  j =-2;//no module failed but body needed
	  break;
	}
      }
    }
  //  else ; 
  //  printf("***prdr_eoh %p j is %i***\n", ctx, j);
  if (j == 0) {
    //no module needs the body! and there are no errors yet
    clear_message (priv->msg);
    priv->msg = NULL;
    //    printf("---prdr_eoh---no body needed %p---\n", ctx);
  } else
    priv->msg->body = g_string_new (NULL);
  return SMFIS_CONTINUE;
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_body (SMFICTX *ctx, unsigned char* bodyp, const size_t len)
{
  //  printf ("***prdr_body %p***\n", ctx);
  if (len == 0) return SMFIS_CONTINUE;
  MILTPRIV
  if (priv->msg == NULL)
    return SMFIS_SKIP;
  g_string_append_len (priv->msg->body, (char*)bodyp, len);
  //  printf("---prdr_body %p---\n", ctx);
  return SMFIS_CONTINUE;
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_eom (SMFICTX *ctx)
{
  //  printf ("***prdr_eom %p***\n", ctx);
  MILTPRIV
  if (priv->msg) {
  //calculate message size
    priv->stage = MOD_BODY;
    unsigned int i;
    for (i = 0; i < priv->recipients->len; i++) {
      priv->current_recipient = g_ptr_array_index (priv->recipients, i);
      apply_modules (priv);
    }
    //printf("prdr_eom: %p E\n", ctx);
  }
  int j = set_responses (priv);
  clear_privdata (priv);
  //printf("---prdr_eom %p---\n", ctx);
  return j;
}
//-----------------------------------------------------------------------------

static sfsistat
prdr_close (SMFICTX *ctx)
{
  MILTPRIV
  if (priv == NULL || ctx == NULL) return SMFIS_CONTINUE;
  priv->stage = MOD_EHLO;
  clear_module_pool (priv);
  for (priv->size = 0; priv->size < num_so_modules; priv->size++)
    if (so_modules[priv->size]->destroy_msg && priv->msgpriv[priv->size]) {
      so_modules[priv->size]->destroy_msg (priv);
    }
  if (priv->hostaddr) {
      g_free (priv->hostaddr);
      priv->hostaddr = NULL;
  }
  g_free (priv->msgpriv);
  g_string_chunk_clear (priv->gstr);
  g_string_chunk_free (priv->gstr);

  smfi_setpriv (ctx, NULL);
  g_free (priv);
  //printf("---prdr_close %p, time=%li---\n", ctx, time(NULL));
  return SMFIS_CONTINUE;
}

//-----------------------------------------------------------------------------

static sfsistat
prdr_abort (SMFICTX *ctx)
{
  //  printf("***prdr_abort %p***\n", ctx);
  MILTPRIV
  if (priv == NULL || ctx == NULL) return SMFIS_CONTINUE;
  //printf("prdr_abort: priv->recipients = %p\n", priv->recipients);
  clear_privdata (priv);
  //printf("---prdr_abort %p, time=%li---\n", ctx, time(NULL));
  return SMFIS_CONTINUE;
}
/*
//-----------------------------------------------------------------------------
static sfsistat prdr_negotiate(SMFICTX *ctx, unsigned long f0, unsigned long f1, unsigned long f2, unsigned long f3, unsigned long *fp0, unsigned long *fp1, unsigned long *fp2, unsigned long *fp3){
//  printf("***prdr_negotiate***\n");
  *fp0 = f0;
  *fp1 = f1;// | SMFIP_HDR_LEADSPC;
  *fp2 = f2;
  *fp3 = f3;
//  printf("---prdr_negotiate---\n");
  return SMFIS_CONTINUE;
}
*/
//-----------------------------------------------------------------------------
struct smfiDesc smfilter =
{
    "aegee-milter", /* filter name */
    SMFI_VERSION, /* version code -- do not change */
    SMFIF_ADDHDRS | SMFIF_CHGHDRS | SMFIF_CHGBODY
    | SMFIF_ADDRCPT | SMFIF_DELRCPT | SMFIF_CHGFROM,    /* flags */
    prdr_connect, /* connection info filter */
    prdr_helo, /* SMTP HELO command filter */
    prdr_envfrom, /* envelope sender filter */
    prdr_envrcpt, /* envelope recipient filter */
    prdr_header, /* header filter */
    prdr_eoh, /* end of header */
    prdr_body, /* body block filter */
    prdr_eom, /* end of message */
    prdr_abort, /* message aborted */
    prdr_close, /* connection cleanup */
    NULL, /* xxfi_unknown, any unrecognized or unimplemeted command filter */
    NULL, /* xxfi_data, SMTP DATA command filter */
    NULL, //prdr_negotiate /* negotiation callback */
  };
#undef MILTPRIV
