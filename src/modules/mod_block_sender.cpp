#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

struct block_sender final : public SoModule {
  int Status (Privdata&) const override {
    return MOD_RCPT;
  }

  bool Run (Privdata& priv) const override {
    if (priv.GetEnvSender () == "renekeijzer@mail.com") {
      priv.SetResponse ("550", "5.7.0", "Rene, your mailbox is hacked and until you resolve the issue AEGEE.org is not going to accept mails from it.\r\n\r\n   Dilyan // AEGEE Mail Team ");
      AegeeMilter::ListInsert ("log", priv.GetEnvSender (), "mod_block_sender", "blocked");
    }
    return true;
  }
};

extern "C" SoModule* mod_block_sender_LTX_init () {
  return new block_sender;
}
