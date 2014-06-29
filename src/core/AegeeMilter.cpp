#include <ctype.h>
#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <gmime/gmime.h>
#include <unistd.h>
#include "src/core/AegeeMilter.hpp"

extern "C" {
  #include <libmilter/mfapi.h>
  #include <ltdl.h>
  #include <glib.h>
  #include "config.h"
}

extern const lt_dlsymlist lt_preloaded_symbols[];
static std::string sendmail;
std::string prdr_section;
GKeyFile *prdr_inifile;
static int alarm_period;

std::vector<std::unique_ptr<SoModule, void(*)(SoModule*)>> AegeeMilter::so_modules;
std::vector<std::unique_ptr<SoList, void(*)(SoList*)>> AegeeMilter::so_lists;
std::map<std::string, SoList*> AegeeMilter::tables;

static void
catch_signal (int sig)
{
  //std::cout << "SIGNAL " << sig << " received." << std::endl;
  //std::cout << "Received signal " << sig << std::endl;
  switch (sig) {
    case SIGALRM:
      std::cout << "SIGALRM" << std::endl;
      for (auto& l: AegeeMilter::so_lists)
	l->Expire();
      alarm (alarm_period * 60 * 60);
      break;
    case SIGTERM:
      std::cout << "SIGTERM" << std::endl;
      smfi_stop ();
      AegeeMilter::UnloadPlugins ();
      g_mime_shutdown ();
      break;
    case SIGINT:
      std::cout << "SIGINT" << std::endl;;
      smfi_stop ();
      AegeeMilter::UnloadPlugins ();
      break;
    case SIGHUP:
      std::cout << "SIGHUP" << std::endl;;
      smfi_stop ();
      AegeeMilter::UnloadPlugins ();
      //proceed_conf_file (filename);
      //smfi_main ();
      break;
  }
  //  signal (sig, catch_signal);//on System V signal has to be called after every signal handling or installed with sysv_signal
}

AegeeMilter::AegeeMilter() {
  g_mime_init (0);
  lt_dlpreload_default (lt_preloaded_symbols);
}

void AegeeMilter::Fork() {
  if (chdir ("/") < 0)
    std::cout << "chdir(\"/\") in main() failed." << std::endl;
  //fork in backgroud
  pid_t pid = fork ();
  if (pid < 0)
    std::cout << "fork in main() failed" << std::cout;
  if (pid > 0) exit (0);
  umask (0);
  if (setsid () < 0)
    std::cout << "setsid() in main() failed" << std::cout;
  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);
}

int AegeeMilter::LoadPlugins() {
  lt_dlinit ();
  int j = 1;
  std::string g_mods {"Loaded modules:"};
  std::string g_lists {"Loaded lists (with tables):"};
  while (lt_preloaded_symbols[j].name) {
    if (lt_preloaded_symbols[j++].address) continue;
    prdr_section = lt_preloaded_symbols[j-1].name;
    prdr_section.resize(prdr_section.find('.'));
    char ** array = g_key_file_get_keys (prdr_inifile, prdr_section.c_str(),
					 NULL, NULL);
    if (array == NULL)
      // std::cout << "SKIP " << lt_preloaded_symbols[j-1].name << std::endl;
      continue;
    g_strfreev(array);

    //std::cout << "LOAD " <<lt_preloaded_symbols[j-1].name << std::endl;
    lt_dlhandle mod = lt_dlopen (lt_preloaded_symbols[j-1].name);
    if (lt_preloaded_symbols[j-1].name[0] == 'm') { //load module
      SoModule* (*init)() = (SoModule*(*)()) lt_dlsym(mod, "init");
      so_modules.emplace_back (init (), [](SoModule* x) {lt_dlhandle mod = x->mod; delete x; lt_dlclose (mod);});
      so_modules.back()->mod = mod;
      so_modules.back()->name = prdr_section;
      g_mods += "\n  " + prdr_section;
    } else { //load list
      SoList* (*init)() = (SoList*(*)()) lt_dlsym(mod, "init");
      so_lists.emplace_back (init (), [](SoList* x) {lt_dlhandle mod = x->mod; delete x; lt_dlclose (mod);});
      auto& it = so_lists.back ();
      it->mod = mod;
      const std::vector<std::string> &so_tables { it->Tables() };
      if (so_tables.empty ()) {
        so_lists.pop_back ();
	continue;
	//std::cout << "The list backend \"" << lt_preloaded_symbols[j-1].name << "\", does not export prdr_list_tables. The module is useless. aegee-milter exits (config file : " << filename << "..." << std:endl;
      } else {
        g_lists += "\n  " + prdr_section + " (";
        //load tables
	for (const std::string& table: so_tables) {
          g_lists += table + ", ";
	  //std::cout << "loading table " << exported_tables[j] << std:endl;;
	  tables[table] = it.get ();
	}
        g_lists[g_lists.length() - 2] = ')';
      }
    }
  }
  std::cout << g_mods << std::endl << g_lists << std::endl;
  if (so_modules.size() > 31)
    std::cout << "aegee-milter can operate with up to 31 modules, you are "
            "trying to load " << so_modules.size() << " modules." << std::endl;
  so_lists.shrink_to_fit ();
  so_modules.shrink_to_fit ();
  return 0;
}

