#ifndef PRDR_MILTER_H
#define PRDR_MILTER_H 1

//include <stdio.h>
#include <libmilter/mfapi.h>
#include <glib.h>
#include <ltdl.h>
#include <unistd.h>
#include <string.h>
G_BEGIN_DECLS
#include "src/prdr-list.h"

struct header{
  char *field, *value;
  //bit mask for status
  //00000000 - nothing/default
  //0xxxxxxx - added at position xxxxxxx
  //10xxxxxx - deleted from position xxxxxx from the begin
  //11xxxxxx - deleted from position xxxxxx from the end
  unsigned char status;  
};

struct message {
  const char* envfrom;
  char deletemyself;
  char** envrcpts;
  //  struct header **headers;
  GPtrArray* headers;
  GString* body;
};

struct so_list {
  lt_dlhandle mod;
  int   (*expire) ();
  char* (*query) (const char*, const char *, const char*);
  int   (*remove) (const char*, const char*, const char*);
  int   (*insert) (const char*, const char*, const char*, const void*, const unsigned int);
};

struct list {
  struct so_list *module;
  char *name;
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

struct so_module {
  lt_dlhandle mod;
  const char* name;
  int (*run) (struct privdata*);
  int (*status) (struct privdata*);
  int (*init_msg) (struct privdata*);
  int (*init_rcpt) (struct privdata*);
  int (*destroy_msg) (struct privdata*);
  int (*destroy_rcpt) (struct privdata*);
  int (*equal) (struct privdata*, const char*, const char*);
};

struct module {
  struct so_module *so_mod;
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

//MODULE FLAGS
#define MOD_EHLO     1
#define MOD_MAIL     2
#define MOD_RCPT     4
#define MOD_HEADERS  8
#define MOD_BODY     16
#define MOD_FAILED   32
#define RCPT_PRDR_REJECTED 64
#define RCPT_NOTIFY_NEVER  128

//functions that can be used in the prdr modules, apart from the ones included in prdr-list.h
//0 -inactive, 1 - active
void		prdr_set_activity	(struct privdata* priv, const char* const mod_name, gboolean i);
gboolean	prdr_get_activity	(const struct privdata* priv, const char* const mod_name);
int		prdr_get_stage		(const struct privdata* priv);
const char*	prdr_get_recipient	(const struct privdata* priv);
const char**	prdr_get_recipients	(const struct privdata* priv);
void		prdr_add_recipient	(struct privdata* priv, const char* address);
void		prdr_del_recipient	(struct privdata* priv,
					 const char* const address);
int		prdr_get_size		(struct privdata* priv);
struct sockaddr* prdr_get_hostaddr	(struct privdata* priv);
void		prdr_do_fail		(struct privdata* priv);
int 		prdr_has_failed		(struct privdata* priv);
const char**	prdr_get_header		(struct privdata* priv,
					 const char* const headerfield);
GPtrArray*	prdr_get_headers	(const struct privdata *const priv);
void		prdr_add_header		(struct privdata *priv,
					 const unsigned char index,
					 const char* const field,
					 const char* const value);
void		prdr_del_header		(struct privdata *priv,
					 const unsigned char index,
					 const char* const field);//last = 1, at the end, otherwise at the beginning
void		prdr_set_response	(struct privdata *priv, const char *code /*5xx*/, const char *dsn /*1.2.3*/, const char *reason);
const char*	prdr_get_envsender	(const struct privdata* priv);
void		prdr_set_envsender	(struct privdata* priv, const char* const env);
const GString*	prdr_get_body		(struct privdata* priv);
void		prdr_set_body		(struct privdata* priv, const GString *body);
void*		prdr_get_priv_rcpt	(const struct privdata *priv); 
void		prdr_set_priv_rcpt	(struct privdata* priv,
					 void* const user);
void*		prdr_get_priv_msg	(const struct privdata* priv);
void		prdr_set_priv_msg	(struct privdata* const priv,
					 void* const user);
char*		get_date ();
int		prdr_sendmail(const char* from,
			      const char* const rcpt[],
			      const char* body,
			      const char* date,
			      const char* autosubmitted);
char*		prdr_add_string		(struct privdata* const priv, const char* string);
void* prdr_get_ctx(struct privdata* priv);
const char* prdr_get_ehlohost(struct privdata* priv);
const char* prdr_get_domain_name(struct privdata* priv);
const char* prdr_get_queue_id(struct privdata* priv);
const struct so_list* prdr_list_is_available (const char *listname);
void clear_ehlo (struct privdata *priv);
void clear_message (struct message *msg);
void clear_recipient (struct recipient *rcp);
void clear_recipients (struct privdata *priv);
const char* normalize_email (struct privdata* priv, const char *email);
int apply_modules (struct privdata* priv);
int inject_response (SMFICTX *ctx, const char* const code, const char* const dsn, const char* const reason);
sfsistat set_responses (struct privdata* priv);
void clear_privdata (struct privdata* const priv);
void clear_module_pool (struct privdata* const priv);
G_END_DECLS
 
#endif /* PRDR_MILTER_H */
