//logic: first open default.siv.script.  If it is not available, open the activated script

#include <glib.h>
#include <string.h>
#define SIEVEDIR "/usr/sieve"
#define VIRTDOMAINS 0
#include <glib/gstdio.h>
#include <unistd.h>
#include <sys/stat.h>
#define __USE_GNU
#include <stdlib.h>


extern GKeyFile *prdr_inifile;
static char *sievedir;
extern char *prdr_section;

int
list_timsieved_scripts_LTX_load ()
{
  char **array = g_key_file_get_keys (prdr_inifile, prdr_section, NULL, NULL);
  sievedir = SIEVEDIR;
  if (array) {
    int i = 0;
    while (array[i])
      if (strcmp (array[i++], "sievedir") == 0) 
	sievedir = g_key_file_get_string (prdr_inifile, prdr_section,
					  "sievedir", NULL);
      else {
	g_printf ("option %s in section [%s] is not recognized\n", array[i-1],
		  prdr_section);
	g_strfreev (array);
	return -1;
      }
    g_strfreev (array);
  }
  return 0;
}


//this function is not multidomain capable
//it eats the first @ in the user and everything after it
//correct it to reflect hashimapspool and fulldirhash
void*
list_timsieved_scripts_LTX_prdr_list_query (const char* const table,
					    const char* user,
					    const char* key)
{
  //return NULL if the element is not found, otherwise the value
  if (table == NULL) return NULL;
  int format = 0;// 1 - script, 2 - bytecode
  if (strcmp (table, "sieve_scripts") == 0) format = 1;
  else if (strcmp (table, "sieve_bytecode") == 0) format = 2;
  if (table == NULL || strcmp (table, "sieve_scripts")) return NULL;
  GString *sieve_dir = g_string_new (sievedir);
  if (user == NULL) user = "";
  if (key == NULL) key = "";
  if (*user == '\0' || strcmp (":global", user) == 0) {
    g_string_append_printf (sieve_dir, "/global/");
  } else {
    char *strbuf, *user2 = g_strdup (user);//this will be an user, that has no domain
    if ((strbuf = strchr (user2, '@'))) strbuf[0] = '\0';
    if ((strbuf = strchr (user2, '+'))) strbuf[0] = '\0';
    g_string_append_printf (sieve_dir, "/%c/%s/", user2[0], user2);
    g_free (user2);
  };
  FILE *f = NULL;
  if (*key == '\0') {
    g_string_append_printf (sieve_dir, "default.siv.%s",
			    format == 1 ? "script": "bc");
    f = g_fopen (sieve_dir->str, "r");
    if (!f) {
      //cut default.siv.script, add defaultbc <=> cut .siv.script add bc
      g_string_truncate (sieve_dir,
			 sieve_dir->len - ((format == 1)?11:7));
      g_string_append (sieve_dir, "bc");

      char* temp = (char*)canonicalize_file_name (sieve_dir->str);
      if (temp == NULL) {
	g_string_free (sieve_dir, TRUE);
	return NULL;//no default script at all
      }
      else {
	g_string_assign (sieve_dir, temp);
	g_free (temp);
      }
      g_string_truncate (sieve_dir, sieve_dir->len -3);
      g_string_append_printf (sieve_dir, ".%s", format == 1 ? "script" : "bc");
      f = g_fopen (sieve_dir->str, "r");
    };
  } else {
    g_string_append (sieve_dir, key);
    g_string_append_printf (sieve_dir, ".%s", format == 1 ? "script" : "bc");
    f = g_fopen (sieve_dir->str, "r");
  }
  g_string_free (sieve_dir, TRUE);
  if (!f) return NULL;
  struct stat sb;
  int filenum = fileno (f);
  fstat (filenum, &sb);
  gchar* script_content = g_malloc (sb.st_size + 1);
  read (filenum, script_content, sb.st_size);
  script_content[sb.st_size] = '\0';
  fclose (f);
  //  g_printf("X[%s:%s]\n", user, key);
  return script_content;
  // return g_string_free (sieve_dir, FALSE);
}

const char**
list_timsieved_scripts_LTX_prdr_list_tables ()
{
  static const char *exported_table[] = {"sieve_scripts", "sieve_bytecode", NULL};
  return exported_table;
}
