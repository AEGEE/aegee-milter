#define _GNU_SOURCE
#include "prdr-mod.h"
#include <glib/gstdio.h>

extern char* ms_get_original_recipient(const char *);
extern char* ms_get_sender_address(const char *);

inline void mmm (char * const subject,
		 char const * const to,
		 char const * const cc,
		 const char* const rcpt[],
		 char * const message,
		 char const * const body_text) {
  GString *str = g_string_new ("MIME-Version: 1.0\r\nFrom: AEGEE Abuse Reporting <mail@aegee.org>\r\nSubject: ");
  g_string_append_printf (str, "%s\r\nTo: %s\r\n", subject, to);
  if (cc)
    g_string_append_printf (str, "CC: %s\r\n", cc);
  char const * const boundary_begin = strstr(body_text, "--") + 2;
  char *boundary = g_strndup(boundary_begin,
			     strchr(boundary_begin, '\r') - boundary_begin);
  g_string_append_printf(str, "Content-Type:  multipart/mixed; boundary=\"%s\"\r\n\r\n\r\n--%s\r\nContent-Type: text/plain\r\n\r\n%s\r\n\r\nKind regards\r\n  Your AEGEE Mail Team\r\n\r\n  http://mail.aegee.org\r\n  https://lists.aegee.org\r\n  email:mail@aegee.org\r\n  sip:8372@aegee.prg\r\n  +49 162 4091172(m)\r\n  +49 721 94193270\r\n%s", boundary, boundary, message, body_text);
  g_free(boundary);
  prdr_sendmail("mail@aegee.org", rcpt, str->str, "Date", "auto-generated");
  g_string_free (str, TRUE);
  g_free (subject);
  g_free (message);
}

static char const * const rcpt[] = {"mail@aegee.org", NULL};

inline void sentmail_not_list_related(char const * const email,
					     char const * const via,
					     char const * const body_text)
{
  char *message = g_strconcat("Hello Mail Team,\r\n\r\nthere was a complaint about an email sent to ", email, " via \"", via, "\". However \"", via, "\" is not an AEGEE Mailing list.  Please check the attached message (backup of it is available in the arf@aegee.org mailbox) and try to avoid such complaints in the future.", NULL);
  mmm (g_strconcat(email, " sent from ", via, NULL),
       "AEGEE Mail Team <mail@aegee.org>", NULL, rcpt, message, body_text);
}

inline void email_not_in_list(char const * const email,
				     char const * const list,
				     char const * const body_text)
{
  char *message = g_strconcat("Hello Mail Team,\r\n\r\nthe attached email to address ", email, " was sent via the mailing list ", list, ", but the email address is currently not subscribed to ", list, ". Please check what is wrong with the email address/list subscription.  There is a copy of the complaint in the arf@aegee.org mailbox.", NULL);
  mmm (g_strconcat(email, " not in ", list, NULL),
       "AEGEE Mail Team <mail@aegee.org>", NULL, rcpt, message, body_text);
}

inline void email_subscribed_to_list(const char * const email,
					    char const * const list,
					    char const * const recipients,
					    char const * const body_text)
{
  char *temp = g_ascii_strdown (list, -1);
  char *message = g_strconcat("Hello,\r\n\r\nthe subscriber (in CC:) ", email, " has complained about an email received over the mailing list ", list, " (see attachment).  In turn, ", email, " was removed from ", list, ".\r\n\r\n", email, " can be subscribed again to the list, if the recipient promises not to mark anymore messages as spam.  In order to re-subscribe, go to https://lists.aegee.org/", temp, " -> (on the right) Subscribe and Unsubscribe, or send an email to ", list, "-subscribe-request@lists.aegee.org.\r\n\r\nThe listowner can re-subscribe the address as usual over the listserv web interface.", NULL);
  g_free (temp);
  int j = 0, recipients_length = strlen(recipients), i;
  for (i=0; i < recipients_length; i++)
    if (recipients[i] == ',') j++;
  temp = g_strdup(recipients);
  char** rcpts = malloc((j+3) * sizeof(char*));
  j = 0;
  rcpts[0] = temp;
  for (i=0; i < recipients_length; i++)
    if (recipients[i] == ',') {
      temp[i] = '\0';
      rcpts[j++] = temp+i+1;
    }
  rcpts[j++] = (char*)email;
  rcpts[j++] = "mail@aegee.org";
  rcpts[j++] = NULL;
  char *to = g_strconcat("Listowners ", list, " <", list, "-request@lists.aegee.org>", NULL);
  mmm (g_strconcat("Removal of ", email, " from " , list, NULL),
       to, email, (const char * const*)rcpts, message, body_text);
  g_free (to);
  g_free (rcpts);
  g_free (temp);
}

