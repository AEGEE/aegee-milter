extern "C" {
#include <libmilter/mfapi.h>
}

#include "src/objects/SoList.hpp"
#include "src/objects/SoModule.hpp"

struct message {
  const char* envfrom;
  char deletemyself;
  char** envrcpts;
  //  struct header **headers;
  GPtrArray* headers;
  GString* body;
};

struct privdata
{
  struct message *msg;
  char *ehlohost;
  char *hostname;
  char *queue_id, *domain_name;
  struct sockaddr *hostaddr;
  GPtrArray* recipients;
  struct recipient* current_recipient;
  unsigned char prdr_supported;//logically ORed MSG_ flags
  unsigned char mime8bit;
  unsigned int stage;
  GSList *module_pool;
  void** msgpriv;
  SMFICTX *ctx;
  unsigned int size;
  GStringChunk *gstr;
};

struct module {
  SoModule *so_mod;
  void *private_;
  unsigned char flags;
  //see MOD_... flags
  struct message *msg;
  char* return_reason;
  char* return_code;
  char* return_dsn;
  int smfi_const;
};

struct recipient {
  const char* address;
  guint64 activity;
  struct module** modules;
  struct module* current_module;
  long flags;//binary ORed MOD_ flags or
  //RCPT_PRDR_REJECTED - if the recipient was rejected because of laking PRDR support
  //RCPT_NOTIFY_NEVER
};

//int prdr_stage;
void clear_ehlo (struct privdata *priv);
void clear_message (struct message *msg);
void clear_recipient (struct recipient *rcp);
void clear_recipients (struct privdata *priv);
const char* normalize_email (struct privdata* priv, const char *email);
int apply_modules (struct privdata* priv);
int inject_response (SMFICTX *ctx, const char* const code, const char* const dsn, const char* const reason);
sfsistat set_responses (struct privdata* priv);
void clear_privdata (struct privdata* priv);
void clear_module_pool (struct privdata* priv);
