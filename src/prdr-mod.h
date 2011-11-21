#ifndef PRDR_MOD_H
#define PRDR_MOD_H
#include "prdr-milter.h"

//API that shall be offered by the modules

//specifies when does the application want to be called, return values
int
prdr_mod_status (void*); /* required */
int
prdr_mod_run (void*); /* required */
//says if applying the filter to both users will deliver the same responce (=they have exactly the same settings). 0 means no, 1 is yes
int
prdr_mod_equal (struct privdata *priv, char*, char*); /* optional. If missing, it is asumed that the module acts on every recipient in the same way */
int
prdr_mod_init_msg (void*); /* optional */
int
prdr_mod_destroy_msg (void*); /* optional */

int
prdr_mod_init_rcpt (void*); /* optional */
int
prdr_mod_destroy_rcpt (void*); /* optional */

#endif
