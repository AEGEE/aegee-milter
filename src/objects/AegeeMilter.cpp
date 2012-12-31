#include "src/objects/AegeeMilter.hpp"
extern "C" {
#include <libmilter/mfapi.h>
#include <glib/gstdio.h>
}
extern const lt_dlsymlist lt_preloaded_symbols[];
int bounce_mode;
const char* sendmail;
char *prdr_section;
GKeyFile *prdr_inifile;
static int alarm_period;
std::vector<SoModule*> AegeeMilter::so_modules;
std::vector<SoList*> AegeeMilter::so_lists;
std::map<std::string, SoList*> AegeeMilter::tables;

static void
catch_signal (int sig)
{
  //g_printf("SIGNAL %i received\n", sig);
  //g_printf("Received signal %i\n", sig);
  switch(sig) {
    case SIGALRM:
      prdr_list_expire ();
      alarm (alarm_period * 60 * 60);
      break;
    case SIGTERM:
      g_printf ("SIGTERM\n");
      smfi_stop ();
      AegeeMilter::UnloadPlugins ();
      break;
    case SIGINT:
      g_printf ("SIGINT\n");
      smfi_stop ();
      AegeeMilter::UnloadPlugins ();
      break;
    case SIGHUP:
      g_printf ("SIGHUP\n");
      smfi_stop ();
      AegeeMilter::UnloadPlugins ();
      //proceed_conf_file (filename);
      //smfi_main ();
      break;
  }
  //  signal (sig, catch_signal);//on System V signal has to be called after every signal handling or installed with sysv_signal
}

AegeeMilter::AegeeMilter() {
  lt_dlpreload_default (lt_preloaded_symbols);
}

void AegeeMilter::Fork() {
  if (chdir ("/") < 0)
    g_printf ("chdir(\"/\") in main() failed.\n");
  //fork in backgroud
  /*
  pid_t pid = fork ();
  if (pid < 0)
      g_printf ("fork in main() failed\n");
  if (pid > 0) exit (0);
      umask (0);
  if (setsid () < 0)
    g_printf ("setsid() in main() failed.\n");
  close (STDIN_FILENO);  close (STDOUT_FILENO);  close (STDERR_FILENO);
  */
}

int AegeeMilter::LoadPlugins() {
  lt_dlinit();
  int j = 1;
  GString *g_mods = g_string_new ("Loaded modules:");
  GString *g_lists = g_string_new ("Loaded lists (with tables):");
  while (lt_preloaded_symbols[j].name != NULL) {
    if (lt_preloaded_symbols[j++].address != NULL) continue;
    prdr_section = g_strdup (lt_preloaded_symbols[j-1].name);
    char *x = strrchr (prdr_section, '.');
    x[0] = '\0';
    char ** array = g_key_file_get_keys (prdr_inifile, prdr_section,
					 NULL, NULL);
    if ( array == NULL) {
      // g_printf("SKIP %s\n", lt_preloaded_symbols[j-1].name);
      g_free (prdr_section);
      continue;
    } else g_strfreev (array);
    //g_printf("LOAD %s\n", lt_preloaded_symbols[j-1].name);
    if (lt_preloaded_symbols[j-1].name[0] == 'm') { //load module
      so_modules.insert(so_modules.end(), new SoModule(lt_preloaded_symbols[j-1].name));
      g_string_append_printf (g_mods, "\n  %s", prdr_section);
    } else { //load list
      SoList* soList = new SoList(lt_preloaded_symbols[j-1].name);
      char **so_tables = soList->Tables();
      g_string_append_printf (g_lists, "\n  %s (", prdr_section);
      if (so_tables == NULL) {
        delete soList;
	continue;
	//g_printf ("The list backend \"%s\", does not export prdr_list_tables. The module is useless. aegee-milter exits...\n", lt_preloaded_symbols[j-1].name, filename);
      } else {
        so_lists.insert(so_lists.end(), soList);
	 //load tables
        int j = 0;
	while (so_tables[j]) {
	  g_string_append_printf (g_lists, "%s, ", so_tables[j]);
	  //g_printf ("loading table %s\n", exported_tables[j]);
	  tables[so_tables[j++]] = soList;
	}
      }
      g_string_truncate (g_lists, g_lists->len - 2);
      g_string_append_printf (g_lists, ") ");
    }
    g_free (prdr_section);
  }
  g_printf ("%s\n", g_mods->str);
  g_printf ("%s\n", g_lists->str);
  g_string_free (g_mods, 1);
  g_string_free (g_lists, 1);
  if (so_modules.size() > 31)
    g_printf ("aegee-milter can operate with up to 31 modules, you are trying to load %lu.\n", so_modules.size());
  return 0;
}

