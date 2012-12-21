#include <glib.h>
#include <glib/gstdio.h>
#include "config.h"
#include "src/prdr-milter.h"

extern unsigned int num_so_modules;
extern struct so_module **so_modules;
extern char* sendmail;

inline void
prdr_do_fail (struct privdata* const priv)
{
  priv->current_recipient->current_module->flags |= MOD_FAILED;
}

int
prdr_has_failed (struct privdata *const priv) {
  return (priv->current_recipient->current_module->flags & MOD_FAILED)
    == MOD_FAILED;
}

//-----------------------------------------------------------------------------
//j == 0 -- default, j==1 -- active, j == 2 disabled
//j == 3 not necessary to execute
void
prdr_set_activity (struct privdata* const priv,
		   const char* const mod_name,
		   const gboolean j)
{
  unsigned int i;
  if (mod_name) {
    for (i = 0; i < num_so_modules; i++)
      if (g_ascii_strcasecmp (so_modules[i]->name, mod_name) == 0) {
	priv->current_recipient->activity |= j << i*2;
	unsigned int k;
	for (k = 0; k < priv->recipients->len; k++) {
	  struct recipient *rec = g_ptr_array_index (priv->recipients, k);
          if (rec->modules[i] == priv->current_recipient->modules[i])
	    rec->activity |= j << i*2;
	}
	return;
      }
  }
}
//-----------------------------------------------------------------------------
//0 -default, 1 active, 2 disabled, -1 not loaded
gboolean
prdr_get_activity (const struct privdata* const priv,
		   const char* const mod_name)
{
  unsigned int i;
  if (mod_name)
    for (i = 0; i < num_so_modules; i++)
      if (g_ascii_strcasecmp (so_modules[i]->name, mod_name) == 0)
	return (priv->current_recipient->activity & (3 << (i*2))) >> i*2;
  return -1;
}

//-----------------------------------------------------------------------------
int
prdr_get_stage (const struct privdata* const priv)
{
  return priv->stage;
}

//-----------------------------------------------------------------------------
char*
prdr_get_recipient (const struct privdata* const priv)
{
  return priv->current_recipient->address;
}

//-----------------------------------------------------------------------------
char**
prdr_get_recipients (const struct privdata* const priv)
{
  unsigned int j;
  int c = 0, k, i = 0;
  for (j = 0; j < num_so_modules; j++) {
    if ((priv->current_recipient->modules[j]->msg->envrcpts != NULL)
        && (prdr_get_activity (priv, so_modules[j]->name) == 0) //module is active
        && ((priv->current_recipient->flags & MOD_FAILED) == 0)) {
      k = 0;
      while (priv->current_recipient->modules[j]->msg->envrcpts[k++])
	c++;
    }
    if (priv->current_recipient->modules[j]->msg->deletemyself == 0
	&& i == 0)
      c++;
    else
      i = 1;
    if (priv->current_recipient->modules[j]
	== priv->current_recipient->current_module)
      break;
  }
  char **ret = g_malloc((c+2-i) * sizeof(char*));
  c = 0;
  if (i==0)
    ret[c++] = priv->current_recipient->address;
  for (j = 0; j < num_so_modules; j++) {
    if ((priv->current_recipient->modules[j]->msg->envrcpts != NULL) &&
	(prdr_get_activity (priv, so_modules[j]->name) == 0) && //module is active
	((priv->current_recipient->flags & MOD_FAILED) == 0)) {
      k = 0;
      while (priv->current_recipient->modules[j]->msg->envrcpts[k++] != NULL)
	if ((priv->current_recipient->modules[j]->flags & MOD_FAILED) == 0)
	  ret[c++] = priv->current_recipient->modules[j]->msg->envrcpts[k-1];
      if (priv->current_recipient->modules[j]
	  == priv->current_recipient->current_module)
	break;
    }
  }
  ret[c] = NULL;
  return ret;
}

