#include <algorithm>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

Privdata::Privdata (SMFICTX* ctx, const std::string& hostname, 
		   const std::string& domain_name, struct sockaddr* hostaddr) :
  ctx (ctx), hostname (hostname), domain_name (domain_name)
{
  msgpriv = static_cast<void**>(calloc (sizeof(void*), AegeeMilter::so_modules.size ()));

  if (hostaddr) {
      switch(hostaddr->sa_family) {
          case AF_INET:
              this->hostaddr = (struct sockaddr*)malloc (sizeof (struct sockaddr_in));
              memcpy (this->hostaddr, hostaddr, sizeof (struct sockaddr_in));
              break;
          case AF_INET6:
              this->hostaddr = (struct sockaddr*)malloc (sizeof(struct sockaddr_in6));
	      memcpy (this->hostaddr, hostaddr, sizeof (struct sockaddr_in6));
              break;
      }
  };
}

Privdata::~Privdata() {
  stage = MOD_CLOSE;
  for (Module& mod : module_pool)
    if (mod.private_)
      for (current_soModule = 0;
	   current_soModule < AegeeMilter::so_modules.size();
	   current_soModule++)
	  if (mod.so_mod == *AegeeMilter::so_modules[current_soModule]) {
	    current_module = &mod;
	    mod.so_mod.DestroyRcpt (*this);
	    break;
	  }
  ClearModules ();
  free (msgpriv);
  free (hostaddr);
}

void Privdata::InitModules () {
  current_soModule = 0;
  for (auto& mod : AegeeMilter::so_modules) {
    mod->InitMsg (*this);
    current_soModule++;
  }
}

void Privdata::RunModules () {
  for (current_soModule = 0; current_soModule < AegeeMilter::so_modules.size();
       current_soModule++) {
    if (AegeeMilter::so_modules[current_soModule]->Status (*this) & stage)
      AegeeMilter::so_modules[current_soModule]->Run (*this);
  }
}

void Privdata::ClearModules () {
  for (current_soModule = 0; current_soModule < AegeeMilter::so_modules.size();
       current_soModule++)
    if (msgpriv[current_soModule]) {
      AegeeMilter::so_modules[current_soModule]->DestroyMsg (*this);
      msgpriv[current_soModule] = nullptr;
    }
}

void Privdata::DoFail() {
  current_module->flags |= MOD_FAILED;
}

const std::string& Privdata::GetRecipient() const {
  return current_recipient->address;
}

std::vector<std::string>
Privdata::GetRecipients() const {
  bool i = false;
  for (Module* mod : current_recipient->modules) {
    if (mod->msg.deletemyself)
      i = true;
    if (i || (mod == current_recipient->modules[current_soModule]))
      break;
  }

  std::vector<std::string> ret;
  if (!i)
    ret.push_back (current_recipient->address);
  for (int unsigned i = 0; i < current_soModule ; i++)
    if (!current_recipient->modules[i]->msg.envrcpts.empty () &&
	((current_recipient->flags & MOD_FAILED) == 0)) {
      ret.insert (ret.end(),
		  current_recipient->modules[i]->msg.envrcpts.begin (),
		  current_recipient->modules[i]->msg.envrcpts.end ());
    }//if
  return ret;
}

void Privdata::AddRecipient(const std::string& address) {
  if (address.empty ()) return;
  if (address == current_recipient->address)
    current_module->msg.deletemyself = false;
  else
    current_module->msg.envrcpts.insert (address);
}

void Privdata::DelRecipient (const std::string& address) {
  if (address.empty ()) return;
  if (address == current_recipient->address)
    current_module->msg.deletemyself = true;
  else
    current_module->msg.envrcpts.erase (address);
}

std::vector<std::string>
Privdata::GetHeader (const std::string& headerfield) {
  if (stage < MOD_HEADERS) {
    DoFail ();
    throw (int)1;
  }
  //check headers of global modules to global space
  std::vector<std::string> h;
  for (auto &it : msg.headers)
    if (!g_ascii_strcasecmp (std::get<0> (it).c_str (),headerfield.c_str ()))
      h.push_back (std::get<1> (it));

  return h;
}

void Privdata::AddHeader(const unsigned char index, const char* const field,
			 const char* const value) {
  for (auto &it : current_module->msg.headers)
    if (g_ascii_strcasecmp (std::get<0>(it).c_str(), field) == 0
	&& strcmp (std::get<1>(it).c_str(), value) == 0)
       return;

  current_module->msg.headers.emplace_back
    (field, value, index & 0x8F /*0111111b */);
}

void Privdata::DelHeader(const unsigned char index, const char* const field) {
  //last = 1, at the end, otherwise at the beginning
  current_module->msg.headers.emplace_back (field, "", (index & 0x8F) | 0xA0 );
}

