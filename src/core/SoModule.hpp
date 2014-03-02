#ifndef SoModule_hpp
#define SoModule_hpp

#include "src/core/Recipient.hpp"

extern "C" {
  #include <ltdl.h>
}

class Privdata;
class SoModule {
protected:
  SoModule ();
  SoModule (bool);
  bool equal = false;
public:
  lt_dlhandle mod;
  std::string name;
  bool operator==(const SoModule&) const;
  virtual bool Run (Privdata&) const;//false -- module failed
  virtual int Status (Privdata&) const;
  virtual void InitMsg (Privdata&) const;
  virtual void InitRcpt (Privdata&) const;
  virtual void DestroyMsg (Privdata&) const;
  virtual void DestroyRcpt (Privdata&) const;
  virtual bool Equal (Privdata&, const Recipient&, const Recipient&) const;
  virtual ~SoModule();
};
#endif
