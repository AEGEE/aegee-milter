#include "src/prdr-milter.h"
#include "src/core/AegeeMilter.hpp"
#include "src/core/Module.hpp"

Recipient::Recipient(char** argv) :
  address (AegeeMilter::NormalizeEmail (argv[0])), modules (AegeeMilter::so_modules.size()) {
  if (argv) {
    unsigned int k = 1;
    while (argv[k])
      if (g_ascii_strcasecmp (argv[k++], "NOTIFY=NEVER") == 0)
	flags |= RCPT_NOTIFY_NEVER;
  }
}
