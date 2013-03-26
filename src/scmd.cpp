/*==============================================================================
 *
 *       Filename:  cmd.cpp
 *
 *    Description:  This contains the actions to be taken upon the receipt of
 *                  each command.
 *
 * =============================================================================
 */

#include "../inc/sinc.h"
#include "../inc/scmd.h"


/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessCommon
 *  Description:  This function does the common processing
 *   Parameters:  isCas - If the command is cas
 *                isExistData - Do we need to check for flags, exp time
 * =============================================================================
 */
int cmdProcessCommon (const string &input, string &output, bool isCas, 
    bool isExistData, StorageStruct &store)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int tempSpace;
  unsigned int nextEndl = input.find("\r\n");
  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR key not found\r\n");
    return 0;
  }
  nextSpace++;
  store.key = input.substr(nextSpace,
      (tempSpace = input.find(' ', nextSpace)) - nextSpace);

  nextSpace = tempSpace;
  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR flags not found\r\n");
    return 0;
  }
  nextSpace++;
  if (isExistData == false) 
  {
    store.flags = input.substr(nextSpace, 
        (tempSpace = input.find(' ', nextSpace)) - nextSpace);

    nextSpace = tempSpace;
    if ((nextSpace == string::npos) || (nextSpace > nextEndl))
    {
      output.assign ("CLIENT_ERROR expiry time not found\r\n");
      return 0;
    }
    nextSpace++;
    string expTimeStr = input.substr(nextSpace, 
        (tempSpace = input.find(' ', nextSpace)) - nextSpace);
    try
    {
      store.expTime = stoul(expTimeStr);
      if (store.expTime > EXPIRY_THRESHOLD)
      {
        time_t curSystemTime;
        time(&curSystemTime);
        store.expTime -= curSystemTime;
        if (store.expTime < 0)
        {
          output.assign ("CLIENT_ERROR invalid expiry time\r\n");
          return 0;
        }
      }
    }
    catch (const invalid_argument& ia)
    {
      output.assign ("CLIENT_ERROR invalid expiry time\r\n");
      return 0;
    }

    nextSpace = tempSpace;
    if ((nextSpace == string::npos) || (nextSpace > nextEndl))
    {
      output.assign ("CLIENT_ERROR bytes not found\r\n");
      return 0;
    }
  }
  nextSpace++;
  string bytesStr = input.substr(nextSpace, 
      (tempSpace = input.find(' ', nextSpace)) - nextSpace);
  if ((isCas == true) && (tempSpace != string::npos))
  {
    nextSpace = tempSpace;
    if ((nextSpace == string::npos) || (nextSpace > nextEndl))
    {
      output.assign ("CLIENT_ERROR cas string not found\r\n");
      return 0;
    }
    nextSpace++;
    store.casStr= input.substr(nextSpace, 
        (tempSpace = input.find(' ', nextSpace)) - nextSpace);
  }
  if (tempSpace > nextEndl)
  {
    // noreply is not there and there is no space between <bytes> and \r\n
    // or <cas unique> and \r\n
    if (isCas == true)
    {
      store.casStr= input.substr(nextSpace, 
          (tempSpace = input.find("\r\n", nextSpace)) - nextSpace);
    }
    else
    {
      bytesStr = input.substr(nextSpace, 
          (tempSpace = input.find("\r\n", nextSpace)) - nextSpace);
    }
    store.noreply = false;
  }
  else
  {
    // noreply could be there or just a space between <bytes> and \r\n
    nextSpace = tempSpace;
    nextSpace++;
    if (nextSpace == nextEndl)
    {
      // No noreply found
      store.noreply = false;
    }
    else
    {
      string noReplyStr = input.substr(nextSpace, 
          (tempSpace = input.find("\r\n", nextSpace)) - nextSpace);
      if ((noReplyStr.compare("noreply") != 0) &&
          (noReplyStr.compare("noreply ") != 0))
      {
        output.assign ("CLIENT_ERROR junk found instead of \"noreply\"\r\n");
        return 0;
      }
      store.noreply = true;
    }
  }
  try
  {
    store.bytes = stoul(bytesStr);
  }
  catch (const invalid_argument& ia)
  {
    output.assign ("CLIENT_ERROR invalid number of bytes\r\n");
    return 0;
  }

  nextSpace = nextEndl;
  if ((nextSpace == string::npos) || (input.size() < nextSpace + 2))
  {
    // Nothing else is there to read in the string
    if (store.bytes != 0)
    {
      // We will try to read 'bytes + 2' bytes from the socket
      return (store.bytes + 2);
    }
  }
  if (store.bytes == 0)
  {
    return 0;
  }
  store.data = input.substr (nextSpace + 2, store.bytes);
  if (store.data.size() < store.bytes)
  {
    // We still need to fetch more data
    return (store.bytes + 2 - store.data.size());
  }
  // We have all the data we need. Do processing
  return 0;
}		/* -----  end of function cmdProcessCommon  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessSet
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessSet (const string &input, string &output)
{
  StorageStruct store;
  int ret = cmdProcessCommon (input, output, false, false, store);
  if (output.size() > 0)
  {
    // Some error in input
    return 0;
  }
  if (ret > 0)
  {
    // More data needed to process
    return ret;
  }

  if (dbInsertElement(store.key, store.flags, store.casStr, store.data, 
        store.expTime) == MEMORY_FULL)
  {
    cout<<"Could not set element"<<endl;
    if (store.noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element\r\n");
    }
    return 0;
  }
  if (store.noreply == false)
  {
    output.assign("STORED\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessSet  ----- */


