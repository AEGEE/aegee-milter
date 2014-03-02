extern "C" {
  #include <sieve/sieve_interface.h>
  #include "config.h"
}
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "src/core/AegeeMilter.hpp"
#include "src/modules/mod_sieve/message.hpp"

extern "C" EXPORTED void
fatal(const char *fatal_message, int fatal_code) {
  std::cerr << "fatal message: " << fatal_message << ", code: " << fatal_code << std::endl;
}

extern "C" HIDDEN int
cyrus_autorespond(void *action_context, void *, void *message_context, void *, const char **) {
  Privdata *cont = (Privdata*) message_context;
  sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
  if (cont->GetStage () != MOD_BODY) {
    //cont->SetActivity ("spam", 1);
    dat->desired_stages |= MOD_BODY;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE2_ERROR_FAIL;
  }

  dat->actions.emplace_back (mod_action_t::ACTION_VACATION);

  mod_sieve_Action& a = dat->actions.back ();
  a.u.vac.send.addr = strdup ("");
  a.u.vac.send.fromaddr = strdup ("");
  /* user specified subject */
  a.u.vac.send.msg = strdup ("");
  a.u.vac.send.mime = 0;
  a.u.vac.send.subj = strdup ("");

  mod_sieve_autorespond_context_t *z = (mod_sieve_autorespond_context_t*)action_context;;
  a.u.vac.autoresp.hash = strdup(z->hash);
  a.u.vac.autoresp.days = z->days;

  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_send_responce(void *, void *, void *, void *, const char **) {
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_get_size (void *message_context, int *size) {
  Privdata *cont = (Privdata*) message_context;
  if ((*size = cont->GetSize ()))
    return SIEVE_OK;
  else {
    sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
    dat->desired_stages |= MOD_BODY;
    dat->failed = true;
    cont->DoFail ();
    return SIEVE_FAIL;
  }
}

extern "C" HIDDEN int
cyrus_redirect (void *action_context, void *, void *, void *message_context,
		const char **) {
  Privdata *cont = (Privdata*) message_context;
  sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
  if (cont->GetStage () != MOD_BODY) {
    cont->DoFail ();
    dat->failed = true;
    dat->desired_stages |= MOD_BODY;
    return SIEVE_FAIL;
  }
  dat->actions.emplace_back (mod_action_t::ACTION_REDIRECT, ((mod_sieve_redirect_context_t*)action_context)->addr);
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_reject (void *action_context, void *, void *, void *message_context,
	      const char **) {
  Privdata *cont = (Privdata*) message_context;
  sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
  dat->actions.emplace_back (mod_action_t::ACTION_REJECT, ((mod_sieve_reject_context_t*)action_context)->msg);
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_keep (void *, void *, void *, void *message_context, const char **) {
  Privdata *cont = (Privdata*) message_context;
  sieve_local *dat = (sieve_local*) cont->GetPrivRcpt ();
  if (cont->GetStage () != MOD_BODY) {
    dat->desired_stages |= MOD_BODY;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE_FAIL;
  }
  dat->actions.emplace_back (mod_action_t::ACTION_KEEP);
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_fileinto (void* action_context, void*, void*, void* message_context, const char**) {

  Privdata *cont = (Privdata*) message_context;
  sieve_local *dat = (sieve_local*) cont->GetPrivRcpt ();
  if (cont->GetStage () != MOD_BODY) {
    dat->desired_stages |= MOD_BODY;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE_FAIL;
  }
  dat->actions.emplace_back (mod_action_t::ACTION_FILEINTO,  ((mod_sieve_fileinto_context_t*)action_context)->mailbox);
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_discard (void *, void *, void *, void *message_context, const char **) {
  Privdata *cont = (Privdata*) message_context;
  sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
  dat->actions.emplace_back (mod_action_t::ACTION_DISCARD);
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_get_envelope (void* message_context, const char* field,
		    const char ***contents) {
  Privdata *cont = (Privdata*) message_context;
  sieve_local *dat = (sieve_local*) cont->GetPrivRcpt ();
  dat->delete_headers ();
  dat->headers = static_cast<const char**>(malloc (sizeof (char*) * 2));
  switch (field[0]) {
  case 't': // "to"
  case 'T': dat->headers[0] = strdup (cont->GetRecipient ().c_str ());
    break;
  case 'f': //"from"
  case 'F': dat->headers[0] = strdup (cont->GetEnvSender ().c_str ());
    break;
  default:
    dat->headers[0] = NULL;
  }
  dat->headers[1] = NULL;
  *contents = dat->headers;
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_get_header(void *message_context, const char *header,
		 const char ***contents) {
  Privdata *cont = (Privdata*) message_context;
  sieve_local *dat = (sieve_local*)cont->GetPrivRcpt ();
  if (cont->GetStage () < MOD_HEADERS) {
    dat->desired_stages |= MOD_HEADERS;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE_FAIL;
  }
  dat->delete_headers ();
  const std::vector<std::string>& h = cont->GetHeader (header);
  dat->headers = static_cast<const char**>(malloc ((h.size () + 1) * sizeof(char*)));
  int i = 0;
  for (const std::string& field : h) {
    dat->headers[i] = strdup (field.c_str ());
    i++;
  }
  dat->headers[h.size ()] = NULL;
  *contents = dat->headers;
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_get_include(void *script_context, const char *script,
		  int isglobal, char *fpath, size_t size) {
  Privdata *cont = (Privdata*) script_context;
  const std::string& bytecode_path = sieve_getscript (
	      isglobal ? ":global" : cont->GetRecipient (), script, *cont);
  snprintf (fpath, size, "%s", bytecode_path.c_str ());
  return SIEVE_OK;
}

extern "C" HIDDEN int
cyrus_execute_error(const char* msg, void *, void *script_context, void *) {
  Privdata *cont = (Privdata*) script_context;
  char st[5];
  snprintf(st, 5, "%d", cont->GetStage ());
  AegeeMilter::ListInsert ("log", cont->GetRecipient (), "mod_sieve:cyrus execute_error", std::string {"mode: "} + st + ", text: " + msg);
  return SIEVE_OK;
}

HIDDEN bool
libcyrus_sieve_run(Privdata& priv)
{
  static sieve_vacation_t cyrus_vacation = {0, 90*24*60*60,
			cyrus_autorespond, cyrus_send_responce};
  sieve_interp_t *i = sieve_interp_alloc (NULL);
  sieve_register_redirect (i, cyrus_redirect);
  sieve_register_discard (i, cyrus_discard);
  sieve_register_reject (i, cyrus_reject);
  sieve_register_fileinto (i, cyrus_fileinto);
  sieve_register_keep (i, cyrus_keep);
  sieve_register_vacation (i, &cyrus_vacation);
  sieve_register_include (i, cyrus_get_include);
  sieve_register_size (i, cyrus_get_size);
  sieve_register_header (i, cyrus_get_header);
  sieve_register_envelope (i, cyrus_get_envelope);
  //sieve_register_body (i, sieve_get_body *f);
  sieve_register_execute_error (i, cyrus_execute_error);

  sieve_execute_t *sc = NULL;
  sieve_script_load (sieve_getscript (":personal", "", priv).c_str (), &sc);
  bool ret = sieve_execute_bytecode (sc, i, &priv, &priv) == SIEVE_OK;
  sieve_script_unload (&sc);
  sieve_interp_free (&i);
  return ret;
}
