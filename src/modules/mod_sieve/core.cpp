#define __USE_GNU 1
#include <string.h>
#include <stdlib.h>
#include <time.h>

extern "C" {
  #include "config.h"
}
#include <iostream>
#include "src/core/AegeeMilter.hpp"
#include "src/modules/mod_sieve/message.hpp"

extern GKeyFile *prdr_inifile;
static gboolean mod_sieve_vacation = false;
static int mod_sieve_vacation_days_min = 1;
static int mod_sieve_vacation_days_default = 7;
static int mod_sieve_vacation_days_max = 30;
static gboolean mod_sieve_redirect = true;
static std::string mod_sieve_script_format;
int libsieve_run (Privdata&);
int libcyrus_sieve_run (Privdata&);


//In sieve_local, hashTable maps scriptname -> scriptcode for the recipient
//sieve_global.hashTable maps [scriptname:global scope] -> scriptcode
//sieve_global.recipients maps [recipient] maps the default script of the recipient, recipients[:global] is the global default script
struct sieve_global {
  std::unordered_map<std::string, std::string> hashTable;
  std::unordered_map<std::string, std::string> recipients;//default scripts for the recipients
};

HIDDEN
mod_sieve_Action::mod_sieve_Action (mod_action_t a, const char* p) : a (a) {
  switch (a) {
  case mod_action_t::ACTION_REJECT:
    u.rej.msg = strdup (p);
    break;
  case mod_action_t::ACTION_FILEINTO:
    u.fil.mailbox = strdup (p);
    break;
  case mod_action_t::ACTION_KEEP:
    break;
  case mod_action_t::ACTION_REDIRECT:
    u.red.addr = strdup (p);
    break;
  case mod_action_t::ACTION_DISCARD:
    break;
  case mod_action_t::ACTION_VACATION:
    break;
  }
}

HIDDEN
mod_sieve_Action::~mod_sieve_Action () {
  switch (a) {
  case mod_action_t::ACTION_REJECT:
    free (u.rej.msg);
    break;
  case mod_action_t::ACTION_FILEINTO:
    free (u.fil.mailbox);
    break;
  case mod_action_t::ACTION_KEEP:
    break;
  case mod_action_t::ACTION_REDIRECT:
    free (u.red.addr);
    break;
  case mod_action_t::ACTION_DISCARD:
    break;
  case mod_action_t::ACTION_VACATION:
    free (u.vac.send.addr);
    free (u.vac.send.fromaddr);
    free (u.vac.send.msg);
    free (u.vac.send.subj);
    free (u.vac.autoresp.hash);
    break;
  }
}

HIDDEN void sieve_local::delete_headers () {
  if (headers) {
    int i = 0;
    while (headers[i]) {
      free (const_cast<char*> (headers[i]));
      i++;
    }
    free (headers);
  }
}

HIDDEN sieve_local::~sieve_local () {
  delete_headers ();
}

static std::string
substitute_named_variable (Privdata& priv, const std::string& variable)
{
  switch (variable[0]) {
  case 'a':
    if (variable == "advertisement")
      return "";//"----    ----    ----    ----    ----\r\nClick to get your own mailing list:\r\nhttps://lists.aegee.org/new\r\n    Service provided by AEGEE Mail Team\r\n    http://mail.aegee.org/\r\n----    ----    ----    ----    ----";
    break;
  case 'e':
    if (variable == "envelope.from")
      return priv.GetEnvSender ();
    if (variable == "envelope.to")
      return priv.GetRecipient ();
    break;
  case 'h':
    size_t index = variable.find ("headers.");
    if (index != std::string::npos) {
      if (priv.GetStage () < MOD_HEADERS ) {
	sieve_local *dat = (sieve_local*)priv.GetPrivRcpt ();
	dat->desired_stages |= MOD_HEADERS;
	dat->failed = 1;
	priv.DoFail ();
	return "mod_sieve/subst_var:defect";
      }
      const std::vector<std::string>& headers = priv.GetHeader({variable, 8});
      return (headers.empty ()) ? "mod_sieve/subst_var: nichts gefunden" : headers.front();
    }
    break;
  };
  return "";
}

