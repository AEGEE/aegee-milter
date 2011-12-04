#include <sys/stat.h>
#include <signal.h>
#include <glib/gprintf.h>
#include "prdr-milter.h"
#include <glib/gstdio.h>

int bounce_mode;
unsigned int num_tables, num_so_lists, num_so_modules;
struct list **lists = NULL;
struct so_list **so_lists;
struct so_module **so_modules;
static int alarm_period;
extern const lt_dlsymlist lt_preloaded_symbols[];
char *prdr_section;

inline static void
unload_plugins ()
{
  unsigned int i;
  void (*unload) ();
  if (num_so_modules) {
    for (i = 0; i < num_so_modules; i++) {
      unload = lt_dlsym (so_modules[i]->mod, "unload");
      if (unload) unload ();
      lt_dlclose (so_modules[i]->mod);
      g_free(so_modules[i]);
    }
    g_free (so_modules);
  }

  for (; num_tables; num_tables--)
    g_free (lists[num_tables -1]);
  g_free (lists);
  lists = NULL;
  if (num_so_lists) {
    for (i = 0; i < num_so_lists; i++) {
      unload = lt_dlsym (so_lists[i]->mod, "unload");
      if (unload) unload();
      lt_dlclose (so_lists[i]->mod);
      g_free(so_lists[i]);
    }
    g_free (so_lists);
  }
  lt_dlexit ();
}

inline static int
load_plugins ()
{
  lt_dlinit();
  int j = 1;
  num_so_modules = 0;
  num_so_lists = 0;
  so_lists =   NULL; //g_malloc(num_so_lists   * sizeof(struct so_list*));
  so_modules = NULL; //g_malloc(num_so_modules * sizeof(struct so_module*));
  lt_dlhandle mod_handle;
  j = 1; //int temp;
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
    mod_handle = lt_dlopen (lt_preloaded_symbols[j-1].name);
    int (*load)();
    load = lt_dlsym (mod_handle, "load");
    if (load && ( 0 != load ())) {
      g_printf ("Loading %s failed.  Exiting...\n", prdr_section);
      return -1;
    }
    if (lt_preloaded_symbols[j-1].name[0] == 'm') { //load module
      struct so_module *mod = g_malloc0 (sizeof (struct so_module));
      g_string_append_printf (g_mods, "\n  %s", prdr_section);
      mod->mod = mod_handle;
      mod->name = lt_preloaded_symbols[j-1].name;
      //      g_printf("Loading module \"%s\"...\n", mod->name);
      mod->run = lt_dlsym (mod->mod, "prdr_mod_run");
      if (mod->run == NULL)
	g_printf ("Module \"%s\" does not define 'prdr_mod_run'. aegee-milter exits...\n", mod->name);
      mod->status = lt_dlsym (mod->mod, "prdr_mod_status");
      if (mod->status == NULL)
	g_printf ("Module \"%s\" does not define 'prdr_mod_status'. aegee-milter exits...\n", mod->name);
      mod->equal = lt_dlsym (mod->mod, "prdr_mod_equal");
      if (mod->equal == NULL)
	g_printf ("Module \"%s\" does not define 'prdr_mod_equal'\n",
		  mod->name);
      mod->init_msg = lt_dlsym (mod->mod, "prdr_mod_init_msg");
      mod->destroy_msg = lt_dlsym (mod->mod, "prdr_mod_destroy_msg");
      mod->init_rcpt = lt_dlsym (mod->mod, "prdr_mod_init_rcpt");
      mod->destroy_rcpt = lt_dlsym (mod->mod, "prdr_mod_destroy_rcpt");
      so_modules = g_realloc (so_modules,
			      sizeof(struct so_module*) * ++num_so_modules);
      so_modules[num_so_modules-1] = mod;
    } else { //load list
      struct so_list *mod = g_malloc0 (sizeof(struct so_list));
      g_string_append_printf (g_lists, "\n  %s (", prdr_section);
      mod->mod = mod_handle;
      //g_printf ("Loading lists module \"%s\"...\n", );
      char ** (*tables) ();
      tables = lt_dlsym (mod->mod, "prdr_list_tables");
      if (tables == NULL) {
        void (*unload) () = lt_dlsym (mod->mod, "unload");
        if (unload) unload();
        lt_dlclose (mod->mod);
	continue;
	//g_printf ("The list backend \"%s\", does not export prdr_list_tables. The module is useless. aegee-milter exits...\n", lt_preloaded_symbols[j-1].name, filename);
      } else {
	 //load tables
	char **exported_tables = tables ();
	if (exported_tables) {
	  int j = 0;
	  while (exported_tables[j]) {
	    g_string_append_printf (g_lists, "%s, ", exported_tables[j]);
	    //g_printf ("loading table %s\n", exported_tables[j]);
	    lists = g_realloc (lists, (1+num_tables) * sizeof (struct list*));
	    lists[num_tables] = g_malloc (sizeof (struct list));
	    lists[num_tables]->name = exported_tables[j++];
	    lists[num_tables++]->module = mod;
	  }
	}
      }
      g_string_truncate (g_lists, g_lists->len - 2);
      g_string_append_printf (g_lists, ") ");
      mod->query = lt_dlsym (mod->mod, "prdr_list_query");
      mod->insert = lt_dlsym (mod->mod, "prdr_list_insert");
      mod->expire = lt_dlsym (mod->mod, "prdr_list_expire");
      mod->remove = lt_dlsym (mod->mod, "prdr_list_remove");
      so_lists = g_realloc (so_lists,
			    sizeof (struct so_list*) * ++num_so_lists);
      so_lists[num_so_lists-1] = mod;
    }
    g_free (prdr_section);
  }
  g_printf ("%s\n", g_mods->str);
  g_printf ("%s\n", g_lists->str);
  g_string_free (g_mods, 1);
  g_string_free (g_lists, 1);
  if (num_so_modules > 31)
    g_printf ("aegee-milter can operate with up to 31 modules, you are trying to load %i.\n", num_so_modules);
  return 0;
}

