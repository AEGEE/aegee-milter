#include "prdr-list.h"

static inline void
clear_ehlo (struct privdata *priv)
{
}

//clears all message-oriented data
static inline void
clear_message (struct message* msg)
{
  if (msg) {
    //g_printf("***clear_message***\n");
    unsigned int i;
    if (msg->envrcpts)
      g_free (msg->envrcpts);
    if (msg->body)
      g_string_free (msg->body, TRUE);
    if (msg->headers) {
      for (i = 0; i < msg->headers->len; i++)
	g_free(g_ptr_array_index (msg->headers, i));
      g_ptr_array_free (msg->headers, 1);
    }
    g_free (msg);
    //g_printf("---clear_message---\n");
  }
}
//-----------------------------------------------------------------------------
//clears all recipient oriented data
static inline void
clear_recipient (struct recipient *rec)
{
  if (rec) {
    g_free (rec->modules);
    g_free (rec);
  }
}

static inline void
clear_recipients (struct privdata* const priv)
{
  if (priv->recipients) {
    unsigned int i;
    for (i = 0; i < priv->recipients->len; i++) {
      clear_recipient ( g_ptr_array_index (priv->recipients, i));
    }
    g_ptr_array_free (priv->recipients, TRUE);
    priv->recipients = NULL;
  }
}

static inline void
clear_module_pool (struct privdata* const priv)
{
  if (priv->module_pool == NULL) return;
  unsigned int i;
  GSList *temp = priv->module_pool;
  priv->current_recipient = g_malloc (sizeof(struct recipient));
  while (temp) {
    struct module *mod = (struct module*) temp->data;
    if (mod->so_mod->destroy_rcpt && mod->private)
      for (i = 0; i < num_so_modules; i++)
	if (mod->so_mod == so_modules[i]) {
	  priv->current_recipient->current_module = mod;
	  mod->so_mod->destroy_rcpt (priv);
	  break;
	}
    clear_message (mod->msg);
    g_free(mod);
    temp = g_slist_next (temp);
  }
  g_free (priv->current_recipient);
  priv->current_recipient = NULL;
  g_slist_free (priv->module_pool);
  priv->module_pool = NULL;
}

//-----------------------------------------------------------------------------

static inline void
clear_privdata (struct privdata* const priv)
{
  //  g_printf ("***clear_privdata %p***\n", priv);
  if (priv == NULL) return;
  clear_message (priv->msg);
  priv->msg = NULL;
  priv->stage = MOD_MAIL;
  for (priv->size = 0; priv->size < num_so_modules; priv->size++)
    if (so_modules[priv->size]->destroy_msg && priv->msgpriv[priv->size]) {
      so_modules[priv->size]->destroy_msg (priv);
      priv->msgpriv[priv->size] = NULL;
    }
  clear_recipients (priv);
  priv->stage = 0;
  //g_printf ("---clear_privdata--- %p\n", priv);
}
//-----------------------------------------------------------------------------

static inline int 
inject_response (SMFICTX *ctx,
		 char* const code,
		 char* const dsn,
		 char* const reason)
{
  //g_printf("***inject_response***\n");
  if  (code == NULL || code[0] == '2'
       || dsn == NULL || reason == NULL) return 0;
  int m =0;
  while (reason[m] != '\0') {
    if (reason[m] == '\t' || reason[m] == '%') reason[m] = ' ';
    m++;
    }
  char* p[33];
  p[0] = strchr (reason, '\n');
  if ( p[0] == NULL ) {
    /* struct privdata *priv = (struct privdata *) smfi_getpriv(ctx);
     char *str = g_strconcat (code, ":", dsn, ":", reason, NULL);
    prdr_list_insert ("log", priv->current_recipient->address,
			"inject_response 1: ", str, 0);
			g_free(str); */
    //g_printf("---inject_response---\n");
    return smfi_setreply (ctx, code, dsn, reason);
  }
  m = 0;
  //MAX length per reply line 512 octets
  while (p[m] && m < 32) {
    p[m][ (p[m][-1] == '\r') ? -1 : 0] = '\0';
    p[m] += 1;
    p[m+1] = strchr (p[m], '\n');
    m++;
  };
  m--;
  while ((m >= 0) && (strlen(p[m]) == 0))
	 p[m--] = NULL;
  //g_printf("---inject_ml_response---\n");
  return smfi_setmlreply (ctx, code, dsn, reason,
			  p[ 0], p[ 1], p[ 2], p[ 3], p[ 4], p[ 5], p[ 6], 
                          p[ 7], p[ 8], p[ 9], p[10], p[11], p[12], p[13], 
                          p[14], p[15], p[16], p[17], p[18], p[19], p[20], 
                          p[21], p[22], p[23], p[24], p[25], p[26], p[27], 
                          p[28], p[29], p[30], NULL);
}

