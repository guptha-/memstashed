#ifndef INC_SERV_PROT_H
#define INC_SERV_PROT_H

#include "sinc.h"
using namespace std;

int dbInsertElement (const string &key, const string &flags, 
    const string &casUniq, const string &value, 
    const unsigned long int &expiry);
int dbAddElement (const string &key, const string &flags, 
    const string &casUniq, const string &value, 
    const unsigned long int &expiry);
int dbDeleteElement (const string &key, unsigned long int expiry);
int dbGetElement (const string &key, string &flags, string &cas, string &value,
    unsigned long int &expiry);
void socketMain ();
int cmdProcessCommand (string &input, string &output);
void dbSetFlushAll (unsigned long int expiry);
void dbHandleFlushAll ();

#endif
