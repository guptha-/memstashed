#ifndef INC_SERV_PROT_H
#define INC_SERV_PROT_H

#include "sinc.h"
using namespace std;

int dbInsertElement (string &key, string &value, unsigned long int expiry);
int dbDeleteElement (string &key, unsigned long int expiry);
int dbGetElement (string &key, string &value);
void socketMain ();

#endif
