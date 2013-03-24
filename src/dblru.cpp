/*==============================================================================
 *
 *       Filename:  dblru.cc
 *
 *    Description:  This file is the plug-in for the LRU cache replacement
 *                  strategy for memstashed
 *
 * =============================================================================
 */

#include "../inc/sconst.h"
#include "../inc/dblru.h"

// Compare function for timestampToKey
static bool timestampToKeyCompare (TimestampToKeyStruct first,
                                   TimestampToKeyStruct second) {
  return (first.timestamp < second.timestamp);
}
static bool (*timestampToKeyComparePtr)(TimestampToKeyStruct, 
                                 TimestampToKeyStruct) = timestampToKeyCompare;

// The data structures for the set and the map
static vector<TimestampToKeyType> gTimestampToKeySet(DB_MAX_HASH_TABLES, TimestampToKeyType(timestampToKeyComparePtr));
static KeyToValueType gKeyToValueHashMap[DB_MAX_HASH_TABLES];

static mutex gTimestampToKeyMutex[DB_MAX_HASH_TABLES];
static mutex gKeyToValueMutex[DB_MAX_HASH_TABLES];
static atomic<unsigned long int> gTimestamp(0);
static atomic<unsigned long int> gStoredSize(0);

/* ===  FUNCTION  ==============================================================
 *         Name:  getHashTblNbrFromKey
 *         Desc:  Gets the hash table number from the last 5 characters in the
 *                key. Should be modified if there is a pattern to the last 5
 *                characters of the key to ensure proper balancing. The entire
 *                key can be used for this, but that will be expensive
 * =============================================================================
 */
inline unsigned int getHashTblNbrFromKey (const string &key)
{
  unsigned int maxLen = key.size();
  unsigned int tableNbr;
  unsigned int i;
  if (maxLen < 5)
  {
    i = 0;
  }
  else
  {
    i = maxLen - 5;
  }
  for (; i < maxLen; i++) {
    tableNbr += key[i];
  }
  return tableNbr % DB_MAX_HASH_TABLES;
}		/* -----  end of function getHashTblNbrFromKey  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  getValueFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static string getValueFromMap (const KeyToValueType::iterator &it)
{
  return (get<0>((it->second))).value;
}		/* -----  end of function getValueFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  getExpiryFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static TimestampType getExpiryFromMap (const KeyToValueType::iterator &it)
{
  return (get<0>((it->second))).expiry;
}		/* -----  end of function getExpiryFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  getCasFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static string getCasFromMap (const KeyToValueType::iterator &it)
{
  return (get<0>(it->second)).casUniq;
}		/* -----  end of function getCasFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  getFlagsFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static string getFlagsFromMap (const KeyToValueType::iterator &it)
{
  return (get<0>(it->second)).flags;
}		/* -----  end of function getFlagsFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  getTimestampFromMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static TimestampType getTimestampFromMap (const KeyToValueType::iterator &it)
{
  return (get<1>((it->second)));
}		/* -----  end of function getTimestampFromMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  setValueToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setValueToMap (const KeyToValueType::iterator &it, 
    const string &value)
{
  (get<0>((it->second))).value.assign(value);
}		/* -----  end of function setValueToMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  setCasToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setCasToMap (const KeyToValueType::iterator &it, 
    const string &cas)
{
  (get<0>(it->second)).casUniq.assign(cas);
}		/* -----  end of function setCasToMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  setFlagsToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setFlagsToMap (const KeyToValueType::iterator &it, 
    const string &flags)
{
  (get<0>(it->second)).flags.assign(flags);
}		/* -----  end of function setFlagsToMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  setExpiryToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setExpiryToMap (const KeyToValueType::iterator &it, 
    const TimestampType &exp)
{
  (get<0>(it->second)).expiry = exp;
}		/* -----  end of function setExpiryToMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  setTimestampToMap
 *  Description:  Returns the value from the iterator
 * =============================================================================
 */
static void setTimestampToMap (const KeyToValueType::iterator &it, 
    const TimestampType &time)
{
  (get<1>((it->second))) = time;
}		/* -----  end of function setTimestampToMap  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  removeLRUElement
 *  Description:  Removes the least recently used element
 * =============================================================================
 */
