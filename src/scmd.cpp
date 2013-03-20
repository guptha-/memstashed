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
 * =============================================================================
 */
int cmdProcessCommon (const string &input, string &output, bool isCas, 
    StorageStruct &store)
{
  int nextSpace = input.find(' ');
  int tempSpace;
  int nextEndl = input.find("\r\n");
  int retVal = -1;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR key not found\r\n");
    return 0;
  }
  nextSpace++;
  store.key = input.substr(nextSpace,
      (tempSpace = input.find(' ', nextSpace)));

  nextSpace = tempSpace;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR flags not found\r\n");
      return 0;
    }
  }
  nextSpace++;
  store.flags = input.substr(nextSpace, 
      (tempSpace = input.find(' ', nextSpace)));

  nextSpace = tempSpace;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR expiry time not found\r\n");
      return 0;
    }
  }
  nextSpace++;
  string expTimeStr = input.substr(nextSpace, 
      (tempSpace = input.find(' ', nextSpace)));
  try
  {
    store.expTime = stoul(expTimeStr);
  }
  catch (const invalid_argument& ia)
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR invalid expiry time\r\n");
      return 0;
    }
  }

  nextSpace = tempSpace;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR bytes not found\r\n");
      return 0;
    }
  }
  nextSpace++;
  string bytesStr = input.substr(nextSpace, 
      (tempSpace = input.find(' ', nextSpace)));
  if (isCas == true)
  {
    nextSpace = tempSpace;
    if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
    {
      if (retVal != 0)
      {
        output.assign ("CLIENT_ERROR cas string not found\r\n");
        return 0;
      }
    }
    nextSpace++;
    store.casStr= input.substr(nextSpace, 
        (tempSpace = input.find(' ', nextSpace)));
  }
  if (nextSpace > nextEndl)
  {
    // noreply is not there and there is no space between <bytes> and \r\n
    // or <cas unique> and \r\n
    if (isCas == true)
    {
      store.casStr= input.substr(nextSpace, 
          (tempSpace = input.find("\r\n", nextSpace)));
    }
    else
    {
      bytesStr = input.substr(nextSpace, 
          (tempSpace = input.find("\r\n", nextSpace)));
    }
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
          (tempSpace = input.find("\r\n", nextSpace)));
      if (noReplyStr.compare("noreply") != 0)
      {
        if (retVal != 0)
        {
          output.assign ("CLIENT_ERROR junk found instead of \"noreply\"\r\n");
          return 0;
        }
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
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR invalid number of bytes\r\n");
      return 0;
    }
  }

  nextSpace = nextEndl + 2;
  if (input.size() < (size_t)nextSpace)
  {
    // Nothing else is there to read in the string
    if (store.bytes != 0)
    {
      // We will try to read 'bytes + 2' bytes from the socket
      return (store.bytes + 2);
    }
  }
  store.data = input.substr (nextSpace, string::npos);
  if (store.data.size() < (size_t)(store.bytes + 2))
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
  int ret = cmdProcessCommon (input, output, false, store);
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
  return 0;
}		/* -----  end of function cmdProcessSet  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessAdd
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessAdd (const string &input, string &output)
{
  return 0;
}		/* -----  end of function cmdProcessAdd  ----- */

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
  string command = input.substr(0, input.find(' '));

  int ret;

  if (command.compare("set") == 0)
  {
    ret = cmdProcessSet (input, output);
  }
  else if (command.compare("add") == 0)
  {
    ret = cmdProcessAdd (input, output);
  }

  return ret;
}		/* -----  end of function cmdProcessCommand  ----- */