//-----------------------------------------------------------------------------
//moves headers of global modules to the headers of the original message
//checks for module Nr. i
static inline void
compact_headers (struct privdata* const priv, unsigned int i)
{
  //g_printf("***compact_headers***\r\n");
  //check if the initial modules are global
  unsigned char j;
  struct recipient *rec;
  struct module* mod;
  //  rec = (struct recipient*) g_ptr_array_index (priv->recipients, 0);
  mod = priv->current_recipient->modules[i];
  if (prdr_get_activity(priv, mod->so_mod->name) == 2 ||
      mod->msg->headers == NULL) //mod is inactive or does not change headers
    return;
  for ( j = 0; j < priv->recipients->len; j++) {
    rec = (struct recipient*) g_ptr_array_index (priv->recipients, j);
    if (mod != rec->modules[i]) {
      g_printf ("***compact_headers : module is not global***\r\n");
      return; //module is not global
    }
  }
  //move headers from global modules to global space
  while(mod->msg->headers->len) {
    struct header* h = (struct header*) g_ptr_array_index (mod->msg->headers,
							   0);
    if (h->status & 0xA0 /* 1xxxxxxx */ ) { //delete headers
      smfi_chgheader (priv->ctx, h->field, h->status & 0x8F, "");  //delete header via libmilter
    } else {  //h->status has the form 0xxxxxxx => add module
      if (h->status == 0) {//add to the end
	smfi_addheader (priv->ctx, h->field, h->value);
      } else {
	smfi_addheader (priv->ctx, h->field, h->value); //FIXME
      }
    } //end add/delete headers
    g_free (g_ptr_array_remove_index( mod->msg->headers, 0));//delete header from module's headers
  }
  g_ptr_array_free (mod->msg->headers, 1);
  mod->msg->headers = NULL;
  //g_printf("---compact_headers header operatins performed---\r\n");
}