static std::string
expand_variables_in_string (Privdata& cont, const std::string& input)
{
  std::string ret = input;
  size_t pos = 0;
  while (pos != std::string::npos) {
    pos = ret.find("${", pos);
    if (pos != std::string::npos) {
      const size_t end = ret.find("}", pos + 2);
      if (end != std::string::npos)
	ret.replace (pos, end - pos + 1, substitute_named_variable (cont, {ret, pos + 2, end - pos - 2}));
      else break;
    }
  }
  return ret;
}

//rcpt == "" -> global default script
static const std::string&
get_default_script_for_recipient (Privdata& cont, const std::string& rcpt)
{
  sieve_global *glob = (sieve_global*) cont.GetPrivMsg ();
  //  std::string ret;
  try {
    return glob->recipients.at (rcpt);//ret = glob->recipients.at (rcpt);
  } catch (const std::out_of_range&) {
    return glob->recipients[rcpt] = AegeeMilter::ListQuery
      (mod_sieve_script_format, rcpt.empty () ? ":global" : rcpt, "");
  }
  //for the future
  /*
   * if (!ret.empty () && (ret.find ("vacation") != std::string::npos
   *	       || ret.find ("X-Aegee-Spam-Level") != std::string::npos)) {
   *    cont.SetActivity ("spamassassin", 1);
   *  //std::cout << "spamassassin activated for " << cont.GetRecipient () << std::endl;
   *  } else
   *    cont.SetActivity ("spamassassin", 2);
   */
  //  return ret;
}

HIDDEN const std::string&
sieve_getscript (const std::string& path, const std::string& name, Privdata& cont)
{
  if (path.empty() || path != ":global") {//the scope is private
    if (name.empty ()) {
      //default script for recipient
      const std::string& def = get_default_script_for_recipient (cont, cont.GetRecipient ());
      return def.empty () ? /* load the global default script */
	get_default_script_for_recipient (cont, "") : def;
    }
    sieve_local *dat = (sieve_local*) cont.GetPrivRcpt ();
    try {
      return dat->hashTable.at (name);
    } catch (const std::out_of_range&) {
      return dat->hashTable[name] = AegeeMilter::ListQuery (mod_sieve_script_format, cont.GetRecipient (), name);
    }
  } else { //the scope is global
    if (name.empty ())
      return get_default_script_for_recipient (cont, "");
    sieve_global *glob = (sieve_global*) cont.GetPrivMsg ();
    try {
      return glob->hashTable.at (name);
    } catch (const std::out_of_range&) {
      return glob->hashTable[name] = AegeeMilter::ListQuery (mod_sieve_script_format, ":global", name);
    }
  }
}

static int
sieve_fileinto (const mod_sieve_fileinto_context& /*context*/, Privdata& priv)
{
  sieve_local *dat = (sieve_local*)priv.GetPrivRcpt ();
  if (priv.GetStage () != MOD_BODY) {
    dat->desired_stages |= MOD_BODY;
    priv.DoFail ();
    dat->failed = true;
    return SIEVE2_ERROR_FAIL;
  };
  /*
  const std::string& fileinto = priv.GetRecipient () + "+" + context.mailbox;
  priv.AddRecipient (fileinto);
  priv.DelRecipient (priv.GetRecipient ());
  */
  return SIEVE2_OK;
}

