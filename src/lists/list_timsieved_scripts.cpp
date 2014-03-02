//logic: first open default.siv.script.  If it is not available, open the activated script
extern "C" {
  #include <glib.h>
}

#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "src/core/SoList.hpp"


extern GKeyFile *prdr_inifile;

class timsieved_scripts final : public SoList {
  std::string sievedir = "/usr/sieve";
  static constexpr const char*const prdr_section = "list_timsieved_scripts";
 public:
  timsieved_scripts () {
    if (std::unique_ptr<char*[], void(*)(char**)> array { g_key_file_get_keys (prdr_inifile, prdr_section, NULL, NULL), &g_strfreev }) {
      int i = 0;
      while (array[i])
	if (strcmp (array[i++], "sievedir")) {
	  std::cout << "option " << array[i-1] << " in section [" << prdr_section << "] is not recognized" << std::endl;
	  throw -1;
	} else {
	  char* ret = g_key_file_get_string (prdr_inifile, prdr_section,
					    "sievedir", NULL);
	  sievedir = ret;
	  free (ret);
	}
    }
    if (sievedir.back() != '/') sievedir += '/';
  }

  //this function is not multidomain capable
  //it eats the first @ in the user and everything after it
  //correct it to reflect hashimapspool and fulldirhash
  std::string Query (const std::string& table, const std::string& user, const std::string& key) override {
  //return NULL if the element is not found, otherwise the value
    int format;
    if (table == "sieve_scripts") format = 1;
    else if (table == "sieve_bytecode") format = 2;
    else if (table == "sieve_bytecode_path") format = 3;
    else return "";
    std::string sieve_dir = sievedir;
    if (user.empty () || user == ":global")
      sieve_dir += "global/";
    else {
      std::string user2 = user; //this will be an user, that has no domain
      size_t index = user2.find_first_of('+');
      if (index != std::string::npos) user2.resize (index);
      index= user2.find_first_of('@');
      if (index != std::string::npos) user2.resize (index);
      sieve_dir += user2[0] + std::string ("/") + user2 + "/";
    };
    int f;// file descriptor
    struct stat sb;
    if (key.empty ()) {
      sieve_dir += "default.siv." + std::string(format == 1 ? "script" : "bc");
      f = stat (sieve_dir.c_str (), &sb);
      if (f == -1) { // file not found
        //cut default.siv.script, add defaultbc <=> cut .siv.script add bc
	sieve_dir.resize (sieve_dir.length() - (format == 1?11:7));
	sieve_dir += "bc";

	if (char* temp = canonicalize_file_name (sieve_dir.c_str ())) {
	  sieve_dir = temp;
	  free (temp);
	} else
	  sieve_dir = "";//shall not happen
	if (format == 1) {
	  sieve_dir.resize(sieve_dir.length() - 2);
	  sieve_dir += "script";
	}
	f = stat (sieve_dir.c_str(), &sb);
      };
    } else {
      sieve_dir += key + "." + (format == 1 ? "script" : "bc");
      f = stat (sieve_dir.c_str(), &sb);
    }
    if (f != -1 && format == 3) return sieve_dir;
    if (f != -1) f = open (sieve_dir.c_str (), O_RDONLY, 0);
    if (f == -1) return "";
    std::unique_ptr<char[],void(*)(void*)> script_content 
      { static_cast <char*>(malloc (sb.st_size + 1)), &free };
    read (f, script_content.get(), sb.st_size);
    close (f);
    script_content[sb.st_size] = '\0';
    sieve_dir = script_content.get();
    //    free(script_content);
    //  g_printf("X[%s:%s]\n", user, key);
    return sieve_dir;
  }

  std::vector<std::string> Tables () override {
    return {"sieve_scripts", "sieve_bytecode", "sieve_bytecode_path"};
  }
};

extern "C" SoList* list_timsieved_scripts_LTX_init() {
  return new timsieved_scripts;
}
