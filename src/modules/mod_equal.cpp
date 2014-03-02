#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

class equal final : public SoModule {
  bool Run (Privdata& priv) const override {
    const std::string& rcpt_to = priv.GetRecipient ();
    const std::string& mail_from = priv.GetEnvSender ();

    if (priv.GetStage () == MOD_RCPT) {
      if (g_ascii_strcasecmp (rcpt_to.c_str (), mail_from.c_str ()) == 0 &&
	  g_ascii_strcasecmp (mail_from.c_str (), "mail@aegee.org") ) {
	AegeeMilter::ListInsert ("log", rcpt_to, "mod_equal, reject from:",
				 mail_from);
	priv.SetResponse ("550", "5.1.0",
			  "Envelope sender and recipient cannot coincide in our domain (site policy).");
      }
    } else 
      if (mail_from == "<>") {
	const std::string& body = priv.GetBody ();
	if (body.find ("Envelope sender and recipient cannot coinside in our domain.") != std::string::npos) {
	  AegeeMilter::ListInsert ("log", rcpt_to,
			    "mod_equal, mail already rejected (env. sender and recipient coincide)",
			    mail_from);
	  priv.SetResponse ("550", "5.1.0",
			    "Message contains: \"Envelope sender and recipient cannot coincide in our\r\ndomain (site policy).\" -- we have already rejected this email.");
	} else if (body.find ("host aegeeserv.aegee.uni-karlsruhe.de [129.13.131.8") != std::string::npos) {
	  AegeeMilter::ListInsert ("log", rcpt_to, "mod_equal, mail already rejected (host aegeeserv)", mail_from);
	  priv.SetResponse ("550", "5.1.0",
			    "Message contains: \"host aegeeserv.aegee.uni-karlsruhe.de [129.13.131.80]:\"\r\n -- we have already rejected this email.");
	}
      }
    return true;
  }

  int Status (Privdata&) const override {
    return MOD_RCPT | MOD_BODY;
  }
};

extern "C" SoModule* mod_equal_LTX_init () {
  return new equal;
}