static void removeLRUElement (unsigned int hashTblNum)
{
  time_t curSystemTime;
  time(&curSystemTime);
  gTimestampToKeyMutex[hashTblNum].lock();
  gKeyToValueMutex[hashTblNum].lock();
  auto timestampKeyOne = gTimestampToKeySet[hashTblNum].end();
  auto timestampKeyTwo = gTimestampToKeySet[hashTblNum].end();
  auto timestampKeyIter = gTimestampToKeySet[hashTblNum].begin();
  KeyToValueType::iterator valueIter;
  KeyToValueType::iterator valueOne;
  KeyToValueType::iterator valueTwo;

  while (timestampKeyIter != gTimestampToKeySet[hashTblNum].end())
  {
    TimestampToKeyStruct tempTimestampKey = *timestampKeyIter;
    valueIter = gKeyToValueHashMap[hashTblNum].find(tempTimestampKey.key);
    if (valueIter == gKeyToValueHashMap[hashTblNum].end())
    {
      // value has already been deleted
      TimestampToKeyType::iterator tempTimestampKeyIter = timestampKeyIter;
      timestampKeyIter++;
      gStoredSize -= (*tempTimestampKeyIter).key.size() + 
        sizeof((*tempTimestampKeyIter).timestamp);
      gTimestampToKeySet[hashTblNum].erase(tempTimestampKeyIter);
      gKeyToValueMutex[hashTblNum].unlock();
      gTimestampToKeyMutex[hashTblNum].unlock();
      return;
    }
    if (getExpiryFromMap(valueIter) < (TimestampType)curSystemTime)
    {
      // value has expired. Can be deleted
      TimestampToKeyType::iterator tempTimestampKeyIter = timestampKeyIter;
      timestampKeyIter++;
      gStoredSize -= 2 * (*tempTimestampKeyIter).key.size() + 
        2 * sizeof((*tempTimestampKeyIter).timestamp) + 
        (getCasFromMap(valueIter)).size() + 
        (getFlagsFromMap(valueIter)).size() +
        (getValueFromMap(valueIter)).size();
      gKeyToValueHashMap[hashTblNum].erase(valueIter);
      gTimestampToKeySet[hashTblNum].erase(tempTimestampKeyIter);
      gKeyToValueMutex[hashTblNum].unlock();
      gTimestampToKeyMutex[hashTblNum].unlock();
      return;
    }

    if (timestampKeyOne == gTimestampToKeySet[hashTblNum].end())
    {
      timestampKeyOne = timestampKeyIter;
      timestampKeyIter++;
      // We still need to fill the second one
      continue;
    }
    else
    {
      // Earlier iterations have filled the first position
      timestampKeyTwo = timestampKeyIter;
    }
    valueOne = gKeyToValueHashMap[hashTblNum].find((*timestampKeyOne).key);
    valueTwo = gKeyToValueHashMap[hashTblNum].find((*timestampKeyTwo).key);
    if (getValueFromMap(valueOne).size() < getValueFromMap(valueTwo).size())
    {
      valueIter = valueTwo;
      timestampKeyIter = timestampKeyTwo;
    }
    else
    {
      valueIter = valueOne;
      timestampKeyIter = timestampKeyOne;
    }
    break;
  }

  if (timestampKeyTwo == gTimestampToKeySet[hashTblNum].end())
  {
    // Only one element. Delete that.
    valueIter = valueOne;
    timestampKeyIter = timestampKeyOne;
  }
  gStoredSize -= 2 * (*timestampKeyIter).key.size() + 
    2 * sizeof((*timestampKeyIter).timestamp) + 
    (getCasFromMap(valueIter)).size() + 
    (getFlagsFromMap(valueIter)).size() +
    (getValueFromMap(valueIter)).size();
  gKeyToValueHashMap[hashTblNum].erase(valueIter);
  gTimestampToKeySet[hashTblNum].erase(timestampKeyIter);

  gKeyToValueMutex[hashTblNum].unlock();
  gTimestampToKeyMutex[hashTblNum].unlock();
  return;
}		/* -----  end of function removeLRUElement  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  dbInsertElement
 *  Description:  This function inserts an element into the data structures.
 *                Expiry should be in seconds.
 * =============================================================================
 */