inline void remove_email_from_list(char const * const email,
					  char const * const list,
					  char const * const body_text) {
  char *temp = g_strconcat ("sender<", email, ">listname<", list,">", NULL);
  prdr_list_insert ("log", "remove_email_from_list", "mod_arf_microsoft",
		    temp, 0);
  g_free (temp);
  char *subscribed = prdr_list_query ("listserv-check-subscriber", email, list);
  if (subscribed[0] == 'n') {
    email_not_in_list(email, list, body_text);
    return;
  }
  temp = g_strconcat(list, "|Owner", NULL);
  char *recipients = prdr_list_query ("listserv", temp, NULL);
  g_free (temp);
  email_subscribed_to_list (email, list, recipients, body_text);
  temp = g_strconcat (list, " ", email, NULL);
  prdr_list_remove ("listserv", temp, "q");
  g_free (temp);
  g_free (recipients);
  return;
}

int
mod_arf_LTX_prdr_mod_status (void* priv UNUSED)
{
  return MOD_BODY | MOD_RCPT;
}

static char* get_original_recipient(const char *body_text, int format) {
  if (format == 1)
    return ms_get_original_recipient(body_text);
  else
    return NULL;
}

static char* get_sender_address(const char *body_text, int format) {
  if (format == 1)
    return ms_get_sender_address(body_text);
  else
    return NULL;
}

int
mod_arf_LTX_prdr_mod_run (void* priv)
{
  if (prdr_get_stage (priv) & (MOD_RCPT | MOD_BODY)
      && (strcmp (prdr_get_recipient(priv), "arf+microsoft@aegee.org") != 0)
      && (strcmp (prdr_get_recipient(priv), "arf@aegee.org") != 0)
      && (strcmp (prdr_get_recipient(priv), "arf+scc_ms@aegee.org") != 0))
    return 0;

  if (prdr_get_stage (priv) & MOD_RCPT) {
    prdr_do_fail (priv);
    return -1;
  }

  if (prdr_get_stage (priv) & MOD_BODY) {
    int format = 0; //1 - microsoft, 2 - yahoo
    if (strcmp (prdr_get_recipient (priv), "arf+microsoft@aegee.org") == 0)
      format = 1;
    else if ((strcmp (prdr_get_recipient (priv), "arf@aegee.org") == 0)
	     && (strstr (prdr_get_envsender (priv), "arf=aegee.org@returns.bulk.yahoo.com") != NULL))
      format = 2;
    else return -1;
    if (format != 1) return 0;
    const GString *body_text = prdr_get_body (priv);

    char *original_recipient = get_original_recipient (body_text->str, format);
    char *sender_address = get_sender_address (body_text->str, format);
    //here comes address parsing, so only the real email address is left
    //MS sends the pure address, so no parsing is necessary
    printf("mod arf, format=%i, recipient %s, sender %s\n", format, original_recipient, sender_address);
    if (original_recipient && sender_address) {
      char *suffix = strstr (sender_address, "-L@LISTS.AEGEE.ORG");
      if (suffix) {
	char *listname = g_strndup (sender_address, sender_address-suffix+2);
	remove_email_from_list (sender_address, listname, body_text->str);
	g_free (listname);
      } else if (strcasestr (sender_address, "aegee") != NULL)
	sentmail_not_list_related (sender_address, original_recipient, body_text->str);
    }
    if (original_recipient) free (original_recipient);
    if (sender_address) free (sender_address);
  }
  return 0;
}

int
mod_arf_LTX_prdr_mod_equal (struct privdata* priv UNUSED, char* user1 UNUSED, char* user2 UNUSED)
{
  return 1;
}