void Privdata::SetResponse (const std::string& code /*5xx*/,
			   const std::string& dsn /* 1.2.3 */,
			    const std::string& reason) 
{
  current_module->return_code = code;
  current_module->return_dsn = dsn;
  current_module->return_reason = reason;
  switch (code[0]) {
  case '4':
    current_module->smfi_const = SMFIS_TEMPFAIL;
    //prdr_del_recipient(priv, prdr_get_recipient(priv));
    break;
  case '5':
    current_module->smfi_const = SMFIS_REJECT;
    //prdr_del_recipient(priv, prdr_get_recipient(priv));
    break;
  case '2':
  default:
    current_module->smfi_const = SMFIS_CONTINUE;
  };
}

int Privdata::GetSize () {
  if (stage != MOD_BODY) {
    if (!size) DoFail();
    return size;
  }
  //calculate size of headers
  unsigned int size_ = 2;
  // add the ":" header : field - delimiter, and \r\n at the end of each header, and \r\n after the last header
  for (const auto &it : GetHeaders ())
    size_ += std::get<0>(it).length () + std::get<1>(it).length() + 3;
  return size_ + GetBody ().length ();
}

const std::vector<std::tuple<std::string, std::string, uint8_t>>&
Privdata::GetHeaders() {
  return msg.headers;
}

void Privdata::SetEnvSender(const std::string& env) {
  if (stage == MOD_RCPT || stage == MOD_MAIL)
    DoFail ();
  else
    current_module->msg.envfrom = env;
}

const std::string& Privdata::GetEnvSender() {
  if (stage == MOD_EHLO) {
    std::cout << "Privdata::GetEnvSender, throwing 1" << std::endl;
    throw (int)2;
  }

  for (int i = current_soModule; i; i--)
    if ((!current_recipient->modules[i]->msg.envfrom.empty ())
	&& ((current_recipient->modules[i]->flags & MOD_FAILED) ==0))
      return current_recipient->modules[i]->msg.envfrom;

  return msg.envfrom;
}

const std::string& Privdata::GetBody() {
  if (stage != MOD_BODY) {
    DoFail();
    throw (int)3;
  }

  for (int i = current_soModule; i; i--)
    if (!current_recipient->modules[i]->msg.body.empty() &&
	((current_recipient->modules[i]->flags & MOD_FAILED) == 0))
      return current_recipient->modules[i]->msg.body;

  return msg.body;
}

void Privdata::SetBody (const std::string& body) {
  current_module->msg.body = body;
}

void* Privdata::GetPrivRcpt () {
  return current_module->private_;
}

void Privdata::SetPrivRcpt (void* const user) {
  current_module->private_ = user;
}

void* Privdata::GetPrivMsg () {
  return msgpriv[current_soModule];
}

void Privdata::SetPrivMsg (void* const user) {
  msgpriv[current_soModule] = user;
}

void Privdata::ProceedMailFromParams (char** argv) {
  mime8bit = false;
  size = 0;
  prdr_supported = false;
  //check for particular SMTP extensions that the client supports
  msg.envfrom = std::string{argv[0] + 1, strlen(argv[0]) - 2 };
  int i = 1;
  while (argv[i++])
    //check for PRDR extension
    if (!g_ascii_strcasecmp (argv[i-1], "PRDR"))
      prdr_supported = true;
    else if (g_ascii_strncasecmp (argv[i-1], "SIZE=", 5) == 0)
      size = std::strtol (argv[i-1] + 5, NULL, 10);
    else if (g_ascii_strcasecmp (argv[i-1], "BODY=8BITMIME")==0)
      mime8bit = true;
}

//moves headers of global modules to the headers of the original message
//checks for module Nr. i
void Privdata::CompactHeaders(unsigned int i)
{
  //check if the initial modules are global
  Module &mod = *(current_recipient->modules[i]);
  if (mod.msg.headers.empty ())
    //mod does not change headers
    return;
  for (Recipient& r : recipients)
    if (mod != *r.modules[i])
      return; //module is not global

  //move headers from global modules to global space
  for (std::tuple<std::string, std::string, uint8_t>& h : mod.msg.headers) {
    if (std::get<2>(h) & 0xA0 /* 1xxxxxxx */ ) { //delete headers
      smfi_chgheader (ctx, const_cast<char*>(std::get<0>(h).c_str()), std::get<2>(h) & 0x8F, (char*)"");  //delete header via libmilter
    } else {  //std::get<2>(h) has the form 0xxxxxxx => add module
      if (std::get<2>(h) == 0) {//add to the end
	smfi_addheader (ctx, const_cast<char*>(std::get<0>(h).c_str()), const_cast<char*>(std::get<1>(h).c_str()));
      } else {
	smfi_addheader (ctx, const_cast<char*>(std::get<0>(h).c_str()), const_cast<char*>(std::get<1>(h).c_str())); //FIXME
      }
    } //end add/delete headers
  }
  mod.msg.headers.clear();
}

