#include <time.h>
#include <sys/time.h>
#include <iostream>
#include <cstring>
#include "src/core/Privdata.hpp"

static int delay_ehlo = 0;
static int delay_mail = 10;
static int delay_rcpt = 5;
static int delay_data = 30;
extern GKeyFile* prdr_inifile;

struct delay final : public SoModule {
  delay () {
    char **array = g_key_file_get_keys (prdr_inifile, "mod_delay", NULL, NULL);
    int i = 0;
    while (array[i])
      if (std::strcmp (array[i++], "ehlo") == 0)
	delay_ehlo = g_key_file_get_integer (prdr_inifile, "mod_delay",
					     "ehlo", NULL); else
      if (std::strcmp (array[i-1], "mail") == 0)
        delay_mail = g_key_file_get_integer (prdr_inifile, "mod_delay",
					   "mail", NULL); else
      if (std::strcmp (array[i-1], "rcpt") == 0)
        delay_rcpt = g_key_file_get_integer (prdr_inifile, "mod_delay",
					   "rcpt", NULL); else
      if (std::strcmp (array[i-1], "data") == 0)
        delay_data = g_key_file_get_integer (prdr_inifile, "mod_delay",
					   "data", NULL); else {
      std::cerr << "Option " << array[i-1] << " in section [mod_delay] is not recognized" << std::endl;
      g_strfreev (array);
      throw -1;
    }
    g_strfreev (array);
  }

  bool Run (Privdata& priv) const override {
    time_t *last_time = (time_t*)priv.GetPrivMsg();
    if (last_time == nullptr) return 0;
    time_t current_time = time (NULL);
    long diff = (long) difftime (current_time, *last_time);
    switch (priv.GetStage()) {
    case MOD_EHLO:
      diff = delay_ehlo - diff;
      break;
    case MOD_MAIL:
      diff = delay_mail - diff;
      break;
    case MOD_RCPT:
      diff = delay_rcpt - diff;
      break;
    case MOD_BODY:
      diff = delay_data - diff;
      break;
    }
    if (diff > 0) {
      struct timeval timeout = {diff, 0};
      select(0, NULL, NULL, NULL, &timeout);
    }
    if (priv.GetStage () == MOD_BODY) {
      delete last_time;
      priv.SetPrivMsg (nullptr);
    } else
      time(last_time);
    return true;
  }

  int Status (Privdata&) const override {
    return MOD_EHLO | MOD_MAIL | MOD_RCPT | MOD_BODY;
  }

  void InitMsg (Privdata& priv) const override {
    time_t *i = new time_t;
    time(i);
    priv.SetPrivMsg(i);
  }

  void DestroyMsg (Privdata& priv) const override {
    time_t *i = (time_t*) priv.GetPrivMsg ();
    if (i) delete i;
  }
};

extern "C" SoModule* mod_delay_LTX_init () {
  return new delay;
}
