#include "src/modules/mod_sieve/message.h"
#include "prdr-mod.h"

const char const * mod_sieve_script_format;

static int
libsieve_keep (UNUSED sieve2_context_t *s, void *my) {
  struct privdata *cont = (struct privdata*) my;
  struct sieve_local* dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  if (prdr_get_stage (cont) != MOD_BODY) {
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return SIEVE2_ERROR_FAIL;
  }
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE2_ERROR_FAIL;
  dat->last_action = a;
  a->a = ACTION_KEEP;
  a->cancel_keep = 1;
  a->next = NULL;
  return SIEVE2_OK;
}

static int
libsieve_reject (sieve2_context_t *s, void *my) {
  struct privdata *cont = (struct privdata*) my;
  struct sieve_local* dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE2_ERROR_FAIL;
  dat->last_action = a;
  a->a = ACTION_REJECT;
  a->cancel_keep = 1;
  a->u.rej.msg = g_strdup (sieve2_getvalue_string (s, "message"));
  a->next = NULL;
  return SIEVE2_OK;
}

static int
libsieve_discard (UNUSED sieve2_context_t *s, void *my) {
  struct privdata *cont = (struct privdata*) my;
  struct sieve_local* dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE2_ERROR_FAIL;
  dat->last_action = a;
  a->a = ACTION_DISCARD;
  a->cancel_keep = 1;
  a->next = NULL;
  return SIEVE2_OK;
}

static int
libsieve_redirect (sieve2_context_t *s, void *my) {
  struct privdata *cont = (struct privdata*) my;
  struct sieve_local* dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  if (prdr_get_stage (cont) != MOD_BODY) {
    prdr_do_fail (cont);
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return SIEVE2_ERROR_FAIL;
  }
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE2_ERROR_FAIL;
  dat->last_action = a;
  a->a = ACTION_REDIRECT;
  a->cancel_keep = 1;
  a->u.red.addr = g_strdup (sieve2_getvalue_string (s, "address"));
  a->next = NULL;
  return SIEVE2_OK;
}

/*
static int
libsieve_fileinto(UNUSED sieve2_context_t *s, void *my) {
  struct privdata *cont = (struct privdata*) my;
  struct sieve_local* dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE2_ERROR_FAIL;
  dat->last_action = a;
  a->a = ACTION_FILEINTO;
  a->cancel_keep = 1;
  //  a->next->u.fil.mailbox = mbox;
  a->next = NULL;
  return SIEVE2_OK;
}
*/

static int
libsieve_vacation (sieve2_context_t *s, void *my)
{
  struct privdata *cont = (struct privdata*) my;
  if (prdr_get_stage (cont) != MOD_BODY) {
    prdr_set_activity (cont, "spam", 1);
    struct sieve_local *dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return SIEVE2_ERROR_FAIL;
  }
  struct sieve_local* dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE2_ERROR_FAIL;
  dat->last_action = a;
  a->a = ACTION_VACATION;
  a->cancel_keep = 0;
  a->u.vac.send.addr = g_strdup(sieve2_getvalue_string (s, "address"));
  a->u.vac.send.fromaddr = g_strdup(sieve2_getvalue_string (s, "fromaddr"));
  /* user specified subject */
  a->u.vac.send.msg = g_strdup(sieve2_getvalue_string (s, "message")); //text
  a->u.vac.send.mime = sieve2_getvalue_int (s, "mime"); //1 or 0
  a->u.vac.send.subj = g_strdup(sieve2_getvalue_string (s, "subject"));
  a->u.vac.autoresp.hash = g_strconcat (prdr_get_envsender (cont), ":",
				sieve2_getvalue_string(s, "hash"), NULL);
  a->u.vac.autoresp.days = sieve2_getvalue_int(s, "days");
  a->next = NULL;
  return SIEVE2_OK;
}

static int
libsieve_getbody (sieve2_context_t *s, void *my)
{
  struct privdata *cont = (struct privdata*) my;
  if (prdr_get_stage(cont) != MOD_BODY) {
    struct sieve_local *dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return SIEVE2_ERROR_FAIL;
  }
  const GString *body = prdr_get_body (cont);
  sieve2_setvalue_string (s, "body", body->str);
  return SIEVE2_OK;
}

static int
libsieve_getsize (sieve2_context_t *s, void *my)
{
  struct privdata *cont = (struct privdata*) my;
  int i = prdr_get_size (cont);
  sieve2_setvalue_int (s, "size", i);
  if (i)
    return SIEVE2_OK;
  else {
    struct sieve_local* dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return SIEVE2_ERROR_FAIL;
  }
}

