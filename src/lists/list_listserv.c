#include <glib.h>
#include <glib/gstdio.h>
#include <liblistserv.h>
#include <string.h>

#define LISTSERV_HOME "/home/listserv"

GKeyFile* prdr_inifile;
char *prdr_section;

static char *email = NULL;
static char *host = NULL;
static char *password = NULL;

int
list_listserv_LTX_load ()
{
  char **array = g_key_file_get_keys (prdr_inifile, prdr_section, NULL, NULL);
  if (array) {
    int i = 0;
    while (array[i])
      if (strcmp (array[i++], "email") == 0)
	email = g_key_file_get_string (prdr_inifile, prdr_section,
				       "email", NULL); else
      if (strcmp (array[i-1], "host") == 0)
	host = g_key_file_get_string (prdr_inifile, prdr_section,
			      "host", NULL); else
      if (strcmp (array[i-1], "password") == 0)
	password = g_key_file_get_string (prdr_inifile, prdr_section
					  , "password", NULL); else {
	g_printf ("option %s in section [%s] is not recognized\n",
		array[i-1], prdr_section);
	g_strfreev (array);
	return -1;
      }
    g_strfreev (array);
  };
  struct listserv *l = listserv_init (email, password, host);
  if (l == NULL)
    g_printf ("cannot connect to listserv on host %s with email %s and password %s\n", host, email, password);
  else 
    listserv_destroy (l);
  return 0;
}

#define add_email(STRING, LIST)	     				\
  temp1 = g_ascii_strdown (STRING, -1);				\
  temp = g_strconcat (temp1, ",", NULL);			\
  if (g_strstr_len (LIST->str, -1, temp) == NULL)		\
      g_string_append_printf (LIST, "%s", temp);  		\
  g_free (temp); g_free (temp1);

static void
extract_emails_from (struct listserv* const l,
		     const char* const listname,
		     const char* const keyword,
		     GString *emails,
		     GString *loop)
{
  char *temp, *temp1, *temp2;
  unsigned int i = loop->len;
  temp2 = g_malloc (strlen (listname) + strlen (keyword) + 2);
  g_sprintf (temp2, "%s|%s", listname, keyword);
  add_email (temp2, loop);
  g_free (temp2);
  if (loop->len == i) return; //LOOP Detected
  if (keyword[0] == '\0') {
    //insert all subscribers
    i = 0;
    struct listserv_subscriber** ls = listserv_getsubscribers (l, listname);
    while (ls[i]) {
      add_email (ls[i]->email, emails);
      i++;
    }
    return ;
  }
  char **kw = listserv_getlist_keyword (l, listname, keyword);
  if (kw) {
    i = 0;
    while (kw[i]) {
      if (strchr (kw[i++], '@'))	{/*we have an email address */
	add_email (kw[i-1], emails);
      } else
	if (strchr (kw[i-1], '(') != NULL) { /* external list */
	  temp2 = g_strdup (kw[i-1]);
	  temp = temp2;
	  while (temp[0] != '(') temp++;
	  if (temp[0] == '(') {
	    temp[0] = '\0';
	    temp++;
	  }
	  temp1 = temp;
	  while (temp[0] && temp[0] != ')') temp++;
	  if (temp[0]) temp[0] = '\0';
	  extract_emails_from (l, temp1, temp2, emails, loop);
	  g_free(temp2);
	  kw = listserv_getlist_keyword (l, listname, keyword);
	}
    }
  }
}
#undef add_email

//if key == NULL -> redirect, return all email addresses
//else check if key is in user
void*
list_listserv_LTX_prdr_list_query (const char* const table,
				   const char* const user /* listname */,
				   const char* const key /* email */)
{
  //return NULL if the element is not found, otherwise the value
  //  g_printf("prdr_list_query table %s, user %s key %s\n", table, user, key);
  if (table == NULL ) return NULL;
  char *listname = g_strdup (user);
  char *keyword = "";
  if (strchr (listname, '|') != NULL) {
    keyword = strchr (listname, '|');
    keyword[0] = '\0';
    keyword++;
  }
  GString *emails = g_string_new ("");
  GString *loop = g_string_new("");
  struct listserv *l = listserv_init (email, password, host);
  extract_emails_from (l, listname, keyword, emails, loop);
  g_free (listname);
  listserv_destroy (l);
  g_string_free (loop, 1);
  if (key == NULL) {
    emails->len--;
    emails->str[emails->len]='\0';
    return (void*)g_string_free (emails, 0);
  }
  int i = 0;
  char *key2 = g_malloc (strlen (key)+1);
  while (key[i]) {
    key2[i] = g_ascii_tolower (key[i]);
    i++;
  }
  key2[i] = '\0';
  if (strstr (emails->str, key2)) {
    g_string_free (emails, 1);
    return g_strdup (key);
  } else {
    g_string_free (emails, 1);
    return NULL;
  }
}

static const char *exported_table[] = { "listserv", NULL };

const char**
list_listserv_LTX_prdr_list_tables ()
{
  return exported_table;
}