static void
catch_signal (int sig)
{
  //g_printf("SIGNAL %i received\n", sig);
  //g_printf("Received signal %i\n", sig);
  switch(sig) {
    case SIGALRM:
      prdr_list_expire ();
      alarm(alarm_period * 60 * 60);
      break;
    case SIGTERM:
      g_printf ("SIGTERM\n");
    case SIGINT:
      g_printf ("SIGINT\n");
      smfi_stop ();
      unload_plugins ();
      break;
    case SIGHUP:
      smfi_stop ();
      unload_plugins ();
      //proceed_conf_file (filename);
      //smfi_main ();
      break;
  }
  //  signal (sig, catch_signal);//on System V signal has to be called after every signal handling or installed with sysv_signal
}


//----------------------------------------------------------------------------

inline int
proceed_conf_file (const char* filename)
{
  unload_plugins ();
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
    if (prdr_list_is_available ("delayed"))
      bounce_mode = 1;//pseudo-delayed
    else
      bounce_mode = 0;//delayed
  } else {
    if (g_ascii_strcasecmp (temp, "delayed")== 0 ) bounce_mode = 0;
    else if (g_ascii_strcasecmp (temp, "pseudo-delayed")== 0) {
      bounce_mode = 1;
      if (NULL != prdr_list_is_available ("delayed"))
	g_printf ("bounce-mode set to delayed, but no list called `delayed` was exposed by a list module, in configuration file \"%s\"...\n", filename);
    } else if (g_ascii_strcasecmp (temp, "ndr")== 0) bounce_mode = 2;
    else if (g_ascii_strcasecmp (temp, "no-ndr")== 0) bounce_mode = 3;
    else 
      g_printf ("bounce-mode not set correctly in configuration file \"%s\". %s is invalid value.\n", filename, temp);
    g_free (temp);
  }
  //g_printf("bounce-mode loaded as %i\n", bounce_mode);
  if (  load_plugins () == -1)
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
  if (chdir ("/") < 0)
    g_printf ("chdir(\"/\") in main() failed.\n");
  //fork in backgroud
  pid_t pid = fork ();
  if (pid < 0)
      g_printf ("fork in main() failed\n");
  if (pid > 0) exit (0);
      umask (0);
  if (setsid () < 0)
    g_printf ("setsid() in main() failed.\n");
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

int
main (int argc, char **argv)
{
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
      struct stat *buf = g_malloc (sizeof (struct stat));
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
  extern struct smfiDesc smfilter;
  if (smfi_register (smfilter) == MI_FAILURE)
    g_printf ("smfi_register failed, most probably not enough memory\n");
  lt_dlpreload_default (lt_preloaded_symbols);
  g_thread_init (NULL);
  if (proceed_conf_file (string) != 0) {
    if (i != 1) g_free (string);
    return -10;
  }
  if (i != 1) g_free (string);
  close (STDIN_FILENO);  close (STDOUT_FILENO);  close (STDERR_FILENO);
  smfi_main ();
  unload_plugins ();
  return 0;
}
