#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

static std::string
received_via (const std::string& received)
{
  size_t index_start = received.find ('[');
  if (index_start != std::string::npos) {
    size_t index_end = received.find (']', index_start + 1);
    if (index_end != std::string::npos)
      return {received, index_start + 1, index_end - index_start - 1};
  }
  return "";
}

struct via final : public SoModule {
  via () : SoModule (true) {}

  int Status (Privdata&) const override {
    return MOD_HEADERS;
  }

  bool Run (Privdata& priv) const override {
    const std::vector<std::string>& received = priv.GetHeader ("Received");
    if (!received.empty()) {
      const std::string ip = received_via (received.front ());
      if (!ip.empty ())
	AegeeMilter::ListInsert ("log", priv.GetRecipient (), "mod_via", ip);
    }
    return true;
  }
};

extern "C" SoModule* mod_via_LTX_init () {
  return new via;
}
