#include "src/core/Module.hpp"

Module::Module(const SoModule& s) : so_mod(s) {}

bool Module::operator!=(const Module& mod) {
  return ! (mod.so_mod == so_mod);
}

bool Module::operator==(const Module& mod) {
  return mod.so_mod == so_mod;
}
