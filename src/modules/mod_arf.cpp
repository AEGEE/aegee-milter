#include <algorithm>
#include <cstring>
#include <iostream>
#include <ctype.h>
#include <mutex>
#include <stdio.h>
#include "src/core/AegeeMilter.hpp"
#include "src/core/Privdata.hpp"

enum class FORMAT : char {Microsoft, Yahoo};

static void mmm (const std::string& subject, const std::string& to,
		 const std::string& cc, const std::vector<std::string>& rcpt,
		 const std::string& message, GMimeObject* body_text) {
  //begin constructing first mime part
  const std::string& real_message = message + "\r\n\r\nKind regards\r\n  Your "
    "AEGEE Mail Team\r\n\r\n  http://mail.aegee.org\r\n  "
    "https://lists.aegee.org\r\n  email:mail@aegee.org\r\n  "
    "sip:8372@aegee.prg\r\n  +49 176 4171 0724(m)\r\n  +49 911 1313 7989\r\n";

  GMimeStream* gMimeStreamMem = g_mime_stream_mem_new_with_buffer
    (real_message.c_str (), real_message.size ());

  GMimeDataWrapper* gMimeDataWrapper = g_mime_data_wrapper_new_with_stream
    (gMimeStreamMem, GMIME_CONTENT_ENCODING_8BIT);
  g_object_unref (gMimeStreamMem);

  GMimePart* gMimePart = g_mime_part_new_with_type ("text", "plain");
  g_mime_part_set_content_object (gMimePart, gMimeDataWrapper);
  g_object_unref (gMimeDataWrapper);
  //end constructing first mime part

  GMimeMultipart* gMimeMultipart = g_mime_multipart_new_with_subtype ("mixed");
  g_mime_multipart_set_boundary (gMimeMultipart, NULL);
  g_mime_multipart_add (gMimeMultipart, (GMimeObject*)gMimePart);
  g_mime_multipart_add (gMimeMultipart, body_text);
  g_object_unref (gMimePart);

  GMimeMessage* gMimeMessage = g_mime_message_new (FALSE);
  g_mime_message_set_sender (gMimeMessage, "AEGEE Abuse Reporting <mail@aegee.org>");
  g_mime_message_set_subject (gMimeMessage, subject.c_str ());
  g_mime_message_set_date (gMimeMessage, time (NULL), 0);
  g_mime_message_add_recipient (gMimeMessage, GMIME_RECIPIENT_TYPE_TO, NULL, to.c_str ());
  if (!cc.empty()) g_mime_message_add_recipient
	     (gMimeMessage, GMIME_RECIPIENT_TYPE_CC, NULL, cc.c_str ());

  g_mime_object_append_header ((GMimeObject*)gMimeMessage,
			       "Auto-generated", "auto-submitted");
  g_mime_message_set_mime_part (gMimeMessage, (GMimeObject*)gMimeMultipart);
  g_object_unref(gMimeMultipart);

  std::string cmdline = AegeeMilter::Sendmail() + " -fmail@aegee.org";
  for (const std::string& it : rcpt) cmdline += " " + it;

  FILE *sm = popen (cmdline.c_str (), "w");
  GMimeStream* sendmail_pipe = g_mime_stream_file_new (sm);
  g_mime_stream_file_set_owner ((GMimeStreamFile*)sendmail_pipe, FALSE);
  g_mime_object_write_to_stream ((GMimeObject*) gMimeMessage, sendmail_pipe);
  pclose (sm);
  g_object_unref (gMimeMessage);
  g_object_unref (sendmail_pipe);
}

static const std::vector<std::string> rcpt { "mail@aegee.org" };

static void sentmail_not_list_related (const std::string& email,
				      const std::string& via,
				      GMimeObject* body_text)
{
  const std::string message = "Hello Mail Team,\r\n\r\nthere was a complaint about an email sent to " + email + " via \"" + via + "\". However \"" + via + "\" is not an AEGEE Mailing list.  Please check the attached message (backup of it is available in the arf@aegee.org mailbox) and try to avoid such complaints in the future.";
  mmm (email + " sent from " + via,
       "AEGEE Mail Team <mail@aegee.org>", "", rcpt, message, body_text);
}

static void email_not_in_list (const std::string& email,
			      const std::string& list,
			      GMimeObject* body_text)
{
  const std::string message = "Hello Mail Team,\r\n\r\nthe attached email to address " + email + " was sent via the mailing list " + list + ", but the email address is currently not subscribed to " + list + ". Please check what is wrong with the email address/list subscription.  There is a copy of the complaint in the arf@aegee.org mailbox.";
  mmm (email + " not in " + list,
       "AEGEE Mail Team <mail@aegee.org>", "", rcpt, message, body_text);
}