static int
sieve_redirect (const mod_sieve_redirect_context_t& context, Privdata& priv)
{
  char IP[256];
  struct sockaddr* addr = priv.hostaddr;
  switch (addr->sa_family) {
      case AF_INET:
	inet_ntop  (AF_INET, &((sockaddr_in*)addr)->sin_addr, IP, 256);
	break;
      case AF_INET6:
	inet_ntop (AF_INET6, &((sockaddr_in6*)addr)->sin6_addr, IP, 256);
	break;
  }
  //Date
  static const char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  static const char *wday[] = { "Sun", "Mon", "Tue", "Wed",
				"Thu", "Fri", "Sat" };
  time_t _time = time (NULL);
  struct tm lt;
  long gmtoff = -0;
  int gmtneg = 0;
  localtime_r (&_time, &lt);
  char date[64];
  snprintf (date, 64, "%s, %02d %s %4d %02d:%02d:%02d %c%.2lu%.2lu",
	    wday[lt.tm_wday], lt.tm_mday, month[lt.tm_mon],
	    lt.tm_year + 1900, lt.tm_hour, lt.tm_min, lt.tm_sec,
	    gmtneg ? '-' : '+', gmtoff / 60, gmtoff % 60);
  //prepend Received: header, as of RFC5231, Section 4.4. Trace Information
  std::string body = "Received: from " + priv.ehlohost + " [" + IP
    + "]\r\n        by " + priv.domain_name + " id " + priv.queue_id
    + ";\r\n        " + date + "\r\nResent-From: "
    + priv.GetEnvSender () + "\r\nResent-To: " + context.addr + "\r\n";
  AegeeMilter::ListInsert ("log", priv.GetRecipient (), "mod_sieve, action redirect, to: ", context.addr);
  //Resent-Sender, Resent-CC, Resent-BCC, Resent-Msg-Id
  const std::vector<std::tuple<std::string, std::string, uint8_t>>&
    p = priv.GetHeaders ();
  for (const std::tuple<std::string, std::string, uint8_t>& h : p)
    body += std::get<0>(h) + ": " + std::get<1>(h) + "\r\n";
  //insert body
  AegeeMilter::Sendmail (priv.GetEnvSender(),
	 std::vector<std::string> {context.addr},
	 body + "\r\n" + priv.GetBody (), "Resent-Date", "auto-generated");
  priv.DelRecipient (priv.GetRecipient ());
  return SIEVE2_OK;
}

static int
sieve_discard (Privdata& cont)
{
  AegeeMilter::ListInsert ("log", cont.GetRecipient (),
			   "mod_sieve, action discard, from: ",
			   cont.GetEnvSender ());
  cont.DelRecipient (cont.GetRecipient ());
  return SIEVE2_OK;
}

static int
sieve_reject (const mod_sieve_reject_context& context, Privdata& priv)
{
  const std::string& return_text = expand_variables_in_string
    (priv, context.msg);
  sieve_local *dat = (sieve_local*)priv.GetPrivRcpt ();
  if (!dat->failed) {
    priv.SetResponse ("550", "5.7.1", return_text);
    priv.DelRecipient (priv.GetRecipient ());
    AegeeMilter::ListInsert ("log", priv.GetRecipient (), "from " + priv.GetEnvSender() + ", mod_sieve, action reject:", return_text);
  }
  return SIEVE2_OK;
}

