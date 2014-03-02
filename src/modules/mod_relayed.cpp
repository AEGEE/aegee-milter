#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

struct relayed final : SoModule {
  relayed () : SoModule (true) {};

  bool Run (Privdata& priv) const override {
    const std::string& mail_from = priv.GetEnvSender ();
    const std::string& rcpt_to = priv.GetRecipient ();
    AegeeMilter::ListInsert ("log", rcpt_to, "mod_relayed, envelope from:",
		    mail_from);
    try {
      AegeeMilter::ListInsert ("relayed", rcpt_to /* to */,
			       mail_from /* from */, "" /*value */);
    } catch (std::out_of_range&) {
      //odbc not loaded, no relayed table
    }
    return true;
  }

  int Status (Privdata&) const override {
    return MOD_BODY;
  }
};

extern "C" SoModule* mod_relayed_LTX_init () {
  return new relayed;
}
