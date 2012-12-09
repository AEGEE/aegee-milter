#include "src/modules/mod_sieve/message.h"
#include <sieve/sieve_interface.h>
#include "config.h"
#include "prdr-mod.h"

const char const * mod_sieve_script_format;

EXPORTED
void fatal(const char *fatal_message, int fatal_code) {
  printf("fatal message: %s, code: %i\n", fatal_message, fatal_code);
}

static int
cyrus_autorespond(void *action_context UNUSED, void *interp_context UNUSED,
		void *script_context UNUSED, void *message_context UNUSED,
		const char **errmsg UNUSED) {
  return SIEVE_OK;
}

static int
cyrus_send_responce(void *action_context UNUSED, void *interp_context UNUSED,
		void *script_context UNUSED, void *message_context UNUSED,
		const char **errmsg UNUSED) {
  return SIEVE_OK;
}

static int
cyrus_get_size (void *message_context, int *size) {
  struct privdata *cont = (struct privdata*) message_context;
  *size = prdr_get_size (cont);
  if (*size)
    return SIEVE_OK;
  else {
    struct sieve_local* dat = (struct sieve_local*) prdr_get_priv_rcpt (cont);
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return -1;
  }
}

static int
cyrus_redirect (void *action_context, void *interp_context UNUSED,
		void *script_context UNUSED, void *message_context,
		const char **errmsg UNUSED) {
  struct privdata *cont = (struct privdata*) message_context;
  struct sieve_local* dat = (struct sieve_local*) prdr_get_priv_rcpt (cont);
  if (prdr_get_stage (cont) != MOD_BODY) {
    prdr_do_fail (cont);
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return SIEVE2_ERROR_FAIL;
  }
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE_NOMEM;
  dat->last_action = a;
  a->a = ACTION_REDIRECT;
  a->cancel_keep = 1;
  a->u.red.addr = g_strdup (((mod_sieve_redirect_context_t*)action_context)->addr);
  a->next = NULL;  
  return SIEVE_OK;
}

static int
cyrus_reject (void *action_context, void *interp_context UNUSED,
	      void *script_context UNUSED, void *message_context,
	      const char **errmsg UNUSED) {
  struct privdata *cont = (struct privdata*) message_context;
  struct sieve_local* dat = (struct sieve_local*) prdr_get_priv_rcpt (cont);
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE_NOMEM;
  dat->last_action = a;
  a->a = ACTION_REJECT;
  a->cancel_keep = 1;
  a->u.rej.msg = g_strdup (((mod_sieve_reject_context_t*)action_context)->msg);
  a->next = NULL;
  return SIEVE_OK;
}

static int
cyrus_keep (void *action_context UNUSED, void *interp_context UNUSED,
	    void *script_context UNUSED, void *message_context,
	    const char **errmsg UNUSED) {
  struct privdata *cont = (struct privdata*) message_context;
  struct sieve_local *dat = (struct sieve_local*) prdr_get_priv_rcpt (cont);
  if (prdr_get_stage (cont) != MOD_BODY) {
    dat->desired_stages |= MOD_BODY;
    prdr_do_fail (cont);
    return -1;
  }
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE_NOMEM;
  dat->last_action = a;
  a->a = ACTION_KEEP;
  a->cancel_keep = 1;
  a->next = NULL;
  return SIEVE_OK;
}

static int
cyrus_discard (void *action_context UNUSED, void *interp_context UNUSED,
          void *script_context UNUSED, void *message_context, const char **errmsg UNUSED) {
  struct privdata *cont = (struct privdata*) message_context;
  struct sieve_local* dat = (struct sieve_local*) prdr_get_priv_rcpt (cont);
  dat->last_action->next = (mod_action_list_t *) g_malloc(sizeof(mod_action_list_t));
  mod_action_list_t *a = dat->last_action->next;
  if (a == NULL)
    return SIEVE_NOMEM;
  dat->last_action = a;
  a->a = ACTION_DISCARD;
  a->cancel_keep = 1;
  a->next = NULL;
  return SIEVE_OK;
}