static int
sieve_vacation (mod_sieve_autorespond_context& autoresp,
		const mod_sieve_send_response_context& send, Privdata& cont)
{
  if (!mod_sieve_vacation) return SIEVE2_OK;
  std::string temp = cont.GetEnvSender ();
  size_t index = temp.find ('@');
  if (index == std::string::npos) return SIEVE2_OK;//this is a bounce -- no vacation
  temp[index] = '\0';//begin section 4.6. of RFC 5230
  if (temp.find ("owner-") == 0 || temp == "<>" ||
      strcasestr (temp.c_str (), "-request") ||
      strcasestr (temp.c_str (), "LISTSERV") ||
      strcasestr (temp.c_str (), "MAILER-DAEMON") ||
      strcasestr (temp.c_str (), "majordomo") ) {
    return SIEVE2_OK;
  }

  const std::vector<std::string>& headers1 = cont.GetHeader ("X-Spam-Status");
  if (!headers1.empty ()) {
    cont.AddHeader (0, "X-AEGEE-milter", "mod_sieve vacation not sent, as X-Spam-Status is missing");
    AegeeMilter::ListInsert ("log", cont.GetRecipient (), "vacation", "not sent -- X-Spam-Status is missing");
    return SIEVE2_OK;
  }
  const std::vector<std::string>& headers2 = cont.GetHeader ("X-Aegee-Spam-Level");
  if (!headers2.empty () && headers2.front ().size () > 2) {
    cont.AddHeader (0, "X-AEGEE-milter",
	       "mod_sieve vacation not sent -- spam probability too high");
    AegeeMilter::ListInsert ("log", cont.GetRecipient (), "vacation",
		"not sent -- spam probability is too high");
    return SIEVE2_OK;
  }

  if (autoresp.days == 0) autoresp.days = mod_sieve_vacation_days_default; else
  if (autoresp.days < mod_sieve_vacation_days_min) autoresp.days = mod_sieve_vacation_days_min; else
  if (autoresp.days > mod_sieve_vacation_days_max) autoresp.days = mod_sieve_vacation_days_max;

  /*
  const std::string handle& = cont.GetEnvSender () + ":" + autoresp.hash;
  if (!AegeeMilter::ListInsert ("vacation", cont.GetRecipient (),
			      handle, "handle", 24 * 60 * 60 * autoresp.days)) 
  {
    cont.AddHeader (0, "X-AEGEE-milter", "mod_sieve vacation not sent -- already replied to this sender");
    AegeeMilter::ListInsert ("log", cont.GetRecipient (), "vacation", "not sent -- already replied to this sender", 0);
    return SIEVE2_OK;
  }
  */
  cont.AddHeader (0, "X-AEGEE-milter", "mod_sieve vacation sent");
  AegeeMilter::ListInsert ("log", cont.GetRecipient (), "vacation", "sent");
  //  char const * const address UNUSED = send.addr;

  std::string msg = std::string{"Subject: "} + send.subj
					       + "\r\n+References: \r\n";
  const std::vector<std::string>& headers = cont.GetHeader ("References");
  if (!headers.empty ()) {
    for (const std::string& h : headers)
      msg += "    " + h + "\r\n";
  } else {
    const std::vector<std::string>& h4 = cont.GetHeader ("In-Reply-To");
    if (!h4.empty ())
      msg += "     " + h4.front () + "\r\n";
  }
  const std::vector<std::string>& h5 = cont.GetHeader ("Message-Id");
  if (!h5.empty ()) {
    msg += "     " + h5.front () + "\r\n" //finish References
      + "In-Reply-To: " + h5.front () + "\r\n";
  }
  msg += "From: " + (send.fromaddr ? send.fromaddr : cont.GetRecipient ()) + "\r\nTo: " + cont.GetEnvSender () + "\r\n";
  if (send.mime) msg += "Mime-Version: 1.0\r\n";
  //FIXME add RCPT
  msg += std::string{"\r\n"} + send.msg;
  AegeeMilter::Sendmail ("", std::vector<std::string> {cont.GetEnvSender ()},
			 msg, "Date", "auto-replied");
  return SIEVE2_OK;
}

static int
sieve_keep (Privdata& priv)
{
  priv.AddRecipient (priv.GetRecipient ());
  return SIEVE2_OK;
}

struct sieve final : public SoModule {
    sieve () {
      mod_sieve_redirect = true;
      bool vac_err = false;
      if (char **array = g_key_file_get_keys (prdr_inifile, "mod_sieve", NULL, NULL)) {
	int i = 0;
	while (array[i]) {
	  if (strcmp (array[i], "vacation") == 0) {
	    mod_sieve_vacation = g_key_file_get_boolean (prdr_inifile,
				"mod_sieve", "vacation", NULL); } else
          if (strcmp(array[i], "vacation_days_min") == 0) {
	    mod_sieve_vacation_days_min = g_key_file_get_integer (prdr_inifile,
				"mod_sieve", "vacation_days_min", NULL); } else
          if (strcmp(array[i], "vacation_days_default") == 0) {
	    mod_sieve_vacation_days_default = g_key_file_get_integer (
	      prdr_inifile, "mod_sieve", "vacation_days_default", NULL); } else
          if (strcmp(array[i], "vacation_days_max") == 0) {
	    mod_sieve_vacation_days_max = g_key_file_get_integer (prdr_inifile,
				"mod_sieve", "vacation_days_max", NULL); } else
          if (strcmp (array[i], "redirect") == 0) {
	    mod_sieve_redirect = g_key_file_get_boolean (prdr_inifile,
				"mod_sieve", "redirect", NULL); } else {
	    std::cerr << "option " << array[i] << " in section [mod_sieve] is not recognized" << std::endl;
	    g_strfreev (array);
	    throw -1;
	  }
	  i++;
	}
	g_strfreev (array);
      };
      if (mod_sieve_vacation_days_min < 1) {
	vac_err = true;
        std::cerr << "vacation_days_min is set to " << mod_sieve_vacation_days_min << " in section [mod_sieve], but it cannot be less than one." << std::endl;
      }
      if (mod_sieve_vacation_days_max < 1) {
	vac_err = true;
	std::cerr << "vacation_days_max is set to " << mod_sieve_vacation_days_max << " in section [mod_sieve], but it cannot be less than one" << std::endl;
      }
      if (mod_sieve_vacation_days_min > mod_sieve_vacation_days_default) {
	vac_err = true;
	std::cerr << "vacation_days_default (" << mod_sieve_vacation_days_default << ") cannot be set to less than vacation_days_min (" << mod_sieve_vacation_days_min << ") in section [mod_sieve]" << std::endl;
      }
      if (mod_sieve_vacation_days_default > mod_sieve_vacation_days_max) {
	vac_err = true;
	std::cerr << "vacation_days_default(" << mod_sieve_vacation_days_default << ") cannot be greater than vacation_days_max("<< mod_sieve_vacation_days_max <<") in section [mod_sieve]"<< std::endl;
      }
      if (vac_err) {
	std::cout << "--> vacation is disabled" << std::endl;
	mod_sieve_vacation = false;
      }
      //libsieve
      //mod_sieve_script_format = "sieve_scripts";
      //libcyrus_sieve
      mod_sieve_script_format = "sieve_bytecode_path";
    }

