#ifndef SoModule_hpp
#define SoModule_hpp

extern "C" {
  #include <ltdl.h>
  #include "src/prdr-milter.h"
}

class SoModule {
public:
  SoModule (const char * const &name);
  int Run (struct privdata* &);
  int Status (struct privdata* &);
  int InitMsg (struct privdata* &);
  int InitRcpt (struct privdata* &);
  int DestroyMsg (struct privdata* &);
  int DestroyRcpt (struct privdata* &);
  int Equal (struct privdata* &, const char* &, const char* &);
  const char* GetName();
  ~SoModule();
  SoModule(const SoModule&) = delete;
  void operator=(const SoModule&) = delete;
private:
  lt_dlhandle mod;
  const char * const name;
  int (*run) (struct privdata*);
  int (*status) (struct privdata*);
  int (*init_msg) (struct privdata*);
  int (*init_rcpt) (struct privdata*);
  int (*destroy_msg) (struct privdata*);
  int (*destroy_rcpt) (struct privdata*);
  int (*equal) (struct privdata*, const char* &, const char* &);
};

#endif
