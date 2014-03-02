#ifndef RECIPIENT_HPP
#define RECIPIENT_HPP
#include <vector>

class Module;

class Recipient final {
public:
  Recipient(char** argv);
  ~Recipient() = default;
  Recipient(const Recipient&) = delete;
  Recipient(Recipient&&) = default;
  Recipient& operator=(const Recipient&) = delete;
  Recipient& operator=(Recipient&&) = delete;
  const std::string address;
  std::vector<Module*> modules;
  long flags = 0;//binary ORed MOD_ flags or
  //RCPT_PRDR_REJECTED - if the recipient was rejected because of laking PRDR support
  //RCPT_NOTIFY_NEVER
};
#endif