int AegeeMilter::ProceedConfFile(const char* filename) {
  UnloadPlugins ();
  prdr_inifile = g_key_file_new ();
  g_key_file_set_list_separator (prdr_inifile, ',');
  GError *err;
  if (!g_key_file_load_from_file (prdr_inifile, filename,
				  G_KEY_FILE_NONE, &err)) {
    g_printf ("Unable to parse file %s, error: %s\n", filename, err->message);
    g_error_free (err);
  }
  //  int unsigned k;
  //  for (k = 0; k < 200; k++)
  //    signal(k, catch_signal);
  //  signal (SIGTERM, catch_signal);
  //  signal (SIGINT, catch_signal);
  //  signal (SIGHUP, catch_signal);  //  1
  signal (SIGALRM, catch_signal); // 14
  //  signal (SIGTERM, catch_signal); // 15

  //section General 
  sendmail = g_strstrip (g_key_file_get_string (prdr_inifile, "General", "sendmail", NULL));
  if (sendmail == NULL)
    sendmail = "/usr/bin/sendmail";
  alarm_period = g_key_file_get_integer (prdr_inifile, "General", "expire",
					 NULL);
  if (alarm_period <0 )
    g_printf ("The expire value in configuration file \"%s\" has to be positive", filename);
  else if (alarm_period == 0)
    alarm_period = 24;
  alarm (alarm_period * 60 * 60);
  //g_printf("Load bounce mode\n");
  //load bounce_mode
  char* temp = g_strstrip(g_key_file_get_string (prdr_inifile, "General",
						 "bounce-mode", NULL));
  if (temp == NULL) {
    //bounce mode not found, assuming pseudo-delayed if "delayed' is available, otherwise delayed
    if (tables.find("delayed") == tables.end())
      bounce_mode = 1;//pseudo-delayed
    else
      bounce_mode = 0;//delayed
  } else {
    if (g_ascii_strcasecmp (temp, "delayed")== 0 ) bounce_mode = 0;
    else if (g_ascii_strcasecmp (temp, "pseudo-delayed")== 0) {
      bounce_mode = 1;
      if (tables.find("delayed") == tables.end())
	g_printf ("bounce-mode set to delayed, but no list called `delayed` was exposed by a list module, in configuration file \"%s\"...\n", filename);
    } else if (g_ascii_strcasecmp (temp, "ndr")== 0) bounce_mode = 2;
    else if (g_ascii_strcasecmp (temp, "no-ndr")== 0) bounce_mode = 3;
    else 
      g_printf ("bounce-mode not set correctly in configuration file \"%s\". %s is invalid value.\n", filename, temp);
    g_free (temp);
  }
  //g_printf("bounce-mode loaded as %i\n", bounce_mode);
  if ( LoadPlugins () == -1)
    return -10;
  //section Milter
  char **array = g_key_file_get_keys (prdr_inifile, "Milter", NULL, NULL);
  int i = 0;
  if (array) {
    int timeout;
    while (array[i]) {
      if (strcmp (array[i], "timeout") == 0) {
	timeout = g_key_file_get_integer (prdr_inifile, "Milter", "timeout", NULL);
	if (timeout)
	  smfi_settimeout (timeout);
      } else 
	if (strcmp(array[i], "socket") == 0) {
	  temp = g_strstrip (g_key_file_get_string (prdr_inifile, "Milter",
						    "socket", NULL));
	  if (smfi_setconn (temp) == MI_FAILURE)
	    g_printf ("Connection to '%s' could not be established, as specified in configuration file \"%s\".\n", temp, filename);
	  if (smfi_opensocket (1) == MI_FAILURE)
	    g_printf ("Socket %s specified in configuration file %s could not be opened\n", temp, filename);
	  g_free (temp);
	} else
	  g_printf ("Keyword %s has no place in secion [Milter] of %s\n", array[i], filename);
      i++;
    }
    g_strfreev (array);
  }
  if ((temp = g_strstrip (g_key_file_get_string (prdr_inifile, "General",
						 "pidfile", NULL)))) {
    FILE *stream = g_fopen (temp, "w");
    if (!stream)
      g_printf ("Couldn't open pid file %s.\n", temp);
    g_fprintf (stream, "%i\n", getpid ());
    fclose (stream);
    g_free (temp);
  }
  g_key_file_free (prdr_inifile);
  return 0;
}

void AegeeMilter::UnloadPlugins() {
  if (!so_modules.empty ())
    for (std::vector<SoModule*>::iterator it = so_modules.begin ();
	it != so_modules.end (); it++)
	delete *it;
  so_modules.clear ();

  if (!AegeeMilter::so_lists.empty ())
    for (std::vector<SoList*>::iterator it = so_lists.begin ();
	 it != so_lists.end (); it++)
	delete *it;
  so_lists.clear ();

  tables.clear ();

  lt_dlexit ();
}

AegeeMilter::~AegeeMilter() {
}
