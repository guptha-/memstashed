#ifndef INC_SERV_INC_H
#define INC_SERV_INC_H

#include <iostream>
#include <string>
#include <atomic>
#include "sconst.h"
#include "sprot.h"

using namespace std;
extern atomic<unsigned int> gServMemLimit;
extern atomic<unsigned int> gServListPort;
extern atomic<unsigned int> gServWorkerThreads;
#endif
