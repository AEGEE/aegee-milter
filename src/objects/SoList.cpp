#include "src/objects/SoList.hpp"

extern "C" {
  extern const lt_dlsymlist lt_preloaded_symbols[];
}

SoList::SoList (const char * const &name) {
  if (name[0] != 'l')
    throw -2;

  mod = lt_dlopen (name);

  int (*load)() = (int(*)())lt_dlsym (mod, "load");
  if (load && ( 0 != load ()))
    throw -4;

  query = (char*(*)(const char*, const char*, const char*))lt_dlsym (mod, "prdr_list_query");
  insert = (int(*)(const char*, const char*, const char*, const void*, const unsigned int))lt_dlsym (mod, "prdr_list_insert");
  expire = (int(*)())lt_dlsym (mod, "prdr_list_expire");
  remove = (int(*)(const char*, const char*, const char*))lt_dlsym (mod, "prdr_list_remove");
}

char* SoList::Query(const char* table, const char* user, const char* key) {
  return query (table, user, key);
}

int SoList::Remove(const char* table, const char* user, const char* key) {
  return remove (table, user, key);
}


int SoList::Insert(const char* table, const char* user, const char* key, const char* value, const unsigned int expire) {
  return insert (table, user, key, value, expire);
}

int SoList::Expire() {
  return expire ();
}


char** SoList::Tables () {
  char ** (*tables) ()
    = (char**(*)())lt_dlsym (mod, "prdr_list_tables");
  return (tables == NULL) ? NULL : tables();
}

SoList::~SoList () {
  void (*unload)();
  unload = (void(*)())lt_dlsym(mod, "unload");
  if (unload) unload ();
  lt_dlclose (mod);
}
