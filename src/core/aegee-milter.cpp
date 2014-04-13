#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gmime/gmime.h>

#include "src/core/AegeeMilter.hpp"
extern "C" {
  #include "libmilter/mfapi.h"
  #include "src/prdr-milter.h"
  extern struct smfiDesc smfilter;
}

int
main (int argc, char **argv)
{
  AegeeMilter aegeeMilter;
  /**
   * h - help
   * c - configuration file
   * v - version info
   */
  //  g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
#define CONF_FILE "/aegee-milter.ini"
  int c;
  std::string str (SYSCONFDIR CONF_FILE);
  while ((c = getopt (argc, argv, "vhc:")) != -1)
    switch (c){
    case 'h':
      std::cout << "aegee-milter 1.0" << std::endl
         << "Copyright (C) 2008-2013 Dilyan Palauzov <dilyan.palauzov@aegee.org>"
         << std::endl << std::endl << "Usage:" << std::endl 
         << " -c file  : loads the configuration file. If the parameter is a"
         " directory, a file called aegee-milter.conf within it is opened"
         << std::endl << "-v  : show the version and exit"
         << std::endl << "-h  : print this help" << std::endl << std::endl;
      return -1;
    case 'v':
      std::cout << "PRDR Milter 1.0, (C) 2007-2013 Dilyan Palauzov"
                << std::endl << std::endl;
      return -1;
    case 'c':
      if (optarg == NULL || *optarg == '\0') {
	std::cout << "-c requires as parameter a configuration file or a directory containing " << CONF_FILE << std::endl;
	return -1;
      }
      struct stat buf;
      if (stat (optarg, &buf))
	std::cout << "File " << std::string (optarg) << "does not exist." << std::endl;
      str = *optarg;
      if (S_ISDIR (buf.st_mode)) str += CONF_FILE;
    }
  if (smfi_register (smfilter) == MI_FAILURE)
    std::cout << "smfi_register failed, most probably not enough memory" << std::endl;
  if (aegeeMilter.ProceedConfFile (str) != 0) {
    return -10;
  }
  //  aegeeMilter.Fork();
  smfi_main ();
  AegeeMilter::UnloadPlugins ();
  g_mime_shutdown ();
  return 0;
}
