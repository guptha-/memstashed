#ifndef INC_SERV_INC_H
#define INC_SERV_INC_H

#include <iostream>
#include <string>
#include <atomic>
#include "sconst.h"
using namespace std;
extern atomic<int> gServMemLimit;
extern atomic<int> gServListPort;
extern atomic<string> gServDefAddress;
extern atomic<int> gServWorkerThreads;
#endif
