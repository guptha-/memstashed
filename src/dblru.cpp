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
static ValueType getValueFromMap (KeyToValueType::iterator it)
{
  return (get<0>((it->second))).value;
}		/* -----  end of function getValueFromMap  ----- */

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
 *         Name:  setValueToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setValueToMap (KeyToValueType::iterator it, ValueType value)
{
  (get<0>((it->second))).value.assign (value);
}		/* -----  end of function setValueToMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  setExpiryToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setExpiryToMap (KeyToValueType::iterator it, TimestampType exp)
{
  (get<0>((it->second))).expiry = exp;
}		/* -----  end of function setExpiryToMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  setTimestampToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setTimestampToMap (KeyToValueType::iterator it, TimestampType time)
{
  (get<1>((it->second))) = time;
}		/* -----  end of function setTimestampToMap  ----- */

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
    if (gTimestampToKeySet.size() <= 0)
    {
      cout<<"Not enough memory to store even a single element"<<endl;
      exit(0);
    }
    TimestampToKeyStruct tempTimeStampKey = *(gTimestampToKeySet.begin());

    gTimestampToKeySet.erase(tempTimeStampKey);
    gTimestampToKeyMutex.unlock();
    gKeyToValueMutex.lock();
    gKeyToValueHashMap.erase(key);
    cout<<"Removed element"<<endl;
    gStoredSize -= 2 * key.size() + 2 * sizeof(timestamp) +
      sizeof(valueStr.expiry) + value.size();
  }

  pair<KeyToValueType::iterator, bool> alreadyExists;
  alreadyExists = gKeyToValueHashMap.emplace(
              key, pair<ValueStruct, TimestampType> (valueStr, timestamp));

  if (get<1>(alreadyExists) != true)
  {
    if (getExpiryFromMap(get<0>(alreadyExists)) > (TimestampType) curSystemTime)
    {
      // The entry already exists and is not expired
      gKeyToValueMutex.unlock();
      return EXIT_FAILURE;
    }
    // Expired
    get<0>((get<0>(alreadyExists))->second).expiry = curSystemTime + expiry;
    gKeyToValueMutex.unlock();
    gTimestampToKeyMutex.lock();
    gTimestampToKeySet.erase(timestampToKeyDS);
    gTimestampToKeyMutex.unlock();
  }
  else
  {
    // New element was created
    gStoredSize += 2 * key.size() + 2 * sizeof(timestamp) +
      sizeof(valueStr.expiry) + value.size();
    gKeyToValueMutex.unlock();
  }

  gTimestampToKeyMutex.lock();
  gTimestampToKeySet.insert(timestampToKeyDS);
  gTimestampToKeyMutex.unlock();
  return EXIT_SUCCESS;
}		/* -----  end of function dbInsertElement  ----- */


/* ===  FUNCTION  ==============================================================
 *         Name:  dbDeleteElement
 *  Description:  This function invalidates the entry in the database based on
 *                the expiry supplied
 * =============================================================================
 */
int dbDeleteElement (string key, unsigned long int expiry)
{
  time_t curSystemTime;
  time(&curSystemTime);

  gKeyToValueMutex.lock();
  KeyToValueType::iterator it;
  if ((it = gKeyToValueHashMap.find(key)) != gKeyToValueHashMap.end()) 
  {
    // Value exists
    if (getExpiryFromMap(it) < (TimestampType)curSystemTime) 
    {
      // Already deleted
      gKeyToValueMutex.unlock();
      return EXIT_FAILURE;
    }
    // Value has not already expired. Set new expiry
    (get<0>(it->second)).expiry = curSystemTime + expiry;
    gKeyToValueMutex.unlock();
    return EXIT_SUCCESS;
  }
  // Trying to delete a missing entry
  gKeyToValueMutex.unlock();
  return EXIT_FAILURE;
}		/* -----  end of function dbDeleteElement  ----- */


/* ===  FUNCTION  ==============================================================
 *         Name:  dbGetElement
 *  Description:  This function gets the element from the map if it exists.
 * =============================================================================
 */
int dbGetElement (string key, string &value)
{
  time_t curSystemTime;
  time(&curSystemTime);

  TimestampType timestamp = gTimestamp++;
  TimestampToKeyStruct timestampToKeyDS;
  timestampToKeyDS.key.assign(key);

  gKeyToValueMutex.lock();
  KeyToValueType::iterator it;
  if ((it = gKeyToValueHashMap.find(key)) != gKeyToValueHashMap.end()) 
  {
    // Value exists
    if (getExpiryFromMap(it) < (TimestampType)curSystemTime) 
    {
      // Value expired
      gKeyToValueMutex.unlock();
      return EXIT_FAILURE;
    }
    value.assign(getValueFromMap(it));
    gKeyToValueMutex.unlock();
    // Updating the timestamp so that the cache entry doesn't become stale soon
    gTimestampToKeyMutex.lock();
    timestampToKeyDS.timestamp = getTimestampFromMap(it);
    gTimestampToKeySet.erase(timestampToKeyDS);
    timestampToKeyDS.timestamp = timestamp;
    gTimestampToKeySet.insert(timestampToKeyDS);
    gTimestampToKeyMutex.unlock();
    gKeyToValueMutex.lock();
    // Updating new timestamp in map
    setTimestampToMap (it, timestamp);
    gKeyToValueMutex.unlock();
    return EXIT_SUCCESS;
  }
  // Value does not exist
  gKeyToValueMutex.unlock();
  return EXIT_FAILURE;
}		/* -----  end of function dbGetElement  ----- */