//take a long line as SMTP-reply and split it in smaller lines.
int Privdata::InjectResponse(const std::string& code, const std::string& dsn,
			     const std::string& reason) {
  if (code[0] == '2' || reason.empty()) return 0;
  std::string reason_ (reason);
  for (char& m : reason_)
    if (m == '\t' || m == '%') m = ' ';
  char* p[33];
  p[0] = strchr (const_cast<char*>(reason_.c_str()), '\n');
  if ( p[0] == NULL )
    return smfi_setreply (ctx, const_cast<char*>(code.c_str()),
			  const_cast<char*>(dsn.c_str()),
			  const_cast<char*>(reason_.c_str()));
  int m = 0;
  //MAX length per reply line 512 octets
  while (p[m] && m < 32) {
    p[m][ (p[m][-1] == '\r') ? -1 : 0] = '\0';
    p[m] += 1;
    p[m+1] = strchr (p[m], '\n');
    m++;
  };
  m--;
  while ((m >= 0) && (p[m][0] == '\0'))
	 p[m--] = NULL;
  return  smfi_setmlreply (ctx, code.c_str(), dsn.c_str(), reason_.c_str(),
			  p[ 0], p[ 1], p[ 2], p[ 3], p[ 4], p[ 5], p[ 6], 
                          p[ 7], p[ 8], p[ 9], p[10], p[11], p[12], p[13], 
                          p[14], p[15], p[16], p[17], p[18], p[19], p[20], 
                          p[21], p[22], p[23], p[24], p[25], p[26], p[27], 
                          p[28], p[29], p[30], NULL);
}

int Privdata::SetResponses() {
  //proceed global modules
  //add global headers
  for (unsigned int n = 0; n < AegeeMilter::so_modules.size (); n++)
    CompactHeaders (n);
  //add global recipients
  //proceed private modules
  //add private headers
  //add private recipients
  for (Recipient& rec : recipients) {
    current_recipient = &rec;
    for (unsigned int n = 0; n < AegeeMilter::so_modules.size(); n++) {
      current_soModule = n;
      const std::vector<std::string>& recipients = GetRecipients ();
      if (recipients.empty ()) //the current recipient is deleted
	smfi_delrcpt (ctx, const_cast<char*>(current_recipient->address.c_str ()));
      else {
	if (g_ascii_strcasecmp (recipients.front ().c_str (),
				current_recipient->address.c_str ()))
	  smfi_delrcpt(ctx, const_cast<char*>(current_recipient->address.c_str ()));
	for (const std::string& recip : recipients)
	  smfi_addrcpt (ctx, const_cast<char*>(recip.c_str ()));
      }
    }
  }

  //check if all recipients agree on the same answer

  //fill in answers-list per recipient, which module has not accepted her
  std::vector<const Module*> answers;
  unsigned int nulls = 0;
  for (Recipient& rec : recipients)
    if (std::find_if (rec.modules.begin (), rec.modules.end (),
		      [&answers] (const Module* mod) {
	  if (mod->smfi_const != SMFIS_CONTINUE) {
	    answers.push_back (mod);
	    return true;
	  } else return false;
		      }) == rec.modules.end ()) {
      answers.push_back (nullptr);
      nulls++;
    }

  if (nulls == answers.size ()) { //all modules accept the message
    ClearModules();
    return SMFIS_CONTINUE;
  };

  if (nulls == 0) {
    //every recipient thinks this message must be rejected
    const Module& it = *answers.front();
    InjectResponse (it.return_code, it.return_dsn, it.return_reason);
    //FIXME : all responses from all modules must be included for proper PRDR response
    ClearModules ();
    return SMFIS_REJECT;
  } else { //modules disagree on total acceptance/rejection
    //check what the first recipient thinks
    for (const Module* answer: answers)
      if (answer)
	InjectResponse (answer->return_code, answer->return_dsn,
			answer->return_reason);
      else
	InjectResponse ("250", "2.1.5", "Message accepted");
    const char ret_code = answers.front()->return_code[0];
    int i = (ret_code == '4') ? SMFIS_TEMPFAIL :
      (ret_code == '5') ? SMFIS_REJECT : SMFIS_CONTINUE;
    ClearModules ();
    return i;
  }
}

//throws >0 - not failed & REJECTED, nothrow - not failed & ACCEPTED, returns false if some module fails and needs more data/stages, true if no module fail & message is accepted

//FIXME: What happens if a module was executed for another, equivalent recipient in the past and has failed?
bool Privdata::operator ()(Recipient* param) {
  current_recipient = param;
  for (unsigned int i = 0; i < AegeeMilter::so_modules.size(); i++) {
    current_soModule = i;
    current_module = current_recipient->modules[i];
    if (AegeeMilter::so_modules[i]->Status (*this) & stage
	&& (current_module->flags & stage) == 0) {
	//module is subject to execution at this stage
	current_module->flags |= stage;
	current_module->flags &= !MOD_FAILED;
	if (stage == MOD_BODY) smfi_progress (ctx);
	if (!AegeeMilter::so_modules[i]->Run (*this)
	    || current_module->flags & MOD_FAILED) {
          //module was executed with error
	  current_module->return_reason.clear ();
	  current_module->return_code.clear ();
	  current_module->return_dsn.clear ();
	  current_module->smfi_const = SMFIS_CONTINUE;
	  //modified recipients
	  current_module->msg.envrcpts.clear ();
	  //modified headers from
	  //priv->current_recipient->current_soModule->msg->headers
	  return false;
	};
	if (current_module->smfi_const != SMFIS_CONTINUE)
	  throw (int)i;
    }
  }
  return true;
}
