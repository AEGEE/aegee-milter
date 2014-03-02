extern "C"{
  #include <libmilter/mfapi.h>
}

#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

struct x_scc_headers final : public SoModule {
  x_scc_headers () : SoModule (true) {};

  bool Run (Privdata& priv) const override {
    const std::vector<std::string>& header = priv.GetHeader ("X-SCC-Spam-Level");
    if (!header.empty())
      AegeeMilter::ListInsert ("log", priv.GetRecipient(),
				"mod_x_scc_headers, X-SCC-Spam-Level:",
				header.front());
    SMFICTX *ctx = priv.ctx;
    smfi_chgheader (ctx, const_cast<char*>("X-SCC-Debug-Zeilenzahl"), 1, NULL);
    smfi_chgheader (ctx, const_cast<char*>("X-Scan-Signature"), 1, NULL);
    smfi_chgheader (ctx, const_cast<char*>("X-Scan-Server"), 1, NULL);
    smfi_chgheader (ctx, const_cast<char*>("X-SCC-Spam-Level"), 1, NULL);
    smfi_chgheader (ctx, const_cast<char*>("X-SCC-Spam-Status"), 1, NULL);
    return true;
  }

  int Status (Privdata&) const override {
    return MOD_BODY;
  }
};

extern "C" SoModule* mod_x_scc_headers_LTX_init () {
  return new x_scc_headers;
}
