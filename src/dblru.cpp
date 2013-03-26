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
static atomic<unsigned long int> gStoredSize(0);
static atomic<unsigned long int> gTimestamp(0);
static atomic <unsigned long int> gFlushAllTimestamp(0);
static atomic <unsigned long int> gFlushAllTime(0);
static atomic<bool> gIsFlushAllSet(false);

/* ===  FUNCTION  ==============================================================
 *         Name:  dbSetFlushAll
 *         Desc:  This function sets up flush all when the request comes in from
 *                the client
 * =============================================================================
 */
void dbSetFlushAll (unsigned long int expiry)
{
  gIsFlushAllSet = true;
  time_t curSystemTime;
  time(&curSystemTime);

  gFlushAllTime = curSystemTime + expiry;
}		/* -----  end of function dbSetFlushAll  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  dbHandleFlushAll
 *         Desc:  This function is called each time any request comes in. It
 *                checks if it is time for flushing all data
 * =============================================================================
 */
void dbHandleFlushAll ()
{
  if (!gIsFlushAllSet)
  {
    return;
  }

  time_t curSystemTime;
  time(&curSystemTime);

  if (gFlushAllTime <= (unsigned long int)curSystemTime)
  {
    // Time to do a flush all
    unsigned long int tempTimestamp = ++gTimestamp;
    gFlushAllTimestamp = tempTimestamp;
    gIsFlushAllSet = false;
  }
}		/* -----  end of function dbHandleFlushAll  ----- */

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
  unsigned int tableNbr = 0;
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
    if ((getExpiryFromMap(valueIter) < (TimestampType)curSystemTime) ||
        (getTimestampFromMap(valueIter) < gFlushAllTimestamp))
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
  if (valueIter == gKeyToValueHashMap[hashTblNum].end())
  {
    return;
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
 *                Expiry should be in seconds. Flags and casUniq have to be 
 *                filled if they exist
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
  if (expiry != 0)
  {
    valueStr.expiry = curSystemTime + expiry;
  }
  else
  {
    valueStr.expiry = ULONG_MAX;
  }
  bool casCheck = true;
  static unsigned seed = curSystemTime;
  static mt19937_64 generator (seed);// mt19937_64 is a mersenne_twister_engine
  valueStr.casUniq.assign (to_string(generator()));
  if (casUniq.empty())
  {
    casCheck = false;
  }
  valueStr.flags.assign(flags);
  
  while (true) 
  {
    if ((gStoredSize + 2 * key.size() + 2 * sizeof(timestamp) + flags.size() +
          valueStr.casUniq.size() + sizeof(valueStr.expiry) + value.size()) 
        < gServMemLimit) 
    {
      break;
    }
    // If heap is used to maximum capacity, delete elements till we can fit in
    // new element
    
    gTimestampToKeyMutex[hashTblNum].lock();
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
        return MEMORY_FULL;
      }
      continue;
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
    return MEMORY_FULL;
  }

  TimestampToKeyStruct timestampToKeyDS;
  timestampToKeyDS.key.assign(key);
  if (get<1>(notAlreadyExists) != true)
  {
    // The entry already exists
    timestampToKeyDS.timestamp = getTimestampFromMap(get<0>(notAlreadyExists));
    int sizeDiff = sizeof(getExpiryFromMap(get<0>(notAlreadyExists))) - 
      sizeof(expiry) + getCasFromMap(get<0>(notAlreadyExists)).size() -
      valueStr.casUniq.size() + getFlagsFromMap(get<0>(notAlreadyExists)).size() -
      flags.size() + getValueFromMap(get<0>(notAlreadyExists)).size() -
      value.size();
    if (casCheck == true)
    {
      if ((getCasFromMap(get<0>(notAlreadyExists)).compare(casUniq) 
            != 0))
      {
        gKeyToValueMutex[hashTblNum].unlock();
        return EXIST;
      }
    }
    setExpiryToMap((get<0>(notAlreadyExists)), curSystemTime + expiry);
    setCasToMap((get<0>(notAlreadyExists)), valueStr.casUniq);
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
    if (casCheck == true)
    {
      // Element should not have been created
      gKeyToValueHashMap[hashTblNum].erase(key);
      gKeyToValueMutex[hashTblNum].unlock();
      return NOT_EXIST;
    }
    gKeyToValueMutex[hashTblNum].unlock();
    gStoredSize += 2 * key.size() + 2 * sizeof(timestamp) + 
      valueStr.casUniq.size() + valueStr.flags.size() + 
      sizeof(valueStr.expiry) + value.size();
  }

  timestampToKeyDS.timestamp = timestamp;
  gTimestampToKeyMutex[hashTblNum].lock();
  gTimestampToKeySet[hashTblNum].insert(timestampToKeyDS);
  gTimestampToKeyMutex[hashTblNum].unlock();
  return SUCCESS;
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
  unsigned int origHashTblNum = hashTblNum;

  ValueStruct valueStr;
  TimestampType timestamp = gTimestamp++;
  valueStr.value.assign(value);
  if (expiry != 0)
  {
    valueStr.expiry = curSystemTime + expiry;
  }
  else
  {
    valueStr.expiry = ULONG_MAX;
  }
  unsigned seed = curSystemTime;

  mt19937_64 generator (seed);// mt19937_64 is a mersenne_twister_engine
  valueStr.casUniq.assign (to_string(generator()));
  valueStr.flags.assign(flags);

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
        return MEMORY_FULL;
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
    return MEMORY_FULL;
  }

  TimestampToKeyStruct timestampToKeyDS;
  timestampToKeyDS.key.assign(key);
  if (get<1>(notAlreadyExists) != true)
  {
    // The entry already exists
    timestampToKeyDS.timestamp = getTimestampFromMap(get<0>(notAlreadyExists));
    if ((getExpiryFromMap(get<0>(notAlreadyExists)) 
          < (TimestampType)curSystemTime) ||
        (getTimestampFromMap(get<0>(notAlreadyExists)) < gFlushAllTimestamp))
    {
      // Expired
      setExpiryToMap((get<0>(notAlreadyExists)), curSystemTime + expiry);
      setCasToMap((get<0>(notAlreadyExists)), valueStr.casUniq);
      setValueToMap((get<0>(notAlreadyExists)), value);
      setFlagsToMap((get<0>(notAlreadyExists)), flags);
      gKeyToValueMutex[hashTblNum].unlock();
      gTimestampToKeyMutex[hashTblNum].lock();
      gTimestampToKeySet[hashTblNum].erase(timestampToKeyDS);
      timestampToKeyDS.timestamp = timestamp;
      gTimestampToKeySet[hashTblNum].insert(timestampToKeyDS);
      gTimestampToKeyMutex[hashTblNum].unlock();
      return SUCCESS;
    }
    gKeyToValueMutex[hashTblNum].unlock();
    return EXIST;
  }
  
  // New element was created
  gKeyToValueMutex[hashTblNum].unlock();
  gStoredSize += 2 * key.size() + 2 * sizeof(timestamp) + 
    valueStr.casUniq.size() + valueStr.flags.size() + 
    sizeof(valueStr.expiry) + value.size();

  timestampToKeyDS.timestamp = timestamp;
  gTimestampToKeyMutex[hashTblNum].lock();
  gTimestampToKeySet[hashTblNum].insert(timestampToKeyDS);
  gTimestampToKeyMutex[hashTblNum].unlock();
  return SUCCESS;
}		/* -----  end of function dbInsertElement  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  dbDeleteElement
 *  Description:  This function invalidates the entry in the database based on
 *                the expiry supplied
 * =============================================================================
 */