//ret is the list of (sub)lists, where email is subscribed
static void email_subscribed_to_list (const std::string& email,
				const std::string& list,
				const std::list<std::string>& ret,
				const std::string& recipients,
				GMimeObject* body_text)
{
  std::string temp;
  for (char it : list) temp += tolower (it);
  std::string message ="Hello,\r\n\r\nthe subscriber (in CC:) " + email
    + " has complained about an email received over the mailing list " + list
    + " (see attachment).  In turn, " + email + " was removed from ";
  if (ret.size () == 1) {
    message += ret.front ();
    if (ret.front () != list)
      message += " (Sub-List of " + list + ")";
    message  += ".\r\n\r\n"+ email + " can be subscribed again to the list";
  } else {
    for (const std::string it : ret)
      message += it + ", ";
    message.resize (message.size () - 2);
    const size_t pos = message.find_last_of (',');
    message[pos] = ' ';
    message.insert (pos + 1, "and");
    if (ret.front () == list)
      message += " (Main list " + list + " and sub-list"
	+ (ret.size() > 1 ? "s":"") + ")";
    else {
      message += ", sub-list";
      if (ret.size () > 1) message +=" s";
      message += " of " + list + ".  The subscription to the sub-lists implies"
	" membership in the main list.";
    }
    message+= ".\r\n\r\n"+ email + " can be subscribed again to the lists";
  }

  message += ", if the recipient promises not to mark anymore messages as "
    "spam,  received over the AEGEE mail server.  In order to re-subscribe, go"
    " to https://lists.aegee.org/" + temp + " -> (on the right) Subscribe and"
    " Unsubscribe, or send an email to " + list
    + "-subscribe-request@lists.aegee.org.\r\n\r\nThe listowner can "
    "re-subscribe the address as usual over the listserv web interface.";
  if (ret.size () > 1) {
    message += "\r\n\r\nThe procedure to subscribe to the "
      + std::string{ret.front () == list ? "sub-list" : "other sub-lists"}
      + " is analogue.";
  }

  std::vector<std::string> rcpts;
  int m = 0, recipients_length = recipients.length ();
  for (int i = 0; i < recipients_length; i++)
    if (recipients[i] == ',') {
      const std::string& temp {recipients.c_str () + m, (unsigned int)(i - m)};
      //insert in rcpts only email addresses, which are not yet there
      if (std::find(rcpts.begin(), rcpts.end(), temp) == rcpts.end())
	rcpts.emplace_back (temp);
      m = i;
    }
  rcpts.emplace_back (recipients.c_str () + m);
  rcpts.emplace_back (email);
  rcpts.emplace_back ("mail@aegee.org");
  std::string subject = "Removal of " + email + " from ";
  std::string to;
  if (ret.size() == 1) {
    subject += ret.front();
    to = "Listowners " + ret.front() + " <" + ret.front() + "-request@lists.aegee.org>";
  } else {
    for (const std::string& it : ret) {
      subject += it + ", ";
      to += "Listowners " + it + " <" + it + "-request@lists.aegee.org>, ";
    }
    subject.resize (subject.size() - 2);
    to.resize (to.size() - 1);
    size_t last_comma = subject.find_last_of (',');
    subject[last_comma] = ' ';
    subject.insert (last_comma+1, "and");
  }

  mmm (subject, to, email, rcpts, message, body_text);
}

//retrieves the lists, out of the current list and all its sublists, where the address is subscribed
static std::list<std::string> remove_email_from_list_deep (const std::string& email, const std::string& list) {
  std::list<std::string> ret;
  if (AegeeMilter::ListQuery ("listserv-check-subscriber", list, email)[0] == 'y') ret.push_back (list);

  const std::string& sub_lists = AegeeMilter::ListQuery ("listserv-get-sublists", list, "");
  if (!sub_lists.empty ()) {
    size_t pos = 0;
    do {
      size_t pos_old = pos;
      pos = sub_lists.find (' ', pos);
      const std::string& sub = sub_lists.substr (pos_old, pos);
      if (AegeeMilter::ListQuery ("listserv-check-subscriber", sub, email)[0] == 'y')
	ret.push_back (std::move(sub));
    } while (pos != std::string::npos && pos++);
  }
  return ret;
}

