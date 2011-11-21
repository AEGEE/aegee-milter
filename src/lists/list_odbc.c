#define _GNU_SOURCE
#include <features.h>
#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#define UNICODE 1
GKeyFile* prdr_inifile;
char* prdr_section;

static SQLHENV env;
static SQLHDBC dbc;
static SQLHSTMT stmt;
//static SQLHDESC desc;
static const char **exported_tables = NULL;
static char* sql_insert_permanent = NULL;
static char* sql_insert_expire = NULL;
static char* sql_query = NULL;
static char* sql_remove = NULL;
static char* sql_expire = NULL;
static GMutex* mutex;

int
list_odbc_LTX_load ()
{
  mutex = g_mutex_new ();
  g_mutex_lock (mutex);
  char* sql_create_table = NULL;
  char* sql_init = NULL;
  //which tables;
  char **array = g_key_file_get_keys (prdr_inifile, prdr_section, NULL, NULL);
  char *dsn = NULL, *database = NULL;
  int i = 0;
  if (array) {
    while (array[i]) {
      if (strcmp (array[i], "dsn") == 0 )
	dsn = g_key_file_get_string (prdr_inifile, prdr_section, "dsn", NULL);
      else
      if (strcmp (array[i], "tables") == 0)  //tables
	exported_tables =
	  (const char**)g_key_file_get_string_list (prdr_inifile,
						    prdr_section, "tables",
						    NULL, NULL);
      else
      if (strcmp (array[i], "database") == 0)
	database = g_key_file_get_string (prdr_inifile, prdr_section,
					  "database", NULL);
      else
      if (strcmp (array[i], "init") == 0)
	sql_init = g_key_file_get_string (prdr_inifile, prdr_section,
					  "init", NULL);
      else
      if (strcmp (array[i], "create_table") == 0)
	sql_create_table = g_key_file_get_string (prdr_inifile, prdr_section,
						  "create_table", NULL);
      else
      if (strcmp (array[i], "insert_permanent") == 0)
	sql_insert_permanent = g_key_file_get_string (prdr_inifile, 
						      prdr_section,
						      "insert_permanent",
						      NULL);
      else
      if (strcmp (array[i], "insert_expire") == 0)
	sql_insert_expire = g_key_file_get_string (prdr_inifile, prdr_section,
						   "insert_expire", NULL);
      else
      if (strcmp (array[i], "remove") == 0)
	sql_remove = g_key_file_get_string (prdr_inifile, prdr_section,
					    "remove", NULL);
      else 
      if (strcmp (array[i], "query") == 0)
	sql_query = g_key_file_get_string (prdr_inifile, prdr_section,
					   "query", NULL);
      else
      if (strcmp(array[i], "expire") == 0)
	sql_expire = g_key_file_get_string (prdr_inifile, prdr_section,
					    "expire", NULL);
      else {
	g_printf ("option %s in section [%s] is not recognized\n",
		  array[i], prdr_section);
	g_strfreev (array);
	g_mutex_unlock (mutex);
	g_mutex_free (mutex);
	return -1;
      }
      i++;
    }
    g_strfreev (array);
  }
  SQLRETURN ret;
  if (sql_insert_permanent == NULL) sql_insert_permanent = sql_insert_expire;
  SQLAllocHandle (SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
  SQLSetEnvAttr (env, SQL_ATTR_ODBC_VERSION, (void*) SQL_OV_ODBC3, 0);
  SQLAllocHandle (SQL_HANDLE_DBC, env, &dbc);
  SQLSMALLINT size = 0;
  char connection_string[1024];
  ret = SQLDriverConnect (dbc, 0 /* WindowHandle */,
			  dsn, SQL_NTS,
			  connection_string, sizeof(connection_string),
			  &size, SQL_DRIVER_COMPLETE);
  if (database == NULL) {
    char *temp = strcasestr (connection_string, "UID=");
    if (temp) {
      database = g_strdup ( temp+4);
      if (temp = strchr (database, ';'))
	temp[0] = '\0';
    }
  }
  if (! SQL_SUCCEEDED (ret)) {
    g_strfreev ((char**)exported_tables);
    exported_tables = NULL;
    g_printf ("%s cannot connect to resourse DSN=%s, error=%i\n",
	      prdr_section, dsn, ret);
    g_mutex_unlock (mutex);
    g_mutex_free (mutex);
    return -1;
  };
  //call initial SQL command
  SQLAllocHandle (SQL_HANDLE_STMT, dbc, &stmt);
  if (sql_init) {
    ret = SQLExecDirect (stmt, sql_init, SQL_NTS);
    if (! SQL_SUCCEEDED (ret)) {
      g_printf ("Executing %s returned ODBC error code %i\n", sql_init, ret);
      g_mutex_unlock (mutex);
      g_mutex_free (mutex);
      return -1;
    }
    g_free (sql_init);
  }
  //create the tables
  for (i = 0; exported_tables[i] ; i++) {
    char *query = g_malloc (strlen(sql_create_table)
			    + strlen (exported_tables[i]) + 3);
    g_sprintf (query, sql_create_table, exported_tables[i]);
    ret = SQLExecDirect (stmt, query, SQL_NTS);
    if (! SQL_SUCCEEDED (ret)) {
      g_printf("Executing %s returned error code %i\n", query, ret);
      g_free(query);
      g_mutex_unlock (mutex);
      g_mutex_free (mutex);
      return -1;
    }
    g_free (query);
  };
  if (sql_create_table) g_free (sql_create_table);
  g_free (database);
  g_free (dsn);
  g_mutex_unlock (mutex);
  return 0;
}

int
list_odbc_LTX_prdr_list_insert (const char* const table,
				const char* const user,
				const char* const key,
				const char* const value,
				const unsigned int expire)
{
  //execute query
  //INSERT INTO table VALUES(null, user, key, value);
  SQLRETURN ret;
  //statement
  char *sql;
  if (expire == 0) {
    sql = g_malloc (strlen (table) + strlen (sql_insert_permanent)
		    + strlen (user) + strlen (key) + strlen (value) + 5);
    g_sprintf (sql, sql_insert_permanent, table, user, key, value);
  } else {
    sql = g_malloc (strlen (table) + strlen (sql_insert_expire)
		    + strlen (user) + strlen (key) + strlen (value) + 5);
    g_sprintf (sql, sql_insert_expire, table, expire, user, key, value);
  };
  g_mutex_lock (mutex);
  ret = SQLExecDirect (stmt, sql, SQL_NTS);
  g_mutex_unlock (mutex);
  g_free (sql);
  return ret;
}

void*
list_odbc_LTX_prdr_list_query (const char* table, const char *user, const char *key) {
  //SELECT
  return NULL;
}

int
list_odbc_LTX_prdr_list_expire ()
{
  if (sql_expire == NULL) return;
  int i = 0;
  SQLRETURN ret = 0;
  g_mutex_lock (mutex);
  while (exported_tables[i]) {
    char *sql = g_malloc (strlen (sql_expire) + strlen (exported_tables[i]));
    g_sprintf (sql, sql_expire, exported_tables[i++]);
    ret = SQLExecDirect (stmt, sql, SQL_NTS);
    g_free (sql);
  }
  g_mutex_unlock (mutex);
  return ret;
}

int
list_odbc_LTX_prdr_list_remove (const char* table, const char* user, const char* key)
{
  SQLRETURN ret;
  char *sql = g_malloc (strlen (table) + strlen (user) + strlen (key));
  g_sprintf (sql, sql_remove, table, user, key);
  g_mutex_lock (mutex);
  ret = SQLExecDirect (stmt, sql, SQL_NTS);
  g_mutex_unlock (mutex);
  g_free (sql);
  return ret;
}

char*
list_odbc_LTX_unload ()
{
  g_mutex_lock (mutex);
  g_mutex_unlock (mutex);
  g_mutex_free (mutex);
  SQLFreeHandle (SQL_HANDLE_STMT, stmt);
  SQLDisconnect (dbc);
  SQLFreeHandle (SQL_HANDLE_DBC, dbc);
  SQLFreeHandle (SQL_HANDLE_ENV, env);
  g_strfreev ((char**)exported_tables);
  return NULL;
}

const char**
list_odbc_LTX_prdr_list_tables ()
{
  return exported_tables;
}
