#include <algorithm>
#include <cstring>
#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

extern "C" {
  #include <libmilter/mfapi.h>
}

static sfsistat
prdr_connect(SMFICTX *ctx, char* hostname, struct sockaddr *hostaddr)
{
  smfi_setpriv (ctx, new Privdata (ctx, hostname,
		     smfi_getsymval (ctx, const_cast<char*>("j")), hostaddr));
  return SMFIS_CONTINUE;
}

static sfsistat
prdr_helo (SMFICTX *ctx, char* helohost)
{
  Privdata& priv = *(Privdata*) smfi_getpriv (ctx);
  priv.ehlohost = helohost;
  if (priv.stage != MOD_EHLO) {
    priv.ClearModules ();
    priv.stage = MOD_EHLO;
  }
  priv.InitModules ();
  priv.RunModules ();
  priv.stage = MOD_MAIL;
  return SMFIS_CONTINUE;
}

static sfsistat
prdr_envfrom (SMFICTX *ctx, char **argv)
{
  if (argv[0] == NULL) return SMFIS_TEMPFAIL;
  Privdata& priv = *(Privdata*) smfi_getpriv (ctx);
  priv.queue_id = smfi_getsymval (ctx, const_cast<char*> ("i"));
  if (priv.stage != MOD_MAIL) {
    priv.recipients.clear ();
    priv.stage = MOD_MAIL;
    priv.InitModules ();
    if (priv.gMimeParser) {
      g_object_unref (priv.gMimeParser);
      priv.gMimeParser = nullptr;
    }
    if (priv.gByteArray) {
      g_byte_array_free (priv.gByteArray, 1);
      priv.gByteArray = nullptr;
    }
  }
  priv.ProceedMailFromParams (argv);
  priv.RunModules ();
  priv.stage = MOD_RCPT;
  if (!priv.gByteArray)
    priv.gByteArray = priv.size ? g_byte_array_sized_new (priv.size)
                           : g_byte_array_new ();
  return SMFIS_CONTINUE;
}

static sfsistat
prdr_envrcpt (SMFICTX *ctx, char **argv)
{
  Privdata& priv = *(Privdata*) smfi_getpriv (ctx);
  std::unique_ptr<Recipient> r {new Recipient (argv)};
  priv.current_recipient = r.get ();
  for (unsigned int k = 0; k < AegeeMilter::so_modules.size (); k++) {
    priv.current_soModule = k;
    if (priv.recipients.empty()
        || std::find_if (priv.recipients.begin (), priv.recipients.end (),
			 [&priv](const Recipient& rec) {
     if (AegeeMilter::so_modules[priv.current_soModule]->Equal (priv, rec, *priv.current_recipient))
       {
	 priv.current_recipient->modules[priv.current_soModule] = rec.modules[priv.current_soModule];
	 return true;
       } else return false;
			 }) == priv.recipients.end ()) {
      priv.module_pool.emplace_back (*AegeeMilter::so_modules[k]);
      priv.current_module = & priv.module_pool.back ();
      r->modules[k] = priv.current_module;
      AegeeMilter::so_modules[k]->InitRcpt (priv);
    }
  }

  bool exec;
  try {
    exec = priv (r.get());
  } catch (const int i) {
      //module Nr. i rejected the message, without prior fails
      priv.InjectResponse (r->modules[i]->return_code,
			   r->modules[i]->return_dsn,
			   r->modules[i]->return_reason);
      return r->modules[i]->smfi_const;
  }

  //a module failed exception -1, or nobody has rejected the recipient noexcept
  if (!priv.recipients.empty ()) {
    for (unsigned int k = 0; k < AegeeMilter::so_modules.size (); k++)
      if ((priv.recipients.front ().modules[k] != r->modules[k])
	  && ((r->flags & RCPT_NOTIFY_NEVER) == 0) ) {
	//if the modules are not equivalent to the modules of the first accepted recipient
	r->flags |= RCPT_PRDR_REJECTED;
	if (!priv.prdr_supported) return SMFIS_TEMPFAIL;
	break;
      }
  }
  priv.recipients.push_back ( std::move(*r));
  return SMFIS_CONTINUE;
};

