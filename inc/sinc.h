#ifndef INC_SERV_INC_H
#define INC_SERV_INC_H

#include <iostream>
#include <string>
#include <atomic>
#include <string>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>

#include "sconst.h"
#include "sprot.h"

using namespace std;
extern atomic<unsigned int> gServMemLimit;
extern atomic<unsigned int> gServListPort;
extern atomic<unsigned int> gServWorkerThreads;
#endif
