#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <time.h>
#include "src/prdr-list.h"
extern GKeyFile* prdr_inifile;
extern char* prdr_section;

static FILE* file = NULL;
//table == log, user == , key == module, value ==message
void
list_log_LTX_prdr_list_insert (const char* const table UNUSED,
			       const char* const user,
			       const char* const key,
			       const char* const value,
			       const unsigned int unused UNUSED)
{
  if (file) {
    time_t _time = time (NULL);
    struct tm lt;
    localtime_r (&_time, &lt);
    //    flockfile(file);
    g_fprintf (file, "%04d/%02d/%02d %02d:%02d:%02d %s, %s, %s\n", 1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, user, key, value);
    fflush (file);
    //    funlockfile(file);
  }
}

int
list_log_LTX_load ()
{
  char **array = g_key_file_get_keys(prdr_inifile, prdr_section, NULL, NULL);
  char* filename = NULL;
  if (array) {
    int i = 0;
    while (array[i])
      if ( strcmp(array[i++], "file") == 0)
	filename = g_key_file_get_string (prdr_inifile, prdr_section,
					  "file", NULL); else {
	g_printf ("option %s in section [%s] is not recognized\n",
		array[i-1], prdr_section);
	g_strfreev (array);
	return -1;
      }
    g_strfreev (array);
  }
  if (filename) {
    file = g_fopen (filename, "a");
    g_free (filename);
    if (file) {
      g_fprintf (file, "Timestamp           User     Module     Message\n");
      g_fprintf (file, "================================================================\n");

      time_t _time = time(NULL);
      struct tm lt;
      localtime_r (&_time, &lt);
      g_fprintf (file,
	       "%04d/%02d/%02d %02d:%02d:%02d          mod_log    Started\n",
	       1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday,
	       lt.tm_hour, lt.tm_min, lt.tm_sec);
      fflush (file);
    } else 
      g_printf ("Open %s failed\n", filename);
  }
  return 0;
}

void
list_log_LTX_unload ()
{
  if (file) {

    time_t _time = time(NULL);
    struct tm lt;
    localtime_r (&_time, &lt);
    g_fprintf (file, "%04d/%02d/%02d %02d:%02d:%02d          mod_log    Terminating\n", 1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    fflush (file);
    fclose (file);
  }
  g_printf ("list_log unloaded\r\n");
}

static char * exported_table[] = {"log", NULL};

char**
list_log_LTX_prdr_list_tables ()
{
  return exported_table;
}
