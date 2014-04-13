#include <map>
#include <memory>
#include <string>
#include <vector>
#include "src/core/SoList.hpp"
#include "src/core/SoModule.hpp"

class AegeeMilter final {
  int LoadPlugins();
public:
  static std::vector<std::unique_ptr<SoModule, void(*)(SoModule*)>> so_modules;
  static std::vector<std::unique_ptr<SoList, void(*)(SoList*)>> so_lists;
  static std::map<std::string, SoList*> tables;
  static std::string ListQuery (const std::string& table,
				const std::string& user,
				const std::string& key);
  static void ListInsert (const std::string& table, const std::string& user,
			  const std::string& key, const std::string& data);
  static bool ListRemove (const std::string& table, const std::string& user,
			  const std::string& key);

  AegeeMilter();
  ~AegeeMilter() =default;
  void Fork();
  int ProceedConfFile(const std::string& filename);
  static void UnloadPlugins();
  static const std::string NormalizeEmail(const std::string& email);
  static int Sendmail (const std::string& from,
		       const std::vector<std::string>& rcpt,
		       const std::string& body,
		       const std::string& date,
		       const std::string& autosubmitted);
  static const std::string& Sendmail ();
};