/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessAdd
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessAdd (const string &input, string &output)
{
  StorageStruct store;
  int ret = cmdProcessCommon (input, output, false, false, store);
  if (output.size() > 0)
  {
    // Some error in input
    return 0;
  }
  if (ret > 0)
  {
    // More data needed to process
    return ret;
  }

  // We have everything to do the actual processing.
  ret = dbAddElement(store.key, store.flags, store.casStr, store.data, 
        store.expTime);
  if (ret == MEMORY_FULL)
  {
    cout<<"Could not add element"<<endl;
    if (store.noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element\r\n");
    }
    return 0;
  }
  else if (ret == EXIST)
  {
    cout<<"Could not add element"<<endl;
    if (store.noreply == false)
    {
      output.assign("NOT_STORED\r\n");
    }
    return 0;
  }
  if (store.noreply == false)
  {
    output.assign("STORED\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessAdd  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessReplace
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessReplace (const string &input, string &output)
{
  StorageStruct store;
  int ret = cmdProcessCommon (input, output, false, false, store);
  if (output.size() > 0)
  {
    // Some error in input
    return 0;
  }
  if (ret > 0)
  {
    // More data needed to process
    return ret;
  }
  
  // We have everything to do the actual processing.
  string flags, cas, value;
  unsigned long int expiry;
  if (dbGetElement(store.key, flags, cas, value, expiry) != SUCCESS)
  {
    // Data is not already present
    cout<<"Could not replace element"<<endl;
    if (store.noreply == false)
    {
      output.assign("NOT_STORED\r\n");
    }
    return 0;
  }

  if (dbInsertElement(store.key, store.flags, store.casStr, store.data, 
        store.expTime) == MEMORY_FULL)
  {
    cout<<"Could not replace element"<<endl;
    if (store.noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element" "\r\n");
    }
    return 0;
  }
  if (store.noreply == false)
  {
    output.assign("STORED\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessReplace  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessAppend
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessAppend (const string &input, string &output)
{
  StorageStruct store;
  int ret = cmdProcessCommon (input, output, false, true, store);
  if (output.size() > 0)
  {
    // Try reading flags and exptime too. We will ignore them anyway
    output.clear();
    ret = cmdProcessCommon (input, output, false, false, store);
    if (output.size() > 0)
    {
      // Error in input
      return 0;
    }
  }
  if (ret > 0)
  {
    // More data needed to process
    return ret;
  }

  // We have everything to do the actual processing.
  string value;
  if (dbGetElement(store.key, store.flags, store.casStr, value, store.expTime)
      != SUCCESS)
  {
    // Data is not already present
    cout<<"Could not append element"<<endl;
    if (store.noreply == false)
    {
      output.assign("NOT_STORED\r\n");
    }
    return 0;
  }

  value.append(store.data);
  // This makes sure dbInsertElement generates a new cas value
  store.casStr.clear();

  if (dbInsertElement(store.key, store.flags, store.casStr, value, 
        store.expTime) == MEMORY_FULL)
  {
    cout<<"Could not append element"<<endl;
    if (store.noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element" "\r\n");
    }
    return 0;
  }
  if (store.noreply == false)
  {
    output.assign("STORED\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessAppend  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessPrepend
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessPrepend (const string &input, string &output)
{
  StorageStruct store;
  int ret = cmdProcessCommon (input, output, false, true, store);
  if (output.size() > 0)
  {
    // Try reading flags and exptime too. We will ignore them anyway
    output.clear();
    ret = cmdProcessCommon (input, output, false, false, store);
    if (output.size() > 0)
    {
      // Error in input
      return 0;
    }
  }
  if (ret > 0)
  {
    // More data needed to process
    return ret;
  }

  // We have everything to do the actual processing.
  string value;
  if (dbGetElement(store.key, store.flags, store.casStr, value, store.expTime)
      != SUCCESS)
  {
    // Data is not already present
    cout<<"Could not prepend element"<<endl;
    if (store.noreply == false)
    {
      output.assign("NOT_STORED\r\n");
    }
    return 0;
  }

  store.data.append(value);
  // This makes sure dbInsertElement generates a new cas value
  store.casStr.clear();

  if (dbInsertElement(store.key, store.flags, store.casStr, store.data, 
        store.expTime) == MEMORY_FULL)
  {
    cout<<"Could not prepend element"<<endl;
    if (store.noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element" "\r\n");
    }
    return 0;
  }
  if (store.noreply == false)
  {
    output.assign("STORED\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessPrepend  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessCas
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessCas (const string &input, string &output)
{
  StorageStruct store;
  int ret = cmdProcessCommon (input, output, true, false, store);
  if (output.size() > 0)
  {
    // Some error in input
    return 0;
  }
  if (ret > 0)
  {
    // More data needed to process
    return ret;
  }

  // We have everything to do the actual processing.
  ret = dbInsertElement(store.key, store.flags, store.casStr, store.data, 
      store.expTime);
  if (ret == MEMORY_FULL)
  {
    cout<<"Could not cas element"<<endl;
    if (store.noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element" "\r\n");
    }
    return 0;
  }
  else if (ret == EXIST)
  {
    cout<<"Could not cas element"<<endl;
    if (store.noreply == false)
    {
      output.assign("EXISTS\r\n");
    }
    return 0;
  }
  else if (ret == NOT_EXIST)
  {
    cout<<"Could not cas element"<<endl;
    if (store.noreply == false)
    {
      output.assign("NOT_FOUND\r\n");
    }
    return 0;
  }
  if (store.noreply == false)
  {
    output.assign("STORED\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessCas  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessGet
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessGet (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int tempSpace;
  unsigned int nextEndl = input.find("\r\n");
  vector<string> keys;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    // Not a single key found
    output.assign ("CLIENT_ERROR no key found\r\n");
    return 0;
  }
  while (true)
  {
    nextSpace++;
    tempSpace = input.find(' ', nextSpace);
    if ((tempSpace == string::npos) || (tempSpace > nextEndl))
    {
      break;
    }
    keys.push_back (input.substr(nextSpace, tempSpace - nextSpace));
    nextSpace = tempSpace;
  }

  keys.push_back (input.substr(nextSpace, nextEndl - nextSpace));

  // Do processing now
  for (auto &key: keys)
  {
    string cas, flags, value;
    unsigned long int expTime;
    if (dbGetElement(key, flags, cas, value, expTime) != SUCCESS)
    {
      // Data is not present
      continue;
    }
    output.append("VALUE ");
    output.append(key);
    output.append(" ");
    output.append(flags);
    output.append(" ");
    output.append(to_string(value.length()));
    output.append("\r\n");
    output.append(value);
    output.append("\r\n");
  }
  output.append("END\r\n");

  return 0;
}		/* -----  end of function cmdProcessGet  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessGets
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessGets (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int tempSpace;
  unsigned int nextEndl = input.find("\r\n");
  vector<string> keys;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    // Not a single key found
    output.assign ("CLIENT_ERROR no key found\r\n");
    return 0;
  }
  while (true)
  {
    nextSpace++;
    tempSpace = input.find(' ', nextSpace);
    if ((tempSpace == string::npos) || (tempSpace > nextEndl))
    {
      break;
    }
    keys.push_back (input.substr(nextSpace, tempSpace - nextSpace));
    nextSpace = tempSpace;
  }

  keys.push_back (input.substr(nextSpace, nextEndl - nextSpace));

  // Do processing now
  for (auto &key: keys)
  {
    string cas, flags, value;
    unsigned long int expTime;
    if (dbGetElement(key, flags, cas, value, expTime) != SUCCESS)
    {
      // Data is not present
      continue;
    }
    output.append("VALUE ");
    output.append(key);
    output.append(" ");
    output.append(flags);
    output.append(" ");
    output.append(to_string(value.length()));
    output.append(" ");
    output.append(cas);
    output.append("\r\n");
    output.append(value);
    output.append("\r\n");
  }
  output.append("END\r\n");

  return 0;
}		/* -----  end of function cmdProcessGets  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessDelete
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessDelete (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int tempSpace;
  unsigned int nextEndl = input.find("\r\n");
  string key;
  string noReplyStr;
  bool noreply = true;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR key not found\r\n");
    return 0;
  }
  nextSpace++;
  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    noreply = false;
    tempSpace = nextEndl;
  }
  key = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;

  if (noreply == true)
  {
    noReplyStr = input.substr(nextSpace, nextEndl - nextSpace);
    if (noReplyStr.compare("noreply") != 0)
    {
      // There should be no junk after the key and before noreply
      output.assign ("CLIENT_ERROR junk found after key\r\n");
      return 0;
    }
  }

  unsigned long int expiry = 0;
  // We have everything to do the actual processing.
  if (dbDeleteElement(key, expiry) == NOT_EXIST)
  {
    if (noreply == false)
    {
      output.assign("NOT_FOUND\r\n");
    }
  }
  else
  {
    if (noreply == false)
    {
      output.assign("DELETED\r\n");
    }
  }
  return 0;
}		/* -----  end of function cmdProcessDelete  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessIncr
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessIncr (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int tempSpace;
  unsigned int nextEndl = input.find("\r\n");
  string key, value, noReplyStr;
  bool noreply = true;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR key not found\r\n");
    return 0;
  }
  nextSpace++;
  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR value not found\r\n");
    return 0;
  }
  key = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;

  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    noreply = false;
    tempSpace = nextEndl;
  }
  value = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;

  if (noreply == true)
  {
    noReplyStr = input.substr(nextSpace, nextEndl - nextSpace);
    if (noReplyStr.compare("noreply") != 0)
    {
      noreply = false;
    }
  }

  // We have everything to do the actual processing.
  string storedValue, flags, cas;
  unsigned long int expTime;
  if (dbGetElement(key, flags, cas, storedValue, expTime)
      != SUCCESS)
  {
    // Data is not already present
    cout<<"Could not incr element"<<endl;
    if (noreply == false)
    {
      output.assign("NOT_FOUND\r\n");
    }
    return 0;
  }

  unsigned long int inputValue = stoul(value, nullptr, 10);
  unsigned long int oldValue = stoul(storedValue, nullptr, 10);
  if (inputValue == 0)
  {
    if (noreply == false)
    {
      output.assign("CLIENT_ERROR invalid format for value\r\n");
      return 0;
    }
  }
  inputValue += oldValue;
  storedValue.assign(to_string(inputValue));
  // This makes sure dbInsertElement generates a new cas value
  cas.clear();

  if (dbInsertElement(key, flags, cas, storedValue, expTime) 
      == MEMORY_FULL)
  {
    cout<<"Could not incr element"<<endl;
    if (noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element" "\r\n");
    }
    return 0;
  }

  if (noreply == false)
  {
    output.assign(storedValue);
    output.append("\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessIncr  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessDecr
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessDecr (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int tempSpace;
  unsigned int nextEndl = input.find("\r\n");
  string key, value, noReplyStr;
  bool noreply = true;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR key not found\r\n");
    return 0;
  }
  nextSpace++;
  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR value not found\r\n");
    return 0;
  }
  key = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;

  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    noreply = false;
    tempSpace = nextEndl;
  }
  value = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;

  if (noreply == true)
  {
    noReplyStr = input.substr(nextSpace, nextEndl - nextSpace);
    if (noReplyStr.compare("noreply") != 0)
    {
      noreply = false;
    }
  }

  // We have everything to do the actual processing.
  string storedValue, flags, cas;
  unsigned long int expTime;
  if (dbGetElement(key, flags, cas, storedValue, expTime)
      != SUCCESS)
  {
    // Data is not already present
    cout<<"Could not decr element"<<endl;
    if (noreply == false)
    {
      output.assign("NOT_FOUND\r\n");
    }
    return 0;
  }

  unsigned long int inputValue = stoul(value, nullptr, 10);
  unsigned long int oldValue = stoul(storedValue, nullptr, 10);
  if (inputValue == 0)
  {
    if (noreply == false)
    {
      output.assign("CLIENT_ERROR invalid format for value\r\n");
      return 0;
    }
  }
  if (oldValue <= inputValue)
  {
    inputValue = 0;
  }
  else
  {
    inputValue = oldValue - inputValue;
  }
  storedValue.assign(to_string(inputValue));
  // This makes sure dbInsertElement generates a new cas value
  cas.clear();

  if (dbInsertElement(key, flags, cas, storedValue, expTime) 
      == MEMORY_FULL)
  {
    cout<<"Could not decr element"<<endl;
    if (noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element" "\r\n");
    }
    return 0;
  }
  if (noreply == false)
  {
    output.assign(storedValue);
    output.append("\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessDecr  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessTouch
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessTouch (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int tempSpace;
  unsigned int nextEndl = input.find("\r\n");
  unsigned int expTime;
  string key, expTimeStr, noReplyStr;
  bool noreply = true;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR key not found\r\n");
    return 0;
  }
  nextSpace++;
  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR value not found\r\n");
    return 0;
  }
  key = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;

  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    noreply = false;
    tempSpace = nextEndl;
  }
  expTimeStr = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;
  try
  {
    expTime = stoul(expTimeStr);
  }
  catch (const invalid_argument& ia)
  {
    output.assign ("CLIENT_ERROR invalid expiry time\r\n");
    return 0;
  }

  if (noreply == true)
  {
    noReplyStr = input.substr(nextSpace, nextEndl - nextSpace);
    if (noReplyStr.compare("noreply") != 0)
    {
      noreply = false;
    }
  }
  // We have everything to do the actual processing.
  string cas, flags, value;
  unsigned long int expiry;
  if (dbGetElement(key, flags, cas, value, expiry) != SUCCESS)
  {
    // Data is not already present
    cout<<"Could not touch element"<<endl;
    if (noreply == false)
    {
      output.assign("NOT_FOUND\r\n");
    }
    return 0;
  }

  // Cas is not updated, but just checked against the prev value
  if (dbInsertElement(key, flags, cas, value, expTime) == MEMORY_FULL)
  {
    cout<<"Could not touch element"<<endl;
    if (noreply == false)
    {
      output.assign("SERVER_ERROR not enough memory to store even single "
          "element" "\r\n");
    }
    return 0;
  }
  if (noreply == false)
  {
    output.assign("TOUCHED\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessTouch  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessFlushAll
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessFlushAll (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int nextEndl = input.find("\r\n");
  unsigned int tempSpace;
  unsigned int expTime = 0;
  string expTimeStr;
  bool noreply = true;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    // There are no options to the flush_all
    dbSetFlushAll(expTime);
    output.assign("OK\r\n");
    return 0;
  }

  nextSpace++;

  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    // We know there is only 1 additional parameter, but we don't know if it is 
    // no reply or the expiry
    expTimeStr = input.substr(nextSpace, nextEndl - nextSpace);
    if (expTimeStr.compare("noreply") == 0)
    {
      // No expiry time, and no reply is set
      dbSetFlushAll(expTime);
      return 0;
    }
    try
    {
      expTime = stoul(expTimeStr);
    }
    catch (const invalid_argument& ia)
    {
      output.assign ("CLIENT_ERROR invalid expiry time\r\n");
      return 0;
    }
    // We now have the exp time, and there is no noreply
    dbSetFlushAll(expTime);
    output.assign("OK\r\n");
    return 0;
  }
  // There are 2 parameters
  expTimeStr = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;
  try
  {
    expTime = stoul(expTimeStr);
  }
  catch (const invalid_argument& ia)
  {
    output.assign ("CLIENT_ERROR invalid expiry time\r\n");
    return 0;
  }

  string noReplyStr = input.substr(nextSpace, nextEndl - nextSpace);
  if (noReplyStr.compare("noreply") != 0)
  {
    output.assign ("CLIENT_ERROR invalid noreply field\r\n");
    return 0;
  }

  // We have everything to do the actual processing.
  dbSetFlushAll(expTime);
  if (noreply == false)
  {
    output.assign("OK\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessFlushAll  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessVersion
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessVersion (const string &input, string &output)
{
  // We have everything to do the actual processing.
  if (input.size() > 9)
  {
    output.assign("CLIENT_ERROR version does not accept options\r\n");
    return 0;
  }
  output.assign(VERSION_STRING);
  return 0;
}		/* -----  end of function cmdProcessVersion  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessVerbosity
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessVerbosity (const string &input, string &output)
{
  unsigned int nextSpace = input.find(' ');
  unsigned int nextEndl = input.find("\r\n");
  unsigned int tempSpace;
  unsigned int verb;
  string verbStr;
  bool noreply = true;

  if ((nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR exp time not found\r\n");
    return 0;
  }
  nextSpace++;

  tempSpace = input.find(' ', nextSpace);
  if ((tempSpace == string::npos) || (tempSpace > nextEndl))
  {
    noreply = false;
    tempSpace = nextEndl;
  }
  verbStr = input.substr(nextSpace, tempSpace - nextSpace);
  nextSpace = tempSpace + 1;
  try
  {
    verb = stoul(verbStr);
  }
  catch (const invalid_argument& ia)
  {
    output.assign ("CLIENT_ERROR invalid expiry time\r\n");
    return 0;
  }

  if (noreply == true)
  {
    string noReplyStr = input.substr(nextSpace, nextEndl - nextSpace);
    if (noReplyStr.compare("noreply") != 0)
    {
      noreply = false;
    }
  }

  // We have everything to do the actual processing.
  // We don't actually have anything for verbosity. Just a mockup
  verb = verb + 1;
  if (noreply == false)
  {
    output.assign("OK\r\n");
  }
  return 0;
}		/* -----  end of function cmdProcessVersion  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessQuit
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessQuit (const string &input, string &output)
{
  if (input.size() > 6)
  {
    output.assign("CLIENT_ERROR quit has no options\r\n");
    return 0;
  }
  return -1;
}		/* -----  end of function cmdProcessQuit  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessStats
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessStats (const string &input, string &output)
{
  // We have everything to do the actual processing.
  if (input.size() > 7)
  {
    output.assign("CLIENT_ERROR quit has no options\r\n");
    return 0;
  }
  output.assign("STAT ");
  output.assign("pid ");
  output.assign(to_string(getpid()));
  output.assign("\r\n");

  output.assign("STAT ");
  output.assign("time ");
  time_t curSystemTime;
  time(&curSystemTime);
  output.assign(to_string(curSystemTime));
  output.assign("\r\n");

  output.assign("STAT ");
  output.assign("version ");
  output.assign(VERSION_STRING);

  output.assign("STAT ");
  output.assign("threads ");
  output.assign(to_string(gServWorkerThreads));
  output.assign("\r\n");
  output.assign("END\r\n");
  return 0;
}		/* -----  end of function cmdProcessStats  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessCommand
 *  Description:  This is the entry point. Upon receiving a command from the
 *                client, this is where the flow begins. The response to be sent
 *                to the client is returned from here
 * =============================================================================
 */
int cmdProcessCommand (string &input, string &output)
{
  // Get the command word from the input
  unsigned int nextSpace = input.find(' ');
  if (nextSpace == string::npos)
  {
    // Commands like stats, version and quit
    nextSpace = input.find("\r\n");
  }
  string command = input.substr(0, nextSpace);

  // This just sees if it is time for a flush all
  dbHandleFlushAll();

  output.clear();
  if (command.compare("set") == 0)
  {
    return cmdProcessSet (input, output);
  }
  else if (command.compare("add") == 0)
  {
    return cmdProcessAdd (input, output);
  }
  else if (command.compare("replace") == 0)
  {
    return cmdProcessReplace (input, output);
  }
  else if (command.compare("append") == 0)
  {
    return cmdProcessAppend (input, output);
  }
  else if (command.compare("prepend") == 0)
  {
    return cmdProcessPrepend (input, output);
  }
  else if (command.compare("cas") == 0)
  {
    return cmdProcessCas (input, output);
  }
  else if (command.compare("get") == 0)
  {
    return cmdProcessGet (input, output);
  }
  else if (command.compare("gets") == 0)
  {
    return cmdProcessGets (input, output);
  }
  else if (command.compare("delete") == 0)
  {
    return cmdProcessDelete (input, output);
  }
  else if (command.compare("incr") == 0)
  {
    return cmdProcessIncr (input, output);
  }
  else if (command.compare("decr") == 0)
  {
    return cmdProcessDecr (input, output);
  }
  else if (command.compare("touch") == 0)
  {
    return cmdProcessTouch (input, output);
  }
  else if (command.compare("stats") == 0)
  {
    return cmdProcessStats (input, output);
  }
  else if (command.compare("flush_all") == 0)
  {
    return cmdProcessFlushAll (input, output);
  }
  else if (command.compare("version") == 0)
  {
    return cmdProcessVersion (input, output);
  }
  else if (command.compare("verbosity") == 0)
  {
    return cmdProcessVerbosity (input, output);
  }
  else if (command.compare("quit") == 0)
  {
    return cmdProcessQuit (input, output);
  }
  else
  {
    output.assign("ERROR\r\n");
    return 0;
  }
}		/* -----  end of function cmdProcessCommand  ----- */