static int
libsieve_getscript (sieve2_context_t *s, void *my) {
  sieve2_setvalue_string (s, "script",
    sieve_getscript (
      sieve2_getvalue_string (s, "path"),//path = ":personal", ":global", ""
      sieve2_getvalue_string (s, "name"),//name of the script for the current user
      my));
  return SIEVE2_OK;
}


static int
libsieve_getheader (sieve2_context_t *s, void *my)
{
  struct privdata *cont = (struct privdata*) my;
  struct sieve_local *dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  if (prdr_get_stage (cont) < MOD_HEADERS) {
    dat->desired_stages |= MOD_HEADERS;
    prdr_do_fail (cont);
    return SIEVE2_ERROR_FAIL;
  }
  char const * const header = sieve2_getvalue_string (s, "header");
  const char** ret_headers = prdr_get_header (cont, header);
  if (ret_headers == NULL || ret_headers[0] == NULL) {
    sieve2_setvalue_stringlist (s, "body", NULL);
    if (ret_headers) g_free (ret_headers);
    return SIEVE2_OK;
  }
  if (dat->headers)
    g_free (dat->headers);
  int i, j;
  for (j = 0; ret_headers[j]; j++);
  dat->headers = g_malloc (sizeof (char *) * (j+1));
  for (i = 0; i < j; i++)
    dat->headers[i] = ret_headers[i];
  dat->headers[j] = NULL;
  g_free (ret_headers);
  sieve2_setvalue_stringlist (s, "body", dat->headers);
  return SIEVE2_OK;
}

static int
libsieve_getenvelope (sieve2_context_t *s, void *my)
{
  struct privdata *cont = (struct privdata*) my;
  sieve2_setvalue_string (s, "to", prdr_get_recipient(cont));
  sieve2_setvalue_string (s, "from", prdr_get_envsender(cont));
  return SIEVE2_OK;
}

static int
libsieve_getsubaddress (sieve2_context_t *s, void *my)
{
  struct privdata *cont = (struct privdata*) my;
  char *subaddr1 = prdr_add_string (my, sieve2_getvalue_string (s, "address"));
  char *temp = strchr (subaddr1, '@');
  if (temp == NULL)
    sieve2_setvalue_string (s, "domain", "");
  else {
    *temp = '\0';
    sieve2_setvalue_string (s, "domain", temp+1);//text after @
  }
  sieve2_setvalue_string (s, "localpart", subaddr1);

  char* subaddr2 = prdr_add_string (cont, subaddr1);
  temp = strchr (temp, '+');
  if (temp == NULL)
    sieve2_setvalue_string (s, "detail", "");
  else {
    *temp = '\0';
    sieve2_setvalue_string (s, "detail", temp+1);
  }
  sieve2_setvalue_string (s, "user", subaddr2);
  return SIEVE2_OK;
}

static sieve2_callback_t sieve_callbacks[] = {
  { SIEVE2_ACTION_KEEP,           libsieve_keep },
  { SIEVE2_ACTION_REJECT,         libsieve_reject },
  { SIEVE2_ACTION_DISCARD,        libsieve_discard },
  { SIEVE2_ACTION_REDIRECT,       libsieve_redirect },
  //  { SIEVE2_ACTION_FILEINTO,       libsieve_fileinto },
  { SIEVE2_ACTION_VACATION,       libsieve_vacation },
  { SIEVE2_MESSAGE_GETBODY,       libsieve_getbody },
  { SIEVE2_MESSAGE_GETSIZE,       libsieve_getsize },
  { SIEVE2_SCRIPT_GETSCRIPT,      libsieve_getscript },
  { SIEVE2_MESSAGE_GETHEADER,     libsieve_getheader },
  { SIEVE2_MESSAGE_GETENVELOPE,   libsieve_getenvelope },
  { SIEVE2_MESSAGE_GETSUBADDRESS, libsieve_getsubaddress },
  {0, 0} };

int
libsieve_load() {
  mod_sieve_script_format = "sieve_scripts";
  return 0;
}

int
libsieve_unload() {
  return 0;
}

int
libsieve_run(void *priv) {
  struct sieve_local* dat = (struct sieve_local*) prdr_get_priv_rcpt (priv);
  if (sieve2_alloc (&dat->sieve2_context) == SIEVE2_OK) {
    sieve2_callbacks (dat->sieve2_context, sieve_callbacks);
    int ret = sieve2_execute (dat->sieve2_context, priv);
    sieve2_free (&dat->sieve2_context);
    return ret == SIEVE2_OK ? 0 : -1;
  } else return -1;
}