    bool Run (Privdata& priv) const override {
    //priv->AddHeader (0, "X-AEGEE-milter-mod_sieve", "sieve script executed");
      sieve_local* dat = (sieve_local*) priv.GetPrivRcpt ();
      dat->failed = false;
      //const  int ret = libsieve_run (priv);
      const bool ret = libcyrus_sieve_run (priv);
      if (dat->failed) {
	dat->actions.clear ();
	dat->delete_headers ();
	dat->headers = nullptr;
      } else {
	for (mod_sieve_Action& a: dat->actions)
	  switch (a.a) {
	  case mod_action_t::ACTION_FILEINTO:
	    sieve_fileinto (a.u.fil, priv);
	    break;
	  case mod_action_t::ACTION_DISCARD:
	    sieve_discard (priv);
	    break;
	  case mod_action_t::ACTION_REJECT:
	    sieve_reject (a.u.rej, priv);
	    break;
	  case mod_action_t::ACTION_REDIRECT:
	    sieve_redirect (a.u.red, priv);
	    break;
	  case mod_action_t::ACTION_VACATION:
	    sieve_vacation (a.u.vac.autoresp, a.u.vac.send, priv);
	    break;
	  case mod_action_t::ACTION_KEEP:
	    sieve_keep (priv);
	    break;
	  }
	dat->actions.clear ();
	dat->delete_headers ();
	dat->headers = nullptr;
      }
      return ret;
    }

    int Status (Privdata& priv) const override {
      return priv.GetStage() < MOD_RCPT ? MOD_RCPT : ((sieve_local*) priv.GetPrivRcpt ())->desired_stages;
    }

    void InitMsg (Privdata& priv) const override {
      priv.SetPrivMsg (new sieve_global {});
    }

    void InitRcpt (Privdata& priv) const override {
      priv.SetPrivRcpt (new sieve_local {});
    }

    void DestroyMsg (Privdata& priv) const override {
      delete (sieve_global*)priv.GetPrivMsg ();
    }

    void DestroyRcpt (Privdata& priv) const override {
      delete (sieve_local*)priv.GetPrivRcpt ();
    }

    bool Equal (Privdata& priv, const Recipient& r1, const Recipient& r2) const override {
      //returns true if the recipients are considered equal
      const std::string& u1 = get_default_script_for_recipient (priv, r1.address);
      const std::string& u2 = get_default_script_for_recipient (priv, r2.address);
      if (u1 == u2) return true;//check if u1 and u2 are empty
      if (!u1.empty () && !u2.empty ())
	return (0 == g_ascii_strcasecmp(u1.c_str (), u2.c_str ()));
      return false;
    }
};

extern "C" SoModule* mod_sieve_LTX_init () {
  return new sieve;
}
