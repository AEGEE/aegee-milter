#include <string>
#include <cstring>

__attribute__ ((visibility ("hidden")))
std::string ms_get_original_recipient (const std::string& body_text) {
  if (const char *temp = std::strstr (body_text.c_str (), "\r\nX-HmXmrOriginalRecipient: "))
    return {temp + 28, static_cast<long unsigned int>(std::strchr (temp + 28, '\r') - temp - 28)};
  else
    return "";
}

__attribute__ ((visibility ("hidden")))
std::string ms_get_sender_address (const std::string& body_text) {
  if (const char *temp = std::strstr (body_text.c_str (), "\r\nX-SID-PRA: "))
    return {temp + 13, static_cast<long unsigned int>(std::strchr (temp + 13, '\r') - temp - 13)};
  else
    return "";
}