static void remove_email_from_list (const std::string& email,
				    const std::string& list,
				    GMimeObject* body_text) {
  AegeeMilter::ListInsert ("log", "remove_email_from_list", "mod_arf", "sender<" + email + ">listname<" + list + ">");
  const std::list<std::string>& ret = remove_email_from_list_deep (email, list);
  if (ret.empty ())
    email_not_in_list (email, list, body_text);
  else {
    std::string recipients;
    for (const std::string& s : ret) {
      recipients += AegeeMilter::ListQuery ("listserv", s + "|Owner", "") + ',';
      AegeeMilter::ListRemove ("listserv", s + " " + email, "q");
    }
    recipients.resize (recipients.size () - 1);
    email_subscribed_to_list (email, list, ret, recipients, body_text);
  }
}

class arf final : public SoModule {
  mutable std::mutex mutex;
 public:
  bool Run (Privdata& priv) const override {
    const std::string& recipient = priv.GetRecipient ();
    if (priv.GetStage () & (MOD_RCPT | MOD_BODY)
	&& recipient != "arf+microsoft@aegee.org"
	&& recipient != "arf@aegee.org"
	&& recipient != "arf+scc_ms@aegee.org")
      return true;

    if (priv.GetStage () & MOD_RCPT) {
      priv.DoFail();
      return false;
    }

    if (priv.GetStage () & MOD_BODY) {
      FORMAT format;
      if (recipient == "arf+microsoft@aegee.org"
	  && priv.GetEnvSender() == "staff@hotmail.com")
	format = FORMAT::Microsoft;
      else if (recipient == "arf@aegee.org"
	       && priv.GetEnvSender ().find ("arf=aegee.org@returns.bulk.yahoo.com") != std::string::npos)
	format = FORMAT::Yahoo;
      else return false;
      GMimeMessage* gMimeMessage = g_mime_parser_construct_message (priv.GetGMimeParser ());
      GMimeMultipart* gMimeMultipart =
	(GMimeMultipart*)gMimeMessage->mime_part;

      std::string original_recipient;
      std::string sender_address;
      GMimeObject* original_message;
      char *temp;

      switch (format) {
      case FORMAT::Microsoft:
	{
	original_message = g_mime_multipart_get_part (gMimeMultipart, 0);
	GMimeObject* nested_message = (GMimeObject*)g_mime_message_part_get_message ((GMimeMessagePart*)original_message);
	original_recipient = g_mime_object_get_header
	  (nested_message, "X-HmXmrOriginalRecipient");
	sender_address = g_mime_object_get_header
	  (nested_message, "Return-Path");
	break;
	}
      case FORMAT::Yahoo:
	GMimeObject* feedback_report = g_mime_multipart_get_part
	  (gMimeMultipart, 1);
	char* x = g_mime_object_to_string (feedback_report);
	temp = strstr (x, "\r\nOriginal-Mail-From: ");
	sender_address = {temp + 22, static_cast<long unsigned int>(strchr(temp + 22, '\r') - temp - 22)};
	temp = strstr (x, "\r\nOriginal-Rcpt-To: ");
	original_recipient = std::string {temp + 20, static_cast<long unsigned int>(strchr(temp + 20, '\r') - temp - 20)};
	original_message = g_mime_multipart_get_part (gMimeMultipart, 2);
	g_free (x);
	break;
      }

      sender_address = AegeeMilter::NormalizeEmail (sender_address);
      std::transform (sender_address.begin(), sender_address.end(), sender_address.begin(), ::toupper);
      if (!sender_address.find("OWNER-"))
	sender_address.erase (0, 6);
      //remove text from the first * until the @
      const size_t asterisk = sender_address.find('*');
      if (asterisk != std::string::npos)
	sender_address.erase (asterisk, sender_address.find('@', asterisk + 1) - asterisk);

      if (!original_recipient.empty () && !sender_address.empty ()) {
	size_t index = sender_address.find ("-L@LISTS.AEGEE.ORG");
	std::lock_guard<std::mutex> lg (mutex);
	if (index != std::string::npos) {
	  remove_email_from_list (original_recipient, std::string {sender_address, 0, index + 2}, original_message);
	} else if (strcasestr (sender_address.c_str (), "aegee")) {
	  sentmail_not_list_related (original_recipient, sender_address, original_message);
	} else
	  std::cout << time (NULL)
		    << " mod_arf:The impossible happened, sender={"
		    <<  sender_address << "}, recipient={"
		    << original_recipient << "}" << std::endl;
      }

      g_object_unref (gMimeMessage);
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