//-----------------------------------------------------------------------------
void
prdr_add_recipient (struct privdata* const priv, const char* const address)
{
  if (address == NULL) return;
  char** x = priv->current_recipient->current_module->msg->envrcpts;
  //x[0] is indication for the original recipient
  if (g_ascii_strcasecmp (address, priv->current_recipient->address) == 0) {
    priv->current_recipient->current_module->msg->deletemyself = 0;
  } else if (x == NULL) {
    x = g_malloc (2 * sizeof (char*));
    x[0] = g_string_chunk_insert (priv->gstr, address);
    x[1] = NULL;
    priv->current_recipient->current_module->msg->envrcpts = x;
  } else {
    int i=0;
    //check if the address is not already included
    while (x[i])
      if (g_ascii_strcasecmp (x[i++], address) == 0) return;//it is already included
    //add the recipient at the end
    x = (char**)g_realloc (x, (2+i) * sizeof (char*));
    x[i] = g_string_chunk_insert (priv->gstr, address);
    x[i+1] = NULL;
    priv->current_recipient->current_module->msg->envrcpts = x;
  }
}

//-----------------------------------------------------------------------------
void
prdr_del_recipient (struct privdata* const priv,
		    const char* const address)
{
  if (address == NULL) return;
  if (g_ascii_strcasecmp (address, priv->current_recipient->address) == 0) {
    priv->current_recipient->current_module->msg->deletemyself = 1;
  } else {
    char** x = priv->current_recipient->current_module->msg->envrcpts;
    int i = 0;
    while(x[i])
      if (g_ascii_strcasecmp (x[i++], address) == 0) {
	//delete the recipient at index i-1, move the last reciepient here
	int j = i;
	while (x[j]) j++;
	//x[j-1] is now the last reciepient
	if (i != j)  x[i-1] = x[j-1];
	x[j-1] = NULL;
	break;
      }
  }
};


//-----------------------------------------------------------------------------
const char**
prdr_get_header (struct privdata* const priv, const char* const headerfield)
{
  //g_printf("***prdr_get_header, %s***\n", headerfield);
  if (priv->stage < MOD_HEADERS) {
    prdr_do_fail (priv);
    //g_printf("---prdr_get_header called in ENVELOPE---\n");
    return NULL;
  }
  //check headers of global modules to global space
  unsigned int i, j;
  j = priv->msg->headers->len;
  for (i = 0 ; i < num_so_modules; i++) {
    if (priv->current_recipient->modules[i]->msg->headers != NULL) 
      j+= priv->current_recipient->modules[i]->msg->headers->len;
    if ( priv->current_recipient->current_module == priv->current_recipient->modules[i])
      break;
  }
  i = 0;
  const char **h = g_malloc (sizeof (char*) * j);
  for (j = 0; j < priv->msg->headers->len; j++) {
    struct header *h1 =
      (struct header*) g_ptr_array_index (priv->msg->headers, j);
    if (g_ascii_strcasecmp (h1->field, headerfield) == 0)
      h[i++] = h1->value;
  }
  if (i == 0) {
    g_free (h);
    return NULL;
  }
  h[i] = NULL;
  //g_printf("---prdr_get_header, h=%p---\n", h);
  return h;
};
//-----------------------------------------------------------------------------
void
prdr_add_header (struct privdata* const priv,
		 const unsigned char index,
		 const char* const field,
		 const char* const value)
{
  struct header *h;
  if (priv->current_recipient->current_module->msg->headers == NULL)
    priv->current_recipient->current_module->msg->headers = g_ptr_array_new ();
  else {
    unsigned int k;
    for (k = 0;
	 k < priv->current_recipient->current_module->msg->headers->len; k++) {
      h = g_ptr_array_index (priv->current_recipient->current_module->msg->headers, k);
      if (strcmp (h->field, field) == 0 && strcmp (h->value, value) == 0)
	return;
    }
  }
  h = g_malloc (sizeof (struct header));
  h->field = prdr_add_string (priv, field);
  h->value = prdr_add_string (priv, value);
  h->status = index & 0x8F /*0111111b */;
  g_ptr_array_add (priv->current_recipient->current_module->msg->headers, h);
  //g_printf("prdr_add_header: headers nr: %i\r\n", priv->current_recipient->current_module->msg->headers->len);
};

//-----------------------------------------------------------------------------
void
prdr_del_header (struct privdata* const priv,
		 const unsigned char index,
		 const char* const field)
{//last = 1, at the end, otherwise at the beginning
  struct header* h = g_malloc (sizeof(struct header));
  h->field = prdr_add_string (priv, field);
  h->status = (index & 0x8F) | 0xA0;
  if (priv->current_recipient->current_module->msg->headers == NULL)
    priv->current_recipient->current_module->msg->headers = g_ptr_array_new();
  g_ptr_array_add (priv->current_recipient->current_module->msg->headers, h);
};

