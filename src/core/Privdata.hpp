#ifndef PRIVDATA_HPP
#define PRIVDATA_HPP

#include <list>
#include <string>
#include "src/prdr-milter.h"
#include "src/core/Module.hpp"
#include "src/core/Recipient.hpp"

extern "C" {
  #include <arpa/inet.h>
  #include <libmilter/mfapi.h>
}

//functions that can be used in the prdr modules, apart from the ones included in prdr-list.h
class Privdata final {
  unsigned int size;
  bool mime8bit;
  void CompactHeaders (unsigned int);
public:
  Privdata (SMFICTX *ctx, const std::string& hostname,
           const std::string& domain_name, struct sockaddr *hostaddr);
  ~Privdata ();
  void SetResponse (const std::string& code, const std::string& dsn, const std::string& reason);
  void InitModules ();
  void RunModules ();
  void ClearModules ();
  void DoFail ();
  inline int GetStage () { return stage; }
  const std::string& GetRecipient () const;
  std::vector<std::string> GetRecipients () const;
  void AddRecipient (const std::string& address);
  void DelRecipient (const std::string& address);
  std::vector<std::string> GetHeader (const std::string&);
  void AddHeader (const unsigned char index, const char* const field, const char* const value);
  void DelHeader (const unsigned char, const char* const);//last = 1, at the end, otherwise at the beginning
  const std::vector<std::tuple<std::string, std::string, uint8_t>>& GetHeaders();
  int GetSize ();
  void SetEnvSender (const std::string& env);
  const std::string& GetEnvSender ();
  const std::string& GetBody ();
  void SetBody (const std::string&);
  void* GetPrivRcpt ();
  void SetPrivRcpt (void* const);
  void* GetPrivMsg ();
  void SetPrivMsg (void* const);
  void ProceedMailFromParams (char** argv);
  int SetResponses ();
  bool operator()(Recipient*);
  SMFICTX *ctx;
  const std::string hostname;
  const std::string domain_name;
  Message msg;
  std::string ehlohost;
  std::string queue_id;
  struct sockaddr *hostaddr;
  std::vector<Recipient> recipients;
  Recipient* current_recipient;
  unsigned int stage = MOD_EHLO;
  void** msgpriv;
  unsigned int current_soModule;
  Module* current_module;
  std::list<Module> module_pool;
  bool prdr_supported; //logically ORed MSG_ flags
  int InjectResponse(const std::string&, const std::string&,
		     const std::string&);
};
#endif