int AegeeMilter::ProceedConfFile(const std::string& filename) {
  UnloadPlugins ();
  prdr_inifile = g_key_file_new ();
  g_key_file_set_list_separator (prdr_inifile, ',');
  GError *err;
  if (!g_key_file_load_from_file (prdr_inifile, filename.c_str(),
				  G_KEY_FILE_NONE, &err)) {
    std::cout << "Unable to parse file " << filename << " error: "
              << err->message << std::endl;
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
  if (char* temp = g_strstrip (g_key_file_get_string (prdr_inifile, "General", "sendmail", NULL))) {
    sendmail = temp;
    g_free (temp);
  } else
    sendmail = "/usr/bin/sendmail";
  alarm_period = g_key_file_get_integer (prdr_inifile, "General", "expire",
					 NULL);
  if (alarm_period <0 )
    std::cout << "The expire value in configuration file \"" << filename << "\" must be positive";
  else if (alarm_period == 0)
    alarm_period = 24;
  alarm (alarm_period * 60 * 60);

  //std::cout << "bounce-mode loaded as " << bounce_mode << std::endl;
  LoadPlugins ();

  //section Milter
  char **array = g_key_file_get_keys (prdr_inifile, "Milter", NULL, NULL);
  if (array) {
    int i = 0;
    while (array[i]) {
      if (strcmp (array[i], "timeout") == 0) {
	int timeout = g_key_file_get_integer (prdr_inifile, "Milter",
                                              "timeout", NULL);
	if (timeout)
	  smfi_settimeout (timeout);
      } else 
	if (strcmp (array[i], "socket") == 0) {
	  char* temp = g_strstrip (g_key_file_get_string (prdr_inifile,
						"Milter", "socket", NULL));
	  if (smfi_setconn (temp) == MI_FAILURE)
	    std::cout << "Connection to '" << temp << "' could not be "
                         "established, as specified in configuration file \""
                      << filename <<"\"." << std::endl;
	  if (smfi_opensocket (1) == MI_FAILURE)
	    std::cout << "Socket " << temp << " specified in configuration file " << filename << "could not be opened" << std::cout;
	  g_free (temp);
	} else
	  std::cout << "Keyword " << array[i] << " has no place in secion [Milter] of " << filename << std::endl;
      i++;
    }
    g_strfreev (array);
  }
  if (char* temp = g_strstrip (g_key_file_get_string (prdr_inifile, "General",
						      "pidfile", NULL))) {
    FILE *stream = fopen (temp, "w");
    if (!stream)
      std::cout << "Couldn't open pid file " << temp << "." << std::endl;
    fprintf (stream, "%i\n", getpid ());
    fclose (stream);
    g_free (temp);
  }
  g_key_file_free (prdr_inifile);
  return 0;
}

void AegeeMilter::UnloadPlugins() {
  so_modules.clear ();
  tables.clear ();
  so_lists.clear ();
  lt_dlexit ();
}

const std::string
AegeeMilter::NormalizeEmail (const std::string& email)
{
  //remove spaces and <
  if (email == "<>") return "<>";
  size_t index = email.find_first_not_of (" <");
  //lowercase the characters
  std::string temp;
  while (index++ < email.size () && email[index-1] != ' '
	 && email[index-1 ] != '>' )
    temp += tolower (email[index-1]);
  return temp;
}

std::string AegeeMilter::ListQuery (const std::string& table,
				    const std::string& user,
				    const std::string& key) {
  return tables.at (table)->Query (table, user, key);
}

void AegeeMilter::ListInsert (const std::string& table,
			      const std::string& user, const std::string& key,
			      const std::string& data) {
  tables.at (table)->Insert (table, user, key, data, 0);
}

bool AegeeMilter::ListRemove (const std::string& table,
			      const std::string& user, const std::string& key)
{
  return tables.at (table)->Remove (table, user, key);
}

static std::string get_date ()
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
  char date [64];
  snprintf (date, 64, "%s, %02d %s %4d %02d:%02d:%02d %c%.2lu%.2lu",
	    wday[lt.tm_wday], lt.tm_mday, month[lt.tm_mon],
	    lt.tm_year + 1900, lt.tm_hour, lt.tm_min, lt.tm_sec,
	    gmtneg ? '-' : '+', gmtoff / 60, gmtoff % 60);
  return date;
}

const std::string& AegeeMilter::Sendmail () {
  return sendmail;
}

//return values: != 0 is error, == 0 is OK
int AegeeMilter::Sendmail (const std::string& from,
			   const std::vector<std::string>& rcpt,
			   const std::string& body,
			   const std::string& date,
			   const std::string& autosubmitted) {
  std::string cmdline {sendmail + " -f"};
  cmdline += (from.empty () || from=="<>") ? "\\<\\>" : from;//check if "" needed
  for (const std::string& it : rcpt) cmdline += " " + it;

  if (FILE *sm = popen (cmdline.c_str (), "w")) {
    fprintf (sm, "%s: %s\r\n", date.c_str (), get_date ().c_str ());
    if (!autosubmitted.empty ())
      fprintf (sm, "Auto-Submitted: %s\r\n", autosubmitted.c_str ());
    fputs (body.c_str(), sm);
    return pclose (sm);
  } else
    return -1;
}
