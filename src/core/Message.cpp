#include "src/core/Message.hpp"

Message::Message(const std::string& envfrom) :
   envfrom (envfrom)
{
}

bool Message::AddHeader (const std::string& field, const std::string& value) {
  int i = 0;
  while (field[i])
    if (field[i++] <= 0 )
      return false;//header field contains non-ascii characters

  headers.emplace_back (field, value, 0);
  return true;
}

