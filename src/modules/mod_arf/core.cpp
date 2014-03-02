#include <cstring>
#include <iostream>
#include <ctype.h>
#include <mutex>
#include <stdio.h>
#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

extern std::string ms_get_original_recipient (const std::string&);
extern std::string ms_get_sender_address (const std::string&);

static void mmm (const std::string& subject, const std::string& to,
		 const std::string& cc, const std::vector<std::string>& rcpt,
		 const std::string& message, const std::string& body_text) {
  std::string str {"MIME-Version: 1.0\r\nFrom: AEGEE Abuse Reporting <mail@aegee.org>\r\nSubject: " + subject + "\r\nTo: " + to + "\r\n"};
  if (!cc.empty()) str += "CC: " + cc + "\r\n";
  const char* const boundary_begin = std::strstr (body_text.c_str(), "--") + 2;
  const std::string boundary {boundary_begin, static_cast<long unsigned int>(std::strchr (boundary_begin, '\r') - boundary_begin)};
  str += "Content-Type:  multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n\r\n--" + boundary + "\r\nContent-Type: text/plain\r\n\r\n" + message + "\r\n\r\nKind regards\r\n  Your AEGEE Mail Team\r\n\r\n  http://mail.aegee.org\r\n  https://lists.aegee.org\r\n  email:mail@aegee.org\r\n  sip:8372@aegee.prg\r\n  +49 176 9265 6190(m)\r\n  +49 911 1313 7989\r\n" + body_text;
  AegeeMilter::Sendmail ("mail@aegee.org", rcpt, str, "Date", "auto-generated");
}

static std::vector<std::string> rcpt { "mail@aegee.org" };

static void sentmail_not_list_related (const std::string& email,
				      const std::string& via,
				      const std::string& body_text)
{
  const std::string message = "Hello Mail Team,\r\n\r\nthere was a complaint about an email sent to " + email + " via \"" + via + "\". However \"" + via + "\" is not an AEGEE Mailing list.  Please check the attached message (backup of it is available in the arf@aegee.org mailbox) and try to avoid such complaints in the future.";
  mmm (email + " sent from " + via,
       "AEGEE Mail Team <mail@aegee.org>", "", rcpt, message, body_text);
}

static void email_not_in_list (const std::string& email,
			      const std::string& list,
			      const std::string& body_text)
{
  const std::string message = "Hello Mail Team,\r\n\r\nthe attached email to address " + email + " was sent via the mailing list " + list + ", but the email address is currently not subscribed to " + list + ". Please check what is wrong with the email address/list subscription.  There is a copy of the complaint in the arf@aegee.org mailbox.";
  mmm (email + " not in " + list,
       "AEGEE Mail Team <mail@aegee.org>", "", rcpt, message, body_text);
}

static void email_subscribed_to_list (const std::string& email,
				     const std::string& list,
				     const std::string& recipients,
				     const std::string& body_text)
{
  std::string temp;
  for (char it : list) temp += tolower (it);
  const std::string message ="Hello,\r\n\r\nthe subscriber (in CC:) " + email + " has complained about an email received over the mailing list " + list + " (see attachment).  In turn, " + email + " was removed from " + list + ".\r\n\r\n"+ email + " can be subscribed again to the list, if the recipient promises not to mark anymore messages as spam, received over the AEGEE mail server.  In order to re-subscribe, go to https://lists.aegee.org/" + temp + " -> (on the right) Subscribe and Unsubscribe, or send an email to " + list + "-subscribe-request@lists.aegee.org.\r\n\r\nThe listowner can re-subscribe the address as usual over the listserv web interface.";

  std::vector<std::string> rcpts;
  int m = 0, recipients_length = recipients.length ();
  for (int i = 0; i < recipients_length; i++)
    if (recipients[i] == ',') {
      rcpts.emplace_back (recipients.c_str () + m, i - m);
      m = i;
    }
  rcpts.emplace_back (recipients.c_str () + m);
  rcpts.emplace_back (email);
  rcpts.emplace_back ("mail@aegee.org");

  mmm ("Removal of " + email + " from " + list, "Listowners " + list + " <"
       + list + "-request@lists.aegee.org>", email, rcpts, message, body_text);
}

static void remove_email_from_list (const std::string& email,
				    const std::string& list,
				    const std::string& body_text) {
  AegeeMilter::ListInsert ("log", "remove_email_from_list", "mod_arf_microsoft", "sender<" + email + ">listname<" + list + ">");
  if (AegeeMilter::ListQuery ("listserv-check-subscriber", list, email)[0] == 'n')
    email_not_in_list (email, list, body_text);
  else {
    email_subscribed_to_list (email, list, AegeeMilter::ListQuery ("listserv", list + "|Owner", ""), body_text);
    AegeeMilter::ListRemove ("listserv", list + " " + email, "q");
  }
}

class arf final : public SoModule {
  mutable std::mutex mutex;
 public:
  bool Run (Privdata& priv) const override {
    const std::string& recipient = priv.GetRecipient();
    if (priv.GetStage() & (MOD_RCPT | MOD_BODY)
	&& recipient != "arf+microsoft@aegee.org"
	&& recipient != "arf@aegee.org"
	&& recipient != "arf+scc_ms@aegee.org")
      return true;

    if (priv.GetStage() & MOD_RCPT) {
      priv.DoFail();
      return false;
    }

    if (priv.GetStage () & MOD_BODY) {
      int format; //1 - microsoft, 2 - yahoo
      if (recipient == "arf+microsoft@aegee.org")
	format = 1;
      else if (recipient == "arf@aegee.org"
	       && priv.GetEnvSender () == "arf=aegee.org@returns.bulk.yahoo.com")
	format = 2;
      else return false;
      if (format != 1) return true;
      const std::string& body_text = priv.GetBody ();

      const std::string& original_recipient = (format == 1) ? ms_get_original_recipient (body_text) : "";
      const std::string& sender_address = (format == 1) ? ms_get_sender_address (body_text) : "";
      //here comes address parsing, so only the real email address is left
      //MS sends the pure address, so no parsing is necessary
      //    printf("mod arf, format=%i, recipient %s, sender %s\n", format, original_recipient, sender_address);
      if (!original_recipient.empty () && !sender_address.empty ()) {
	size_t index = sender_address.find ("-L@LISTS.AEGEE.ORG");
	std::lock_guard<std::mutex> lg (mutex);
	if (index != std::string::npos) {
	  remove_email_from_list (original_recipient, std::string {sender_address, 0, index + 2}, body_text);
	} else if (strcasestr (sender_address.c_str (), "aegee")) {
	  sentmail_not_list_related (sender_address, original_recipient, body_text);
	} else
	  std::cout << "mor_arf:The impossible happened" << std::endl;
      }
    }
    return true;
  }

  int Status (__attribute__((unused)) Privdata&) const override {
    return MOD_BODY | MOD_RCPT;
  }

  bool Equal (Privdata&, const Recipient&, const Recipient&) const override {
    return true;
  }
};

extern "C" SoModule* mod_arf_LTX_init () {
  return new arf ();
}
