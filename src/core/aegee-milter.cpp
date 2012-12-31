#include "src/objects/AegeeMilter.hpp"
#include "src/core/intern.hpp"
extern "C" {
#include <sys/stat.h>
#include <signal.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include "src/prdr-milter.h"
}
//----------------------------------------------------------------------------

extern "C" struct smfiDesc smfilter;

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
  int i=0, c;
  char *string = g_strconcat (SYSCONFDIR, CONF_FILE, NULL);
  while ((c = getopt (argc, argv, "vhc:")) != -1)
    switch (c){
    case 'h':
      g_printf ("aegee-milter 1.0\nCopyright (C) 2008 Dilyan Palauzov <dilyan.palauzov@aegee.org>\n\nUsage:\n -c file  : loads the configuration file. If the parameter is a directory, a file called aegee-milter.conf within it is opened\n -v  : show the version and exit\n -h  : print this help\n\n");
      return -1;
    case 'v':
      g_printf ("PRDR Milter 1.0, (C) 2007 Dilyan Palauzov\n\n");
      return -1;
    case 'c':
      if (optarg == NULL || *optarg == '\0')
	g_printf ("-c requires as parameter a configuration file or a directory containing %s\n", CONF_FILE);
      struct stat *buf = (struct stat*)g_malloc (sizeof (struct stat));
      if (stat (optarg, buf))
	g_printf ("File %s does not exist.\n", optarg);
      g_free(string);
      if (S_ISDIR (buf->st_mode))
	string = g_strconcat (optarg, CONF_FILE, NULL);
      else {
	string = optarg;
	i = 1;
      }
      g_free (buf);
    }
  if (smfi_register (smfilter) == MI_FAILURE)
    g_printf ("smfi_register failed, most probably not enough memory\n");
  if (aegeeMilter.ProceedConfFile (string) != 0) {
    if (i != 1) g_free (string);
    return -10;
  }
  if (i != 1) g_free (string);
  aegeeMilter.Fork();
  smfi_main ();
  AegeeMilter::UnloadPlugins ();
  return 0;
}
