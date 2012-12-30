#ifndef SoList_hpp
#define SoList_hpp

#include <string>
extern "C" {
  #include <ltdl.h>
  #include "src/prdr-milter.h"
};

class SoList {
public:
  SoList (const char * const &name);
  char* Query (const char*, const char*, const char*);
  int Expire ();
  int Remove (const char*, const char*, const char*);
  int Insert (const char*, const char*, const char*, const char*, const unsigned int);
  char** Tables();
  ~SoList ();
private:
  lt_dlhandle mod;
  int   (*expire) ();
  char* (*query) (const char*, const char *, const char*);
  int   (*remove) (const char*, const char*, const char*);
  int   (*insert) (const char*, const char*, const char*, const void*, const unsigned int);
};
#endif