int dbInsertElement (const string &key, const string &flags, 
    const string &casUniq, const string &value, const unsigned long int &expiry)
{
  // For expiry time
  time_t curSystemTime;
  time(&curSystemTime);
  unsigned int hashTblNum = getHashTblNbrFromKey(key);
  unsigned int origHashTblNum = hashTblNum;

  ValueStruct valueStr;
  TimestampType timestamp = gTimestamp++;
  valueStr.value.assign(value);
  valueStr.expiry = curSystemTime + expiry;
  if (!casUniq.empty())
  {
    valueStr.casUniq.assign(casUniq);
  }
  if (!flags.empty())
  {
    valueStr.flags.assign(casUniq);
  }
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
    
    gTimestampToKeyMutex[hashTblNum].lock();
    // In sorted order, so can just get begin
    if (gTimestampToKeySet[hashTblNum].size() <= 0)
    {
      gTimestampToKeyMutex[hashTblNum].unlock();
      if (hashTblNum != DB_MAX_HASH_TABLES - 1)
      {
        hashTblNum++;
      }
      else
      {
        hashTblNum = 0;
      }
      if (origHashTblNum == hashTblNum)
      {
        cout<<"Not enough memory to store even one element"<<endl;
        exit(0);
      }
    }
    else
    {
      gTimestampToKeyMutex[hashTblNum].unlock();
    }

    removeLRUElement(hashTblNum);
  }
  hashTblNum = origHashTblNum;

  gKeyToValueMutex[hashTblNum].lock();
  pair<KeyToValueType::iterator, bool> notAlreadyExists;
  try
  {
    notAlreadyExists = gKeyToValueHashMap[hashTblNum].emplace(
        key, pair<ValueStruct, TimestampType> (valueStr, timestamp));
  }
  catch (const bad_alloc& ba)
  {
    return FAILURE;
  }

  if (get<1>(notAlreadyExists) != true)
  {
    // The entry already exists
    int sizeDiff = sizeof(getExpiryFromMap(get<0>(notAlreadyExists))) - 
      sizeof(expiry) + getCasFromMap(get<0>(notAlreadyExists)).size() -
      casUniq.size() + getFlagsFromMap(get<0>(notAlreadyExists)).size() -
      flags.size() + getValueFromMap(get<0>(notAlreadyExists)).size() -
      value.size();
    setExpiryToMap((get<0>(notAlreadyExists)), curSystemTime + expiry);
    setCasToMap((get<0>(notAlreadyExists)), casUniq);
    setValueToMap((get<0>(notAlreadyExists)), value);
    setFlagsToMap((get<0>(notAlreadyExists)), flags);
    gKeyToValueMutex[hashTblNum].unlock();
    gTimestampToKeyMutex[hashTblNum].lock();
    gTimestampToKeySet[hashTblNum].erase(timestampToKeyDS);
    gTimestampToKeyMutex[hashTblNum].unlock();
    gStoredSize -= sizeDiff;
  }
  else
  {
    // New element was created
    gKeyToValueMutex[hashTblNum].unlock();
    gStoredSize += 2 * key.size() + 2 * sizeof(timestamp) +
      sizeof(valueStr.expiry) + value.size();
  }

  gTimestampToKeyMutex[hashTblNum].lock();
  gTimestampToKeySet[hashTblNum].insert(timestampToKeyDS);
  gTimestampToKeyMutex[hashTblNum].unlock();
  return EXIT_SUCCESS;
}		/* -----  end of function dbInsertElement  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  dbAddElement
 *  Description:  This function adds an element if it doesn't already exist
 *                Expiry should be in seconds.
 * =============================================================================
 */
int dbAddElement (const string &key, const string &flags, const string &casUniq,
    const string &value, const unsigned long int &expiry)
{
  // For expiry time
  time_t curSystemTime;
  time(&curSystemTime);
  unsigned int hashTblNum = getHashTblNbrFromKey(key);

  gKeyToValueMutex[hashTblNum].lock();

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
    gKeyToValueMutex[hashTblNum].unlock();
    gTimestampToKeyMutex[hashTblNum].lock();
    // In sorted order, so can just get begin
    if (gTimestampToKeySet[hashTblNum].size() <= 0)
    {
      cout<<"Not enough memory to store even a single element"<<endl;
      exit(0);
    }
    TimestampToKeyStruct tempTimestampKey = *(gTimestampToKeySet[hashTblNum].begin());

    gTimestampToKeySet[hashTblNum].erase(tempTimestampKey);
    gTimestampToKeyMutex[hashTblNum].unlock();
    gKeyToValueMutex[hashTblNum].lock();
    gKeyToValueHashMap[hashTblNum].erase(key);
    cout<<"Removed element"<<endl;
    // gStoredSize could have been updated already in another thread, but that's
    // OK.
    gStoredSize -= 2 * key.size() + 2 * sizeof(timestamp) +
      sizeof(valueStr.expiry) + value.size();
  }

  pair<KeyToValueType::iterator, bool> alreadyExists;
  alreadyExists = gKeyToValueHashMap[hashTblNum].emplace(
              key, pair<ValueStruct, TimestampType> (valueStr, timestamp));

  if (get<1>(alreadyExists) != true)
  {
    if (getExpiryFromMap(get<0>(alreadyExists)) > (TimestampType) curSystemTime)
    {
      // The entry already exists and is not expired
      gKeyToValueMutex[hashTblNum].unlock();
      return EXIT_FAILURE;
    }
    // Expired
    setExpiryToMap(get<0>(alreadyExists), curSystemTime + expiry);
    gKeyToValueMutex[hashTblNum].unlock();
    gTimestampToKeyMutex[hashTblNum].lock();
    gTimestampToKeySet[hashTblNum].erase(timestampToKeyDS);
    gTimestampToKeyMutex[hashTblNum].unlock();
  }
  else
  {
    // New element was created
    gStoredSize += 2 * key.size() + 2 * sizeof(timestamp) +
      sizeof(valueStr.expiry) + value.size();
    gKeyToValueMutex[hashTblNum].unlock();
  }

  gTimestampToKeyMutex[hashTblNum].lock();
  gTimestampToKeySet[hashTblNum].insert(timestampToKeyDS);
  gTimestampToKeyMutex[hashTblNum].unlock();
  return EXIT_SUCCESS;
}		/* -----  end of function dbInsertElement  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  dbDeleteElement
 *  Description:  This function invalidates the entry in the database based on
 *                the expiry supplied
 * =============================================================================
 */