//-----------------------------------------------------------------------------
void
prdr_set_response (struct privdata *priv,
		   const char *code /*5xx*/,
		   const char *dsn /*1.2.3*/,
		   const char *reason)
{
  priv->current_recipient->current_module->return_reason =
    reason ? prdr_add_string(priv, reason) : NULL;
  priv->current_recipient->current_module->return_code =
    code ? prdr_add_string(priv, code) : NULL;
  priv->current_recipient->current_module->return_dsn =
    dsn ? prdr_add_string(priv, dsn) : NULL;
  switch (*code) {
  case '4': 
    priv->current_recipient->current_module->smfi_const = SMFIS_TEMPFAIL;
    //prdr_del_recipient(priv, prdr_get_recipient(priv));
    return;
  case '5':
    priv->current_recipient->current_module->smfi_const = SMFIS_REJECT;
    //prdr_del_recipient(priv, prdr_get_recipient(priv));
    return;
  case '2':
  default:
    priv->current_recipient->current_module->smfi_const = SMFIS_CONTINUE;
  };
};
//-----------------------------------------------------------------------------

GPtrArray*
prdr_get_headers (const struct privdata* const priv)
{
  return priv->msg->headers;
}
//-----------------------------------------------------------------------------

int
prdr_get_size (struct privdata* const priv)
{
  //g_printf("***prdr_get_size STAGE=%i***\n", priv->stage);
  if (priv->stage != MOD_BODY) {
    if (priv->size == 0) {
      prdr_do_fail (priv);
      return 0;
    } else
      return priv->size;
  }
  //calculate size of headers
  GPtrArray* headers = prdr_get_headers (priv);
  unsigned int j, size = 0;
  for (j = 0; j < headers->len; j++) {
    struct header * h = g_ptr_array_index (headers, j);
    size += strlen (h->field) + strlen (h->value);
  };
  size += 3*j + 2; // add the ":" header : field - delimiter, and \r\n at the end of each header, and \r\n after the last header
  const GString *temp = prdr_get_body (priv);
  //g_printf("---prdr_get_size B =%i---\n", temp->len);
  return size + temp->len;
}
//-----------------------------------------------------------------------------

void
prdr_set_envsender (struct privdata* const priv, const char* const env)
{
  if (priv->stage == MOD_RCPT || priv->stage == MOD_MAIL)
    prdr_do_fail (priv);
  else
    priv->current_recipient->current_module->msg->envfrom =
      prdr_add_string (priv, env);
};

//-----------------------------------------------------------------------------
char*
prdr_get_envsender (const struct privdata* const priv)
{
  if (priv->stage == MOD_EHLO) return NULL;
  unsigned int j;
  for (j = 0; j < num_so_modules; j++) 
    if (priv->current_recipient->current_module
	== priv->current_recipient->modules[j])
      break;
  int i = j;
  while (i-- >= 0)
    if ((priv->current_recipient->modules[i+1]->msg->envfrom != NULL) &&
	(prdr_get_activity (priv,
			    priv->current_recipient->modules[i+1]->so_mod->name) != 2) // check activity
	&& ((priv->current_recipient->modules[i+1]->flags & MOD_FAILED) ==0))
      return priv->current_recipient->modules[i+1]->msg->envfrom;
  return priv->msg->envfrom;
}

//-----------------------------------------------------------------------------
const GString*
prdr_get_body (struct privdata* const priv)
{
  if (priv->stage != MOD_BODY) {
    prdr_do_fail (priv);
    return NULL;;
  }
  //g_printf("***prdr_get_body***\n");
  unsigned int j;
  for (j = 0; j < num_so_modules; j++) {
    if (priv->current_recipient->modules[j]
	== priv->current_recipient->current_module)
      break;
  }
  int i = j;
  while (i > 0)
    if ((priv->current_recipient->modules[i--]->msg->body != NULL) &&
	(prdr_get_activity (priv, so_modules[i+1]->name) != 2) &&
	((priv->current_recipient->modules[i+1]->flags & MOD_FAILED) == 0))
      return priv->current_recipient->modules[i+1]->msg->body;
  return priv->msg->body;
  //g_printf("---prdr_get_body---\n");
};

