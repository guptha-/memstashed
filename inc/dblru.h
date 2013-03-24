#ifndef INC_DB_LRU_H
#define INC_DB_LRU_H

#include <iostream>
#include <unordered_map>
#include <set>
#include <utility>
#include <ctime>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>

using namespace std;

extern atomic<unsigned int> gServMemLimit;

typedef string KeyType;
typedef unsigned long int TimestampType;

typedef struct 
{
    TimestampType expiry;
    string value;
    string flags;
    string casUniq;
} ValueStruct;

typedef struct 
{
  TimestampType timestamp;
  KeyType key;
} TimestampToKeyStruct;

// For eviction in constant time
typedef set<TimestampToKeyStruct, bool(*)(TimestampToKeyStruct, TimestampToKeyStruct)> TimestampToKeyType;

// For getting the value from the key - no need to order this
typedef unordered_map<KeyType,pair<ValueStruct,TimestampType> > KeyToValueType;
#endif
