extern "C" {
  #include "config.h"
}
#include <stdlib.h>
#include <string.h>
#include "src/modules/mod_sieve/message.hpp"

extern "C" HIDDEN int
libsieve_keep (sieve2_context_t *, void *my) {
  Privdata *cont = (Privdata*) my;
  sieve_local* dat = (sieve_local*)cont->GetPrivRcpt ();
  if (cont->GetStage () != MOD_BODY) {
    dat->desired_stages |= MOD_BODY;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE2_ERROR_FAIL;
  }
  dat->actions.emplace_back (mod_action_t::ACTION_KEEP);
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_reject (sieve2_context_t *s, void *my) {
  sieve_local* dat = (sieve_local*) ((Privdata*)my)->GetPrivRcpt ();
  dat->actions.emplace_back (mod_action_t::ACTION_REJECT, sieve2_getvalue_string (s, "message"));
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_discard (sieve2_context_t *, void *my) {
  sieve_local* dat = (sieve_local*) ((Privdata*)my)->GetPrivRcpt ();
  dat->actions.emplace_back (mod_action_t::ACTION_DISCARD);
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_redirect (sieve2_context_t *s, void *my) {
  Privdata *cont = (Privdata*) my;
  sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
  if (cont->GetStage () != MOD_BODY) {
    dat->desired_stages |= MOD_BODY;
    dat->failed = true;
    cont->DoFail ();
    return SIEVE2_ERROR_FAIL;
  }
  dat->actions.emplace_back (mod_action_t::ACTION_REDIRECT, sieve2_getvalue_string (s, "address"));
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_fileinto (sieve2_context_t *s, void *my) {
  Privdata *cont = (Privdata*) my;
  sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
  dat->actions.emplace_back (mod_action_t::ACTION_FILEINTO, sieve2_getvalue_string (s, "mailbox"));
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_vacation (sieve2_context_t *s, void *my)
{
  Privdata *cont = (Privdata*) my;
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
  a.u.vac.send.addr = strdup (sieve2_getvalue_string (s, "address"));
  a.u.vac.send.fromaddr = strdup (sieve2_getvalue_string (s, "fromaddr"));
  /* user specified subject */
  a.u.vac.send.msg = strdup (sieve2_getvalue_string (s, "message")); //text
  a.u.vac.send.mime = sieve2_getvalue_int (s, "mime"); //1 or 0
  a.u.vac.send.subj = strdup (sieve2_getvalue_string (s, "subject"));
  a.u.vac.autoresp.hash = strdup((cont->GetEnvSender () + ":"
			+ sieve2_getvalue_string (s, "hash")).c_str ());
  a.u.vac.autoresp.days = sieve2_getvalue_int (s, "days");
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_getbody (sieve2_context_t *s, void *my)
{
  Privdata *cont = (Privdata*) my;
  if (cont->GetStage () != MOD_BODY) {
    sieve_local *dat = (sieve_local*)cont->GetPrivRcpt ();
    dat->desired_stages |= MOD_BODY;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE2_ERROR_FAIL;
  }
  sieve2_setvalue_string (s, "body", cont->GetBody ().c_str ());
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_getsize (sieve2_context_t *s, void *my)
{
  Privdata *cont = (Privdata*) my;
  if (const int i = cont->GetSize ()) {
    sieve2_setvalue_int (s, "size", i);
    return SIEVE2_OK;
  } else {
    sieve2_setvalue_int (s, "size", 0);
    sieve_local* dat = (sieve_local*)cont->GetPrivRcpt ();
    dat->desired_stages |= MOD_BODY;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE2_ERROR_FAIL;
  }
}

extern "C" HIDDEN int
libsieve_getscript (sieve2_context_t *s, void *my) {
  sieve2_setvalue_string (s, "script",
    sieve_getscript (
      sieve2_getvalue_string (s, "path"),//path = ":personal", ":global", ""
      sieve2_getvalue_string (s, "name"),//name of the script for the current user
      *(Privdata*)my).c_str ());
  return SIEVE2_OK;
}


extern "C" HIDDEN int
libsieve_getheader (sieve2_context_t *s, void *my)
{
  Privdata *cont = (Privdata*) my;
  sieve_local *dat = (sieve_local*) cont->GetPrivRcpt ();
  if (cont->GetStage () < MOD_HEADERS) {
    dat->desired_stages |= MOD_HEADERS;
    cont->DoFail ();
    dat->failed = true;
    return SIEVE2_ERROR_FAIL;
  }
  const std::vector<std::string>& ret_headers
    = cont->GetHeader (sieve2_getvalue_string (s, "header"));
  if (ret_headers.empty ()) {
    sieve2_setvalue_stringlist (s, "body", NULL);
    return SIEVE2_OK;
  }
  dat->delete_headers ();
  dat->headers = static_cast<const char**>(malloc (sizeof (char *) * (ret_headers.size () + 1)));
  int i = 0;
  for (const std::string h : ret_headers) {
    dat->headers[i] = strdup (h.c_str ());
    i++;
  }
  dat->headers[i] = NULL;
  sieve2_setvalue_stringlist (s, "body", dat->headers);
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_getenvelope (sieve2_context_t *s, void *my)
{
  Privdata *cont = (Privdata*) my;
  sieve2_setvalue_string (s, "to", cont->GetRecipient ().c_str ());
  sieve2_setvalue_string (s, "from", cont->GetEnvSender ().c_str ());
  return SIEVE2_OK;
}

extern "C" HIDDEN int
libsieve_getsubaddress (sieve2_context_t *s, void *my)
{
  Privdata *cont = (Privdata*) my;
  sieve_local* dat = (sieve_local*) cont->GetPrivRcpt ();
  dat->address = sieve2_getvalue_string (s, "address");
  size_t index = dat->address.find ('@');
  if (index == std::string::npos)
    sieve2_setvalue_string (s, "domain", "");
  else {
    dat->address[index] = '\0';
    sieve2_setvalue_string (s, "domain", dat->address.c_str() + index + 1);//text after @
  }
  sieve2_setvalue_string (s, "localpart", dat->address.c_str());

  dat->user = dat->address;
  index = dat->user.find ('+');
  if (index == std::string::npos)
    sieve2_setvalue_string (s, "detail", "");
  else {
    dat->user[index] = '\0';
    sieve2_setvalue_string (s, "detail", dat->user.c_str() + index + 1);
  }
  sieve2_setvalue_string (s, "user", dat->user.c_str());
  return SIEVE2_OK;
}

static sieve2_callback_t sieve_callbacks[] = {
  { SIEVE2_ACTION_KEEP,           libsieve_keep },
  { SIEVE2_ACTION_REJECT,         libsieve_reject },
  { SIEVE2_ACTION_DISCARD,        libsieve_discard },
  { SIEVE2_ACTION_REDIRECT,       libsieve_redirect },
  { SIEVE2_ACTION_FILEINTO,       libsieve_fileinto },
  { SIEVE2_ACTION_VACATION,       libsieve_vacation },
  { SIEVE2_MESSAGE_GETBODY,       libsieve_getbody },
  { SIEVE2_MESSAGE_GETSIZE,       libsieve_getsize },
  { SIEVE2_SCRIPT_GETSCRIPT,      libsieve_getscript },
  { SIEVE2_MESSAGE_GETHEADER,     libsieve_getheader },
  { SIEVE2_MESSAGE_GETENVELOPE,   libsieve_getenvelope },
  { SIEVE2_MESSAGE_GETSUBADDRESS, libsieve_getsubaddress },
  { SIEVE2_VALUE_LAST, 0} };

HIDDEN bool
libsieve_run(Privdata& priv) {
  sieve_local* dat = (sieve_local*) priv.GetPrivRcpt ();
  if (sieve2_alloc (&dat->sieve2_context) == SIEVE2_OK) {
    sieve2_callbacks (dat->sieve2_context, sieve_callbacks);
    bool ret = sieve2_execute (dat->sieve2_context, &priv) == SIEVE2_OK;
    sieve2_free (&dat->sieve2_context);
    return ret;
  } else return false;
}
