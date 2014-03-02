#ifndef Message_hpp
#define Message_hpp

#include <set>
#include <string>
#include <tuple>
#include <vector>

extern "C" {
#include "glib.h"
}

class Message {
public:
  Message (const std::string& envfrom);
  Message () = default;
  Message (const Message&) = delete;
  Message (Message&&) = delete;
  Message& operator= (const Message&) = delete;
  Message& operator= (Message&&) = default;
  ~Message() = default;
  bool AddHeader (const std::string& field, const std::string& value);
  std::string envfrom;
  bool deletemyself = false;
  std::set<std::string> envrcpts;
  std::vector<std::tuple<std::string, std::string, uint8_t>> headers;
  std::string body;
};

#endif