//-----------------------------------------------------------------------------
void
prdr_set_body (struct privdata* const priv, const GString* const body)
{
  //g_printf("***prdr_set_body***\n");
	if ( priv->current_recipient->current_module->msg->body )
	  g_string_free (priv->current_recipient->current_module->msg->body,
			 TRUE);
	priv->current_recipient->current_module->msg->body =
	  g_string_new_len (body->str, body->len);
  //g_printf("---prdr_set_body---\n");
};

//-----------------------------------------------------------------------------
void*
prdr_get_priv_rcpt (const struct privdata* const priv)
{
  return priv->current_recipient->current_module->private_;
};

//-----------------------------------------------------------------------------
void
prdr_set_priv_rcpt (struct privdata* const priv, void* const user)
{
  priv->current_recipient->current_module->private_ = user;
};

//-----------------------------------------------------------------------------
void*
prdr_get_priv_msg (const struct privdata* const priv)
{
  if (priv->stage == MOD_MAIL || priv->stage == MOD_EHLO)
    return priv->msgpriv[priv->size];
  unsigned int j;
  for (j = 0; j < num_so_modules; j++)
    if (priv->current_recipient->modules[j] ==
	priv->current_recipient->current_module) {
      return priv->msgpriv[j];
    }
  return NULL;
}

//-----------------------------------------------------------------------------
void
prdr_set_priv_msg (struct privdata* const priv, void* const user)
{
  if (priv->stage == MOD_MAIL || priv->stage == MOD_EHLO)
    priv->msgpriv[priv->size] = user;
  else {
    unsigned int j;
    for (j = 0; j < num_so_modules; j++)
      if (priv->current_recipient->modules[j] ==
	  priv->current_recipient->current_module) {
	//g_printf ("   %p get_priv_msg %p module %i\n", priv, &( priv->msgpriv[j]), j);
	priv->msgpriv[j] = user;
	break;
      }
  }
}

//-----------------------------------------------------------------------------
inline char*
prdr_add_string (struct privdata* const priv, const char* const string)
{
  return g_string_chunk_insert (priv->gstr, string);
}

char* get_date ()
{
  static const char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

  static const char *wday[] = { "Sun", "Mon", "Tue", "Wed",
				"Thu", "Fri", "Sat" };
  time_t _time = time (NULL);
  struct tm lt;
  long gmtoff = -0;
  int gmtneg = 0;
  localtime_r (&_time, &lt);
  char *date = g_malloc (64);
  g_sprintf(date, "%s, %02d %s %4d %02d:%02d:%02d %c%.2lu%.2lu",
    	  wday[lt.tm_wday], lt.tm_mday, month[lt.tm_mon],
	  lt.tm_year + 1900, lt.tm_hour, lt.tm_min, lt.tm_sec,
	  gmtneg ? '-' : '+', gmtoff / 60, gmtoff % 60);
  return date;
}

//-----------------------------------------------------------------------------
//return values: != 0 is error, == 0 is OK
int
prdr_sendmail (const char* const from,
	       const char* const rcpt[],
	       const char* const body,
	       const char* const date,
	       const char* const autosubmitted)
{
  int length = (from ? strlen (from) : 4) + strlen (sendmail) + 4;
  int i;
  for (i = 0; rcpt[i]; i++)
    length += strlen (rcpt[i]) + 1;
  char *cmdline = g_malloc (length);
  cmdline[0] = '\0';
  char *temp = g_stpcpy (cmdline, sendmail);
  temp = g_stpcpy (temp, " -f");//replace with g_strcpy
  if (from)
    temp = g_stpcpy (temp, from);//check if "" needed
  else
    temp = g_stpcpy (temp, "\\<\\>");
  i = 0;
  while (rcpt[i]) {
    temp[0] = ' ';
    temp++;
    temp[0] = '\0';
    temp = g_stpcpy (temp, rcpt[i++]);
  }
  FILE *sm = popen (cmdline, "w");
  g_free (cmdline);
  if (sm == NULL) return -1;
  char *date_ = get_date ();
  g_fprintf (sm, "%s: %s\r\n", date, date_);
  g_free (date_);
  if (autosubmitted)
    g_fprintf (sm, "Auto-Submitted: %s\r\n", autosubmitted);
  g_fprintf (sm, "%s", body);
  //g_printf("---prdr_sendmail---\n");
  return pclose (sm);
}