int dbDeleteElement (const string &key, const unsigned long int expiry)
{
  time_t curSystemTime;
  time(&curSystemTime);
  unsigned int hashTblNum = getHashTblNbrFromKey(key);

  gKeyToValueMutex[hashTblNum].lock();
  KeyToValueType::iterator it;
  if ((it = gKeyToValueHashMap[hashTblNum].find(key)) != 
      gKeyToValueHashMap[hashTblNum].end()) 
  {
    // Value exists
    if ((getExpiryFromMap(it) < (TimestampType)curSystemTime) ||
        (getTimestampFromMap(it) < gFlushAllTimestamp))
    {
      // Already deleted or expired
      gKeyToValueMutex[hashTblNum].unlock();
      return NOT_EXIST;
    }
    // Value has not already expired. Set new expiry
    if (expiry == 0)
    {
      // We delete only from this list. The timestamptokey entry will be 
      // deleted when inserting
      gKeyToValueHashMap[hashTblNum].erase(key);
      gKeyToValueMutex[hashTblNum].unlock();
      return SUCCESS;
    }
    setExpiryToMap(it, curSystemTime + expiry);
    gKeyToValueMutex[hashTblNum].unlock();
    return SUCCESS;
  }
  // Trying to delete a missing entry
  gKeyToValueMutex[hashTblNum].unlock();
  return NOT_EXIST;
}		/* -----  end of function dbDeleteElement  ----- */


/* ===  FUNCTION  ==============================================================
 *         Name:  dbGetElement
 *  Description:  This function gets the element from the map if it exists.
 * =============================================================================
 */
int dbGetElement (const string &key, string &flags, string &cas, string &value,
    unsigned long int &expiry)
{
  time_t curSystemTime;
  time(&curSystemTime);
  unsigned int hashTblNum = getHashTblNbrFromKey(key);

  TimestampType timestamp = gTimestamp++;
  TimestampToKeyStruct timestampToKeyDS;
  timestampToKeyDS.key.assign(key);

  gKeyToValueMutex[hashTblNum].lock();
  KeyToValueType::iterator it;
  if ((it = gKeyToValueHashMap[hashTblNum].find(key)) != 
      gKeyToValueHashMap[hashTblNum].end()) 
  {
    // Value exists
    if ((getExpiryFromMap(it) < (TimestampType)curSystemTime) ||
        (getTimestampFromMap(it) < gFlushAllTimestamp))
    {
      // Value expired
      gKeyToValueHashMap[hashTblNum].erase(key);
      gKeyToValueMutex[hashTblNum].unlock();
      return NOT_EXIST;
    }
    timestampToKeyDS.timestamp = getTimestampFromMap(it);
    value.assign(getValueFromMap(it));
    flags.assign(getFlagsFromMap(it));
    cas.assign(getCasFromMap(it));
    expiry = getExpiryFromMap(it) - (TimestampType)curSystemTime;
    // Updating new timestamp in map
    setTimestampToMap (it, timestamp);
    gKeyToValueMutex[hashTblNum].unlock();
    // Updating the timestamp so that the cache entry doesn't become stale soon
    gTimestampToKeyMutex[hashTblNum].lock();
    gTimestampToKeySet[hashTblNum].erase(timestampToKeyDS);
    timestampToKeyDS.timestamp = timestamp;
    gTimestampToKeySet[hashTblNum].insert(timestampToKeyDS);
    gTimestampToKeyMutex[hashTblNum].unlock();
    return SUCCESS;
  }
  // Value does not exist
  gKeyToValueMutex[hashTblNum].unlock();
  return NOT_EXIST;
}		/* -----  end of function dbGetElement  ----- */
