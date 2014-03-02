extern "C" {
  #include <glib.h>
  #include <liblistserv.h>
}
#include <ctype.h>
#include <iostream>
#include <set>
#include <stdio.h>
#include <string.h>
#include "src/core/SoList.hpp"

extern GKeyFile* prdr_inifile;

static char *email = NULL;
static char *host = NULL;
static char *password = NULL;
#define LISTSERV_HOME "/home/listserv"

static void
extract_emails_from (listserv* const l,
		       const std::string& listname,
		       const std::string& keyword,
		       std::set<std::string>& emails,
		       std::set<std::string>& loop)
{
  unsigned int i = loop.size ();
  std::string a;
  for (const char& it: listname + "|" + keyword)
    a += tolower (it);
  loop.insert (a);
  if (loop.size () == i) return; //LOOP detected
  if (keyword.empty ()) {
    //insert all subscribers
    unsigned int i = 0;
    listserv_subscriber** ls
      = listserv_getsubscribers (l, listname.c_str());
    while (ls[i]) {
      std::string a;
      for (const char& it : std::string(ls[i]->email))
	a += tolower (it);
      emails.insert (a);
      i++;
    }
    return;
  }
  if (char **kw = listserv_getlist_keyword (l, listname.c_str(), keyword.c_str())) {
    unsigned int i = 0;
    while (kw[i])
      if (strchr (kw[i++], '@')) /*we have an email address */
	emails.insert (kw[i-1]);
      else
	if (strchr (kw[i-1], '(') != NULL) { /* external list */
	  std::string temp1 {strchr (kw[i-1], '(') + 1};
	  temp1.resize (temp1.find (')'));
	  extract_emails_from (l, temp1, kw[i-1], emails, loop);
	  kw = listserv_getlist_keyword (l, listname.c_str (), keyword.c_str ());
	}
  }
}


static std::string
listserv_check_subscriber (const std::string& listname,
			  const std::string& emailaddress) {
  std::string query { "QUERY " + listname + " FOR " + emailaddress };
  listserv *l = listserv_init (email, password, host);
  char ret = listserv_command (l, query.c_str())[0];
  listserv_destroy (l);
  return (ret == 'T')
    ? "no" //is not subscribed or there is no such list
    : "yes"; //is subscribed
}

class _listserv final: public SoList {
 public:
  _listserv () {
    if (char **array = g_key_file_get_keys (prdr_inifile, "list_listserv", NULL, NULL)) {
      int i = 0;
      while (array[i])
        if (strcmp (array[i++], "email") == 0)
	  email = g_key_file_get_string (prdr_inifile, "list_listserv",
					 "email", NULL); else
	if (strcmp (array[i-1], "host") == 0)
	  host = g_key_file_get_string (prdr_inifile, "list_listserv",
					  "host", NULL); else
	if (strcmp (array[i-1], "password") == 0)
	  password = g_key_file_get_string (prdr_inifile, "list_listserv",
				"password", NULL); else {
	  std::cerr << "Option " << array[i-1] << " in section [list_listserv] is not recognized" << std::endl;
	  g_strfreev (array);
	  throw -1;
	}
      g_strfreev (array);
    };
    listserv *l = listserv_init (email, password, host);
    if (l)
      listserv_destroy (l);
    else
      std::cout << "cannot connect to listserv on host " << host << " with email " << email << " and password " << password << std::endl;
  }

  //if key == NULL -> redirect, return all email addresses
  //else check if key is in user
  std::string Query (const std::string& table, const std::string& user
	     /* listname */, const std::string& key /* email */) override {
 
    // return NULL if the element is not found, otherwise the value
    // printf("prdr_list_query table %s, user %s key %s\n", table, user, key);
    if (table == "listserv-check-subscriber")
      return listserv_check_subscriber (user, key);
    std::string keyword, listname = user;
    size_t it = listname.find('|');
    if (it != std::string::npos) {
      keyword = listname.c_str() + it + 1;
      listname.resize (it);
    }
    std::set<std::string> emails, loop;
    listserv *l = listserv_init (email, password, host);
    extract_emails_from (l, listname, keyword, emails, loop);
    listserv_destroy (l);
    if (key.empty ()) {
      std::string ret;
      for (const std::string& it : emails)
	ret += it + ", ";
      ret.resize (ret.size() - 2);
      return ret;
    }
    std::string key2;
    for (const char& c : key)
      key2 += g_ascii_tolower (c);

    return (emails.find (key2) != emails.end ()) ? key : "";
  };

  bool Remove (__attribute__((unused)) const std::string& table,
	      const std::string& user, const std::string& key) override {
    listserv *l = listserv_init (email, password, host);
    //table -- "listserv"
    //user -- subscriber to be removed, form "listname-L email"
    // key 'q' -- quiet, otherwise non-quiet
    std::string cmd = "DEL " + user;
    if (!key.empty () && key[0] == 'q')
      cmd = "QUIET " + cmd;
    char *ret = listserv_command (l, cmd.c_str());

    bool k = strstr ("is not subscribed", ret) != NULL;
    listserv_destroy (l);
    return k;
  }

  std::vector<std::string> Tables () override {
    return {"listserv", "listserv-check-subscriber"};
  }
};

extern "C" SoList* list_listserv_LTX_init () {
  return new _listserv;
}
