#include <iostream>

#include "src/objects/SoModule.hpp"

extern "C" {
  extern const lt_dlsymlist lt_preloaded_symbols[];
}

SoModule::SoModule(const char* const &name) {
  this->name = name;
  if (name[0] != 'm')
    throw -2;

  this->mod = lt_dlopen (name);
  int (*load)() = (int (*)()) lt_dlsym (this->mod, "load");
  if (load && (0 != load ())) {
    lt_dlclose (this->mod);
    std::cout << "Loading " << name << "failed. Exiting..." << std::endl;
    throw -1;
  }
  run = (int(*)(struct privdata*))lt_dlsym(mod, "prdr_mod_run");
  if (run == NULL) {
    std::cout << "Module \"" << name << "\" does not define 'prdr_mod_run'. aegee-milter exits..." << std::endl;
    lt_dlclose(mod);
    throw -3;
  }
  status = (int(*)(struct privdata*))lt_dlsym(mod, "prdr_mod_status");
  if (status == NULL) {
    std::cout << "Module \"" << name << "\" does not define 'prdr_mod_status'. aegee-milter exits..." << std::endl;
    lt_dlclose(mod);
    throw -4;
  }
  equal = (int (*)(struct privdata*,const char*&, const char*&))lt_dlsym(mod, "prdr_mod_equal");
  init_msg = (int(*)(struct privdata*))lt_dlsym (mod, "prdr_mod_init_msg");
  destroy_msg = (int(*)(struct privdata*))lt_dlsym (mod, "prdr_mod_destroy_msg");
  init_rcpt = (int(*)(struct privdata*))lt_dlsym (mod, "prdr_mod_init_rcpt");
  destroy_rcpt = (int(*)(struct privdata*))lt_dlsym (mod, "prdr_mod_destroy_rcpt");
}

int SoModule::Run (struct privdata* &x) { return run (x); }
int SoModule::Status (struct privdata* &x) { return status (x); }
int SoModule::InitMsg (struct privdata* &x) {
  return (init_msg != NULL) ? init_msg (x) : 0;
}

int SoModule::InitRcpt (struct privdata* &x) {
  return (init_rcpt != NULL) ? init_rcpt (x) : 0;
}

int SoModule::DestroyMsg (struct privdata* &x) {
  int ret;
  if (destroy_rcpt == NULL) {
    ret = 0;
  } else
    ret = destroy_msg(x);
  return ret;
}

int SoModule::DestroyRcpt (struct privdata* &x) {
  int ret;
  if (destroy_rcpt == NULL) {
    ret = 0;
  } else {
    ret = destroy_rcpt (x);
  }
  return ret;
}

int SoModule::Equal (struct privdata* &x, const char* &a, const char* &b) {
  return equal(x, a, b);
}
const char* SoModule::GetName () { return name.c_str(); }

SoModule::~SoModule () {
  void (*unload)();
  unload = (void(*)())lt_dlsym(mod, "unload");
  if (unload) unload ();
  lt_dlclose (mod);
}
