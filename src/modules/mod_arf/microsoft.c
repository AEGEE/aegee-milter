#include <string.h>
#include <glib.h>

char* ms_get_original_recipient (const char* body_text) {
  char *temp = strstr (body_text, "\r\nX-HmXmrOriginalRecipient: ");
  if (temp != NULL)
    return g_strndup (temp+28, strchr (temp+28, '\r') - temp - 28);
  else
    return NULL;
}

char* ms_get_sender_address (const char* body_text) {
  char *temp = strstr (body_text, "\r\nX-SID-PRA: ");
  if (temp != NULL)
    return g_strndup (temp + 13, strchr( temp + 13, '\r') - temp - 13);
  else
    return NULL;
}