int dbDeleteElement (const string &key, const unsigned long int &expiry)
{
  time_t curSystemTime;
  time(&curSystemTime);
  unsigned int hashTblNum = getHashTblNbrFromKey(key);

  gKeyToValueMutex[hashTblNum].lock();
  KeyToValueType::iterator it;
  if ((it = gKeyToValueHashMap[hashTblNum].find(key)) != gKeyToValueHashMap[hashTblNum].end()) 
  {
    // Value exists
    if (getExpiryFromMap(it) < (TimestampType)curSystemTime) 
    {
      // Already deleted
      gKeyToValueMutex[hashTblNum].unlock();
      return EXIT_FAILURE;
    }
    // Value has not already expired. Set new expiry
    setExpiryToMap(it, curSystemTime + expiry);
    gKeyToValueMutex[hashTblNum].unlock();
    return EXIT_SUCCESS;
  }
  // Trying to delete a missing entry
  gKeyToValueMutex[hashTblNum].unlock();
  return EXIT_FAILURE;
}		/* -----  end of function dbDeleteElement  ----- */


/* ===  FUNCTION  ==============================================================
 *         Name:  dbGetElement
 *  Description:  This function gets the element from the map if it exists.
 * =============================================================================
 */
int dbGetElement (const string &key, string &value)
{
  time_t curSystemTime;
  time(&curSystemTime);
  unsigned int hashTblNum = getHashTblNbrFromKey(key);

  TimestampType timestamp = gTimestamp++;
  TimestampToKeyStruct timestampToKeyDS;
  timestampToKeyDS.key.assign(key);

  gKeyToValueMutex[hashTblNum].lock();
  KeyToValueType::iterator it;
  if ((it = gKeyToValueHashMap[hashTblNum].find(key)) != gKeyToValueHashMap[hashTblNum].end()) 
  {
    // Value exists
    if (getExpiryFromMap(it) < (TimestampType)curSystemTime) 
    {
      // Value expired
      gKeyToValueMutex[hashTblNum].unlock();
      return EXIT_FAILURE;
    }
    value.assign(getValueFromMap(it));
    gKeyToValueMutex[hashTblNum].unlock();
    // Updating the timestamp so that the cache entry doesn't become stale soon
    gTimestampToKeyMutex[hashTblNum].lock();
    timestampToKeyDS.timestamp = getTimestampFromMap(it);
    gTimestampToKeySet[hashTblNum].erase(timestampToKeyDS);
    timestampToKeyDS.timestamp = timestamp;
    gTimestampToKeySet[hashTblNum].insert(timestampToKeyDS);
    gTimestampToKeyMutex[hashTblNum].unlock();
    gKeyToValueMutex[hashTblNum].lock();
    // Updating new timestamp in map
    setTimestampToMap (it, timestamp);
    gKeyToValueMutex[hashTblNum].unlock();
    return EXIT_SUCCESS;
  }
  // Value does not exist
  gKeyToValueMutex[hashTblNum].unlock();
  return EXIT_FAILURE;
}		/* -----  end of function dbGetElement  ----- */
