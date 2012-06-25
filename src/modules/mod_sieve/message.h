#ifndef MESSAGE_H
#define MESSAGE_H

#include <glib.h>
#include <sieve2.h>

typedef struct mod_sieve_Action mod_action_list_t;
struct sieve_local {
  GHashTable* hashTable;
  char **headers;
  int desired_stages;
  sieve2_context_t *sieve2_context;
  mod_action_list_t *actions;
  mod_action_list_t *last_action;
};

/* message.h
 * Larry Greenfield
 *
 * Copyright (c) 1994-2008 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Carnegie Mellon University
 *      Center for Technology Transfer and Enterprise Creation
 *      4615 Forbes Avenue
 *      Suite 302
 *      Pittsburgh, PA  15213
 *      (412) 268-7393, fax: (412) 268-7395
 *      innovation@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: message.h,v 1.20 2010/01/06 17:01:59 murch Exp $
 */


typedef enum {
    ACTION_NULL = -1,
    ACTION_NONE = 0,
    ACTION_REJECT,
    ACTION_FILEINTO,
    ACTION_KEEP,
    ACTION_REDIRECT,
    ACTION_DISCARD,
    ACTION_VACATION
} mod_action_t;


typedef struct mod_sieve_vacation {
    int min_response;		/* 0 -> defaults to 3 */
    int max_response;		/* 0 -> defaults to 90 */

    /* given a hash, say whether we've already responded to it in the last
       days days.  return SIEVE_OK if we SHOULD autorespond (have not already)
       or SIEVE_DONE if we SHOULD NOT. */
  //    sieve_callback *autorespond;

    /* mail the response */
  //    sieve_callback *send_response;
} mod_sieve_vacation_t;


/* sieve_imapflags: NULL -> defaults to \flagged */

typedef struct mod_sieve_redirect_context {
    char *addr;
} mod_sieve_redirect_context_t;

typedef struct mod_sieve_reject_context {
    char *msg;
} mod_sieve_reject_context_t;

typedef struct mod_sieve_fileinto_context {
    const char *mailbox;
} mod_sieve_fileinto_context_t;

#define SIEVE_HASHLEN 16

typedef struct mod_sieve_autorespond_context {
    char* hash;
    int days;
} mod_sieve_autorespond_context_t;

typedef struct mod_sieve_send_response_context {
    char *addr;
    char *fromaddr;
    char *msg;
    char *subj;
    int mime;
} mod_sieve_send_response_context_t;

/* invariant: always have a dummy element when free_action_list, param
   and vac_subj are freed.  none of the others are automatically freed.

   the do_action() functions should copy param */
struct mod_sieve_Action {
    mod_action_t a;
    union {
	mod_sieve_reject_context_t rej;
	mod_sieve_fileinto_context_t fil;
	mod_sieve_redirect_context_t red;
	struct {
	    /* addr, fromaddr, subj - freed! */
	    mod_sieve_send_response_context_t send;
	    mod_sieve_autorespond_context_t autoresp;
	} vac;
	struct {
	    const char *flag;
	} fla;
    } u;
    int cancel_keep;
    char *param;		/* freed! */
    struct mod_sieve_Action *next;
    char *vac_subj;		/* freed! */
    char *vac_msg;
    int vac_days;
};

#endif
