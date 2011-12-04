#ifndef PRDR_LISTS_H
#define PRDR_LISTS_H

#ifdef __GNUC__
#define UNUSED __attribute__((__unused__))
#else
#define UNUSED
#endif

//functions for DB modules

//returns 0 on success, -1 if the element was already inside, expire specifies when the entry will be deleted in seconds
int
prdr_list_insert (const char* const table, const char *user, const char *key,
		  const char *value, const unsigned int expire);
//return NULL if the element is not found, otherwise the value
char*
prdr_list_query (const char *table, const char *user, const char *key);
//purges expired data from all tables. If undefined, prdr_list_expire is called
void
prdr_list_expire ();
//returns 0 on success, 1 if the element was not found, -1 on failure
int
prdr_list_remove (const char *table, const char *user, const char *key);
const char**
prdr_list_tables ();

//functions that can be used by DB modules
#endif
