#include <iostream>
#include "src/core/Privdata.hpp"

struct simple final : public SoModule {
  simple () : SoModule (true) {}

  bool Run(Privdata&) const override {
    return true;
  }

  int Status (Privdata&) const override {
    return MOD_RCPT | MOD_HEADERS;
  }

  void InitRcpt (Privdata& priv) const override {
    void *i = std::malloc(1024);
    std::cout << "MOD SIMPLE prdr_mod_init_rcpt=" << i << std::endl;
    priv.SetPrivRcpt (i);
  }

  void InitMsg (Privdata& priv) const override {
    void *i = std::malloc (1024);
    std::cout << "MOD SIMPLE prdr_mod_init_msg=" << i << std::endl;
    priv.SetPrivMsg (i);
  }

  void DestroyMsg (Privdata& priv) const override {
    void* i = priv.GetPrivMsg ();
    std::free (i);
    std::cout << "MOD SIMPLE prdr_mod_destroy_msg=" << i << std::endl;
  }

  void DestroyRcpt (Privdata& priv) const override {
    void* i = priv.GetPrivRcpt ();
    std::free (i);
    std::cout << "MOD SIMPLE prdr_mod_destroy_rcpt=" << i << std::endl;
  }
};

extern "C" SoModule* mod_simple_LTX_init () {
  return new simple;
}
