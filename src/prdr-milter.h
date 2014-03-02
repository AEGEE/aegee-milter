#ifndef PRDR_MILTER_H
#define PRDR_MILTER_H

extern "C" {
//include <stdio.h>
}

struct header{
  char *field, *value;
  //bit mask for status
  //00000000 - nothing/default
  //0xxxxxxx - added at position xxxxxxx
  //10xxxxxx - deleted from position xxxxxx from the begin
  //11xxxxxx - deleted from position xxxxxx from the end
  unsigned char status;  
};

//MODULE FLAGS
#define MOD_EHLO     1
#define MOD_MAIL     2
#define MOD_RCPT     4
#define MOD_HEADERS  8
#define MOD_BODY     16
#define MOD_FAILED   32
#define MOD_CLOSE    64
#define RCPT_PRDR_REJECTED 128
#define RCPT_NOTIFY_NEVER  256

#endif /* PRDR_MILTER_H */
