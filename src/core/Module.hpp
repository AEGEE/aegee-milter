#ifndef Module_hpp
#define Module_hpp

#include <string>
#include "src/core/SoModule.hpp"
#include "src/core/Message.hpp"

class SoModule;

class Module final{
public:
  Module(const SoModule& somod);
  Module(const Module&) = delete;
  Module(class Module&&) = delete;
  bool operator!=(const Module&);
  bool operator==(const Module&);
  Module& operator=(const Module&) = delete;
  Module& operator=(Module&&) = delete;
  ~Module() = default;
  const SoModule& so_mod;
  void* private_ = nullptr;
  uint8_t flags = 0;
  //see MOD_... flags
  Message msg;
  std::string return_reason;
  std::string return_code;
  std::string return_dsn;
  int smfi_const = 0;
};
#endif