static sfsistat
prdr_header (SMFICTX *ctx, char* headerf, char* headerv)
{
  Privdata& priv = *(Privdata*) smfi_getpriv (ctx);
  g_byte_array_append (priv.gByteArray,
		reinterpret_cast<const guint8*>(headerf), strlen (headerf));
  g_byte_array_append (priv.gByteArray,
		reinterpret_cast<const guint8*>(": "), 2);
  if (!strcmp(headerf, "From") && !strcmp(headerv, "Mail Delivery System <Mailer-Daemon@>")) {
    priv.replace_from_header = true;
    headerf = (char*)"Mail Delivery System <Mailer-Daemon@mail.AEGEE.ORG>";
  }
  g_byte_array_append (priv.gByteArray,
		reinterpret_cast<const guint8*>(headerv), strlen (headerv));
  g_byte_array_append (priv.gByteArray,
		reinterpret_cast<const guint8*>("\r\n"), 2);

  if (priv.msg.AddHeader (headerf, headerv))
    return SMFIS_CONTINUE;
  else {
    priv.InjectResponse ("550", "", "Header " + std::string (headerf) 
			 + " contains non-ascii characters");
    return SMFIS_REJECT;
  }
}

static sfsistat
prdr_eoh (SMFICTX *ctx)
{
  Privdata& priv = *(Privdata*) smfi_getpriv (ctx);
  g_byte_array_append (priv.gByteArray, reinterpret_cast<const guint8*>("\r\n"), 2);
  priv.stage = MOD_HEADERS;
  try {
    for (Recipient& rec : priv.recipients)
      priv (&rec);

    //no module failed, message accepted
    for (Recipient& rec : priv.recipients) {
      priv.current_recipient = &rec;
      priv.current_soModule = 0;
      for (Module* mod : priv.current_recipient->modules)
	if (mod->so_mod.Status (priv) & MOD_BODY)
	  throw -2; //no module failed but body needed
	else priv.current_soModule++;
    }
    //no module needs the body, all modules accept the message and 
    //there are no errors yet
    priv.msg = Message();
  } catch (const int j) {
    if (j >= 0) { //no module failed and some module rejected the message
      priv.SetResponses ();
      priv.recipients.clear (); //This shall not be here, but it seems _eom or _abort is not called, when _eoh returns REJECT
      return SMFIS_REJECT;
    } else if (j < 0)
      priv.msg.body.clear ();
  }
  return SMFIS_CONTINUE;
}

static sfsistat
prdr_body (SMFICTX *ctx, unsigned char* bodyp, const size_t len)
{
  if (len == 0) return SMFIS_CONTINUE;
  Privdata& priv = *(Privdata*) smfi_getpriv (ctx);
  g_byte_array_append (priv.gByteArray, bodyp, len);
  if (priv.msg.envfrom.empty ())
    return SMFIS_SKIP;
  priv.msg.body.append ((char*)bodyp, len);
  return SMFIS_CONTINUE;
}

static sfsistat
prdr_eom (SMFICTX *ctx)
{
  Privdata& priv = *(Privdata*) smfi_getpriv (ctx);
  priv.stage = MOD_BODY;
  if (priv.replace_from_header)
    smfi_chgheader (ctx, (char*)"From", 1, (char*)"Mail Delivery System <Mailer-Daemon@mail.AEGEE.ORG>");

  for (Recipient& rec : priv.recipients)
    priv (&rec);
  return priv.SetResponses ();
}

static sfsistat
prdr_close (SMFICTX *ctx)
{
  std::unique_ptr<Privdata> priv {(Privdata*) smfi_getpriv (ctx)};
  smfi_setpriv (ctx, nullptr);
  return SMFIS_CONTINUE;
}

static sfsistat
prdr_abort (SMFICTX *ctx)
{
  std::unique_ptr<Privdata> priv {(Privdata*) smfi_getpriv (ctx)};
  smfi_setpriv (ctx, nullptr);
  return SMFIS_CONTINUE;
}
/*
static sfsistat prdr_negotiate(SMFICTX *ctx, unsigned long f0, unsigned long f1, unsigned long f2, unsigned long f3, unsigned long *fp0, unsigned long *fp1, unsigned long *fp2, unsigned long *fp3){
  *fp0 = f0;
  *fp1 = f1;// | SMFIP_HDR_LEADSPC;
  *fp2 = f2;
  *fp3 = f3;
  return SMFIS_CONTINUE;
}
*/
extern "C" {
struct smfiDesc smfilter =
{
    const_cast<char*>("aegee-milter"), /* filter name */
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
}