//-----------------------------------------------------------------------------
static inline sfsistat 
set_responses (struct privdata* priv)
{
  //g_printf("***set_responses, num_recipients =%i, num_so_modules =%i***\n", priv->recipients->len, num_so_modules);
  unsigned int j, k = 0, n, p;
  //  char *temp;
  int i = -2, m;
  //check if all recipients agree on the same answer
  for (j = 0; j < priv->recipients->len; j++) {
    priv->current_recipient = g_ptr_array_index (priv->recipients,j);
    m = 0;
    for (k = 0; k < num_so_modules; k++) {
      priv->current_recipient->current_module =
	priv->current_recipient->modules[k];
      if (prdr_get_activity(priv, so_modules[k]->name) != 2
	  && priv->current_recipient->current_module->smfi_const
	  != SMFIS_CONTINUE) {
	m = priv->current_recipient->current_module->smfi_const;
	break;
      }
    }//end for k
    if (m != i) {
      if (i == -2) i = m;
      else {
	i = -1;
	break;
      }
    }
  }//end for j
  //g_printf("***set_responses A***\n");
  //proceed global modules

  //add global headers
  for (n = 0; n < num_so_modules; n++)
    compact_headers (priv, n);
  //add global recipients
  //proceed private modules
  //add private headers
  //add private recipients
  for (p =0; p < priv->recipients->len; p++) {
    priv->current_recipient = g_ptr_array_index (priv->recipients, p);
    for (n = 0; n < num_so_modules; n++)
      if (prdr_get_activity (priv, so_modules[n]->name) != 2) {
	priv->current_recipient->current_module =
	  priv->current_recipient->modules[n];
	char** recipients = prdr_get_recipients (priv);
	if (recipients[0] == NULL)
	  smfi_delrcpt (priv->ctx, priv->current_recipient->address);
	else {
	  if (g_ascii_strcasecmp (recipients[0],
				  priv->current_recipient->address)) {
	    smfi_delrcpt(priv->ctx, priv->current_recipient->address);
	  };
	  m = 1;
	  while (recipients[m]) {
	    g_printf ("recipient # %i=%s\n", m, recipients[m]);
	    smfi_addrcpt (priv->ctx, recipients[m++]);
	  }
	}
	g_free (recipients);
      }
  }

  //g_printf("***set_responses B, i=%i***\n", i);
  switch (i) {
  case 0://all modules accept the message
    //g_printf("set_responses: all modules accept the message\n");
    i = SMFIS_CONTINUE;
    break;
  case -1://modules disagree on total acceptance/rejection
    //g_printf("set_responses: modules disagree on total acceptance/rejection\n");
    if (priv->prdr_supported) {
      /*
      //prdr is suppored
      char **ret_string = g_malloc0 ((1+priv->recipients->len)
				     * sizeof (char*));
      char **rcodes = g_malloc0 ((2+priv->recipients->len) * sizeof (char*));
      char **xcodes = g_malloc0 ((1+priv->recipients->len) * sizeof (char*));
      rcodes[0] = "353";
      ret_string[0] = "content analysis has started";
      unsigned int l;
      for (j = 1; j <= priv->recipients->len; j++) {
	l = SMFIS_CONTINUE;
	struct recipient *rec = g_ptr_array_index (priv->recipients, j-1);
	for (k = 0; k < num_so_modules; k++)
	  if (rec->modules[k]->smfi_const != SMFIS_CONTINUE
	      && prdr_get_activity(priv, so_modules[k]->name) !=2 ) {
	    l = rec->modules[k]->smfi_const;
	    break;
	}
	if (l != SMFIS_CONTINUE && rec->modules[k]->return_code != NULL) {
	  //smfi_delrcpt(priv->ctx, rec->address);
	  rcodes[j] = rec->modules[k]->return_code;
	  xcodes[j] = rec->modules[k]->return_dsn;
	  ret_string[j] = rec->modules[k]->return_reason;
	} else {
	  //if (priv->recipients[j-1]->modules[k]->return_reason) g_free(priv->recipients[j-1]->modules[k]->return_reason);
	  //priv->recipients[j-1]->modules[k]->return_reason = ret_string[j];
	  char * temp = g_strconcat (rec->address, " accepts the message",
				     NULL);
	  ret_string[j] = g_string_chunk_insert (priv->gstr, temp);
	  g_free (temp);
	  rcodes[j] = "250";
	  xcodes[j] = "2.1.5";
	}
      }
      i = SMFIS_TEMPFAIL;
      //g_printf("call smfi_setreplies\n");
      smfi_setreplies (priv->ctx, rcodes, xcodes, ret_string);
      //g_printf("smfi_setreplies executed\n");
      g_free (ret_string);
      g_free (rcodes);
      g_free (xcodes);
      */
    } else {
      //g_printf("***set_responses B1 prdr is NOT supported***\n");
      switch (bounce_mode) {
	//case '0': delayed -- cannot happen -- in this mode all recipients agree on the final result
        case '1': //pseudo_delayed
	  //g_printf("***set_responses C bounce_mode =1***\n");
	//check what the first recipient thinks
	  j = 0;
	  struct recipient *rec = g_ptr_array_index (priv->recipients, 0);
	  while (j < num_so_modules) {
	    if (rec->modules[j]->smfi_const != SMFIS_CONTINUE) {
	      inject_response (priv->ctx, rec->modules[j]->return_code,
			       rec->modules[j]->return_dsn,
			       rec->modules[j]->return_reason);
	      break;
	    }
	    j++;
	  }
	  if (j == num_so_modules + 1)
	    inject_response (priv->ctx, "250", "2.1.5", "Message accepted");
	  break;
        case '2': //NDR
	  //g_printf("***set_responses D bounce_mode =2***\n");
        case '3': ;//no-NDR
	  //g_printf("***set_responses E bounce_mode =3***\n");
	  i = SMFIS_CONTINUE;
        }
    }
    break;
  default: ;
    //g_printf("***set_responses F bounce_mode***\n");
    //i > 0, every recipient thinks this message must be rejected
    //g_printf("set_responses: every recipient thinks this message must be rejected, j=%i, k=%i\n", j, k);
    struct recipient *rec = g_ptr_array_index (priv->recipients, j-1);
    inject_response (priv->ctx, rec->modules[k]->return_code,
		     rec->modules[k]->return_dsn,
		     rec->modules[k]->return_reason);
  };
  //g_printf("---set_responses---\n");
  return i;
}

