#include <libmemcached/memcached.h>
#include "src/core/SoList.hpp"

class list_memcached final: public SoList {
  memcached_st *data;
public:
  list_memcached () {
    const char *const conf = "--SERVER=localhost --HASH-WITH-NAMESPACE --NAMESPACE=aegee-milter --TCP-NODELAY --TCP-KEEPALIVE --BINARY-PROTOCOL";
    data = memcached (conf, strlen (conf));
    memcached_behavior_set (data, MEMCACHED_BEHAVIOR_POLL_TIMEOUT, (int64_t)100);
    memcached_behavior_set (data, MEMCACHED_BEHAVIOR_RETRY_TIMEOUT, 10);
  };

  ~list_memcached () override {
    memcached_free (data);
  };

  void Insert (const std::string& table __attribute__((unused))/* memcached */,
	       const std::string& user, const std::string& key,
	       const std::string& value __attribute__ ((unused)),
	       const unsigned int expire __attribute__ ((unused)) /*relative, in seconds */) override {
    const std::string& key2 = user + "|" + key;
    memcached_set(data, key2.c_str (), key2.size (), value.c_str (), value.size (), 2505600 /* 29 Days in seconds */, 0); 
  };

  std::string Query (const std::string& table /* memcached */ __attribute__((unused)), const std::string& user /* listname */ , const std::string& key /* email */) override {
    const std::string& key2 = user + "|" + key;
    memcached_return_t ret = memcached_exist (data, key2.c_str (), key2.length ());
    return memcached_failed (ret) ? "" : "f";
  };

  std::vector<std::string> Tables() override {
    return {"memcached"};
  }
};

extern "C" SoList* list_memcached_LTX_init() {
  return new list_memcached;
}
