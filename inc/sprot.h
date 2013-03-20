#ifndef INC_SERV_PROT_H
#define INC_SERV_PROT_H

#include "sinc.h"
using namespace std;

int dbInsertElement (const string &key, const string &value, 
    const unsigned long int &expiry);
int dbDeleteElement (const string &key, const unsigned long int &expiry);
int dbGetElement (const string &key, string &value);
void socketMain ();
int cmdProcessCommand (string &input, string &output);

#endif
