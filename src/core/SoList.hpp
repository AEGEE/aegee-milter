#ifndef SoList_hpp
#define SoList_hpp

#include <string>
#include <vector>
extern "C" {
  #include <ltdl.h>
}

class SoList {
protected:
  SoList ();
public:
  lt_dlhandle mod;
  virtual std::string Query (const std::string&, const std::string&, const std::string&) { return ""; };
  virtual int Expire () {return 0;};
  virtual bool Remove (const std::string&, const std::string&,
		      const std::string&) {return false;};
  virtual void Insert (const std::string&, const std::string&,
		      const std::string&, const std::string&,
		      const unsigned int) {};
  virtual std::vector<std::string> Tables() = 0;
  virtual ~SoList ();
};
#endif