static int
cyrus_get_envelope (void* message_context, const char* field,  const char ***contents) {
  struct privdata *cont = (struct privdata*) message_context;
  struct sieve_local *dat = (struct sieve_local*) prdr_get_priv_rcpt (cont);
  if (dat->headers)
    g_free (dat->headers);
  dat->headers = g_malloc (sizeof (char*) * 2);
  switch (field[0]) {
  case 't': // "to"
  case 'T': dat->headers[0] = prdr_get_recipient (cont);
    break;
  case 'f': //"from"
  case 'F': dat->headers[0] = prdr_get_envsender (cont);
    break;
  default:
    dat->headers[0] = NULL;
  }
  dat->headers[1] = NULL;
  *contents = dat->headers;
  return SIEVE_OK;
}

static int
cyrus_get_header(void *message_context, const char *header,
		 const char ***contents) {
  struct privdata *cont = (struct privdata*) message_context;
  struct sieve_local *dat = (struct sieve_local*)prdr_get_priv_rcpt (cont);
  if (prdr_get_stage (cont) < MOD_HEADERS) {
    dat->desired_stages |= MOD_HEADERS;
    prdr_do_fail (cont);
    return -1;
  }
  const char** ret_headers = prdr_get_header (cont, header);
  if (ret_headers == NULL || ret_headers[0] == NULL) {
    if (ret_headers) {
      *contents = ret_headers;
    } else {
      *contents = malloc(sizeof(char*));
      *contents[0] = NULL;
    }
    dat->headers = *contents;
    return SIEVE_OK;
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
  *contents = dat->headers;
  return SIEVE_OK;
}

static int
cyrus_get_include(void *script_context, const char *script,
		  int isglobal, char *fpath, size_t size) {
  struct privdata *cont = (struct privdata*) script_context;
  char *bytecode_path = prdr_list_query (mod_sieve_script_format,
      (isglobal == 1 ? ":global" : cont->current_recipient->address), script);
  g_snprintf (fpath, size, "%s", bytecode_path);
  free (bytecode_path);
  return SIEVE_OK;
}

static int
cyrus_execute_error(const char* msg, void *interp_context UNUSED, void *script_context, void *message_context UNUSED) {
  struct privdata *cont = (struct privdata*) script_context;
  char *text = g_malloc (strlen(msg)+17);
  sprintf(text, "mode: %u, text: %s", cont->stage, msg);
  prdr_list_insert ("log", prdr_get_recipient (cont), "mod_sieve:cyrus", text, 0);
  g_free(text);
  return SIEVE_OK;
}


HIDDEN int
libcyrus_sieve_load() {
  mod_sieve_script_format = "sieve_bytecode_path";
  return 0;
}

HIDDEN int
libcyrus_sieve_run(void *priv)
{
  sieve_interp_t *i;
  static sieve_vacation_t cyrus_vacation = {0, 90*24*60*60,
			cyrus_autorespond, cyrus_send_responce};
  sieve_interp_alloc (&i, NULL);
  sieve_register_redirect (i, cyrus_redirect);
  sieve_register_discard (i, cyrus_discard);
  sieve_register_reject (i, cyrus_reject);
  //sieve_register_fileinto (i, sieve_callback *f);
  sieve_register_keep (i, cyrus_keep);
  sieve_register_vacation (i, &cyrus_vacation);
  sieve_register_include (i, cyrus_get_include);
  sieve_register_size (i, cyrus_get_size);
  sieve_register_header (i, cyrus_get_header);
  sieve_register_envelope (i, cyrus_get_envelope);
  //sieve_register_body (i, sieve_get_body *f);
  sieve_register_execute_error (i, cyrus_execute_error);

  sieve_execute_t *sc = NULL;
  sieve_script_load (sieve_getscript (":personal", NULL, priv), &sc);
  sieve_execute_bytecode (sc, i, priv, priv);
  sieve_script_unload (&sc);
  sieve_interp_free(&i);
  return 0;
}