//-----------------------------------------------------------------------------
//returns -1 failed, 0 - not failed & ACCEPTED, >0 - not failed & REJECTED
static inline int
apply_modules (struct privdata* priv)
{
  unsigned int i;
  //g_printf ("***apply_modules %p***\n", priv);
  if (priv->current_recipient == NULL) return 0;
  //  char remaining_recipients = 1;
  for (i = 0; i < num_so_modules; i++) {
    //g_printf ("***apply_modules %p A %i/%i***\n", priv, i, num_so_modules);
    priv->current_recipient->current_module =
      priv->current_recipient->modules[i];

    if (so_modules[i]->status (priv) & priv->stage  //module is subject to execution at this stage 
	//        remaining_recipients && //there are still recipients left
	&& (2 != prdr_get_activity (priv, so_modules[i]->name)) //and module is active (=not yet disabled)
	&& (priv->current_recipient->modules[i]->flags & priv->stage) == 0) {
      priv->current_recipient->current_module->flags |= priv->stage;
      priv->current_recipient->current_module->flags &= !MOD_FAILED;

      if (priv->stage == MOD_BODY)
	smfi_progress (priv->ctx);//call this regularly
      if (so_modules[i]->run (priv)
	  || (priv->current_recipient->modules[i]->flags & MOD_FAILED)) { //module was executed with error
	priv->current_recipient->current_module->return_reason = NULL;
	priv->current_recipient->current_module->return_code = NULL;
	priv->current_recipient->current_module->return_dsn = NULL;
	priv->current_recipient->current_module->smfi_const = SMFIS_CONTINUE;
	//modified recipients
	if (priv->current_recipient->current_module->msg->envrcpts) {
	  int k = 1;
	  while (priv->current_recipient->current_module->msg->envrcpts[k])
	    g_free (priv->current_recipient->current_module->msg->envrcpts[k++]);
	  g_free (priv->current_recipient->current_module->msg->envrcpts);
	  priv->current_recipient->current_module->msg->envrcpts = NULL;
	}
	//modified headers from
	//priv->current_recipient->current_module->msg->headers
	prdr_do_fail (priv);
	return -1;
      };
      if (priv->current_recipient->modules[i]->smfi_const != SMFIS_CONTINUE) {
      //g_printf ("---apply_modules %p REJECTED %i---\n", priv, i);
	return i+1;
      }
    }
  }
//  g_printf("---apply_modules %p ACCEPTED---\n", priv);
  return 0;
}
//-----------------------------------------------------------------------------

static inline char*
normalize_email (struct privdata* priv, const char *email)
{
  //remove spaces and <
  if (strcmp("<>", email) == 0) return "<>";
  while (*email == ' ' || *email == '<')
    email++;
  //lowercase the characters
  char *c = prdr_add_string (priv, email), *k = c;
  while (*c != '\0' && *c != ' ' && *c != '>' ) {
    *c = g_ascii_tolower (*c);
    c++;
  }
  *c= '\0';
  return k;
}
//-----------------------------------------------------------------------------

static inline const struct so_list*
prdr_list_is_available (const char *listname)
{
  unsigned int i = 0;
  if (listname)
    while (i < num_tables)
      if (lists[i]->name && g_ascii_strcasecmp (lists[i++]->name, listname) == 0)
	return lists[i-1]->module;
  return NULL;
}

//-----------------------------------------------------------------------------

int
prdr_list_insert (const char *table,
		  const char *user,
		  const char *key,
		  const char *value,
		  const unsigned int expire)
{
  const struct so_list *ml = prdr_list_is_available (table);
  if (ml && ml->insert)
    return  ml->insert (table, user, key, value, expire);
  else
    return -1;
}
//-----------------------------------------------------------------------------

char*
prdr_list_query (const char *table, const char *user, const char *key)
{
  //g_printf("***prdr_list_query***, table=%s, user=%s, key=%s\n", table, user, key);
  const struct so_list *ml = prdr_list_is_available (table);
  //g_printf("---prdr_list_query---%p\n", ml);
  return ml->query (table, user, key);
}
//-----------------------------------------------------------------------------

void
prdr_list_expire ()
{
  unsigned int i;
  for (i = 0; i < num_so_lists; i++)
    if(so_lists[i]->expire)
      so_lists[i]->expire ();
}
//-----------------------------------------------------------------------------

int
prdr_list_remove (const char *table, const char *user, const char *key)
{
  const struct so_list *ml = prdr_list_is_available (table);
  if (ml &&  ml->remove)
    return ml->remove (table, user, key);
  else
    return -1;
}
