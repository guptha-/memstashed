/*==============================================================================
 *
 *       Filename:  dblru.cc
 *
 *    Description:  This file is the plug-in for the LRU cache replacement
 *                  strategy for memstashed
 *
 * =============================================================================
 */

#include "../inc/dblru.h"

// Compare function for timestampToKey
static bool timestampToKeyCompare (TimestampToKeyStruct first,
                                   TimestampToKeyStruct second) {
  return (first.timestamp < second.timestamp);
}
static bool (*timestampToKeyComparePtr)(TimestampToKeyStruct, 
                             TimestampToKeyStruct) = timestampToKeyCompare;

// The data structures for the set and the map
static TimestampToKeyType gTimestampToKeySet (timestampToKeyComparePtr);
static KeyToValueType gKeyToValueHashMap;

static mutex gTimestampToKeyMutex;
static mutex gKeyToValueMutex;
static atomic<unsigned long int> gTimestamp(0);
static atomic<unsigned long int> gStoredSize(0);


/* ===  FUNCTION  ==============================================================
 *         Name:  getValueFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
/*static valueType getValueFromMap (KeyToValueType::iterator it)
{
  return (get<0>((it->second))).value;
}	*/	/* -----  end of function getValueFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  getExpiryFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static TimestampType getExpiryFromMap (KeyToValueType::iterator it)
{
  return (get<0>((it->second))).expiry;
}		/* -----  end of function getExpiryFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  getTimestampFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static TimestampType getTimestampFromMap (KeyToValueType::iterator it)
{
  return (get<1>((it->second)));
}		/* -----  end of function getTimestampFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  dbInsertElement
 *  Description:  This function inserts an element into the data structures.
 *                Expiry should be in seconds.
 * =============================================================================
 */
int dbInsertElement (string key, string value, unsigned long int expiry) {

  // For expiry time
  time_t curSystemTime;
  time(&curSystemTime);

  gKeyToValueMutex.lock();
  KeyToValueType::iterator it;
  if ((it = gKeyToValueHashMap.find(key)) != 
                      gKeyToValueHashMap.end()) 
  {
    // Value already exists
    if (getExpiryFromMap(it) < (TimestampType)curSystemTime) 
    {
      // Value is still valid
      gKeyToValueMutex.unlock();
      return EXIT_FAILURE;
    }
    gKeyToValueHashMap.erase(it);
    gKeyToValueMutex.unlock();
    gTimestampToKeyMutex.lock();
    TimestampToKeyStruct tempTimeStampKey;
    tempTimeStampKey.key.assign(key);
    tempTimeStampKey.timestamp = getTimestampFromMap(it);
    gTimestampToKeySet.erase(tempTimeStampKey);
    gTimestampToKeyMutex.unlock();
    gKeyToValueMutex.lock();
  }

  ValueStruct valueStr;
  TimestampType timestamp = gTimestamp++;
  valueStr.value.assign(value);
  valueStr.expiry = curSystemTime + expiry;
  TimestampToKeyStruct timestampToKeyDS;
  timestampToKeyDS.timestamp = timestamp;
  timestampToKeyDS.key.assign(key);
  
  while (true) 
  {
    if ((gStoredSize + 2 * key.size() + 2 * sizeof(timestamp) + 
          sizeof(valueStr.expiry) + value.size()) < gServMemLimit) 
    {
      break;
    }
    // If heap is used to maximum capacity, delete elements till we can fit in
    // new element
    
    // Extra overhead, but can't be helped. Some other thread may be holding 
    // these locks in a different order.
    gKeyToValueMutex.unlock();
    gTimestampToKeyMutex.lock();
    // In sorted order, so can just get begin
    TimestampToKeyStruct tempTimeStampKey = *(gTimestampToKeySet.begin());


    gTimestampToKeySet.erase(tempTimeStampKey);
    gTimestampToKeyMutex.unlock();
    gKeyToValueMutex.lock();
    gKeyToValueHashMap.erase(key);
  }

  gKeyToValueHashMap.insert({{key, pair<ValueStruct, TimestampType>
                                     (valueStr, timestamp)}});

  gKeyToValueMutex.unlock();
  gTimestampToKeyMutex.lock();
  gTimestampToKeySet.insert(timestampToKeyDS);
  gTimestampToKeyMutex.unlock();
  return EXIT_SUCCESS;
}		/* -----  end of function dbInsertElement  ----- */
