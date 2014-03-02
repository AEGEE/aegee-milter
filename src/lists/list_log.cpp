extern "C" {
  #include <glib.h>
}
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern "C" GKeyFile* prdr_inifile;

#include "src/core/SoList.hpp"

class _log final: public SoList {
  FILE* file = nullptr;
public:
  _log () {
    char* filename = NULL;
    if (char **array = g_key_file_get_keys (prdr_inifile, "list_log", NULL, NULL)) {
      int i = 0;
      while (array[i])
	if ( strcmp (array[i++], "file") == 0)
	  filename = g_key_file_get_string (prdr_inifile, "list_log",
					    "file", NULL);
	else {
	  std::cerr << "Option " << array[i-1] << " in section [list_log] is not recognized" << std::endl;
	  g_strfreev (array);
	  throw -1;
	}
      g_strfreev (array);
    }
    if (filename) {
      file = fopen (filename, "a");
      free (filename);
      if (file) {
	fprintf (file, "Timestamp           User     Module     Message\n");
	fprintf (file, "================================================================\n");

	time_t _time = time (NULL);
	struct tm lt;
	localtime_r (&_time, &lt);
	fprintf (file,
	   "%04d/%02d/%02d %02d:%02d:%02d          mod_log    Started\n",
		   1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday,
		   lt.tm_hour, lt.tm_min, lt.tm_sec);
	fflush (file);
      } else
	std::cout << "Open " << filename << " failed" << std::endl;
    }
  }

  //table == log, user == , key == module, value ==message
  void Insert (const std::string& table __attribute__((unused)),
	      const std::string& user,
	      const std::string& key, const std::string& value,
	      const unsigned __attribute__((unused)) int)  override {
    if (file) {
      time_t _time = time (NULL);
      struct tm lt;
      localtime_r (&_time, &lt);
      // flockfile(file);
      fprintf (file, "%04d/%02d/%02d %02d:%02d:%02d %s, %s, %s\n", 1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec, user.c_str(), key.c_str(), value.c_str());
      fflush (file);
      // funlockfile(file);
    }
  }

  ~_log () override {
    if (file) {
      time_t _time = time (NULL);
      struct tm lt;
      localtime_r (&_time, &lt);
      fprintf (file, "%04d/%02d/%02d %02d:%02d:%02d          mod_log    Terminating\n", 1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
      // fflush (file); happens automatically on fclose
      fclose (file);
    }
    std::cout << "list_log unloaded" << std::endl;
  }

  std::vector<std::string> Tables () {
    return { "log" };
  }

};

extern "C" SoList* list_log_LTX_init () {
  return new _log;
}
