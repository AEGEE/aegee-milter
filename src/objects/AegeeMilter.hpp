#ifndef AegeeMilter_hpp
#define AegeeMilter_hpp

#include <map>
#include <vector>
extern "C" {
  #include <glib.h>
}

#include "src/objects/SoList.hpp"
#include "src/objects/SoModule.hpp"

class AegeeMilter {
public:
  static std::vector<SoModule*> so_modules;
  static std::vector<SoList*> so_lists;
  static std::map<std::string, SoList*> tables;
  AegeeMilter();
  ~AegeeMilter();
  void Fork();
  int ProceedConfFile(const char* filename);
  static void UnloadPlugins();
private:
  int LoadPlugins();
};
#endif
