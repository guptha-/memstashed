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

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessSet
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessSet (string &input, string &output)
{
  int nextSpace = input.find(' ');
  int tempSpace;
  int nextEndl = input.find("\r\n");
  int retVal = -1;
  bool noreply;
  bool errFlag = false;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    output.assign ("CLIENT_ERROR key not found\r\n");
    errFlag = true;
  }
  nextSpace++;
  string key = input.substr(nextSpace,
      (tempSpace = input.find(' ', nextSpace)));

  nextSpace = tempSpace;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR flags not found\r\n");
      errFlag = true;
    }
  }
  nextSpace++;
  string flags = input.substr(nextSpace, 
      (tempSpace = input.find(' ', nextSpace)));

  nextSpace = tempSpace;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR expiry time not found\r\n");
      errFlag = true;
    }
  }
  nextSpace++;
  string expTimeStr = input.substr(nextSpace, 
      (tempSpace = input.find(' ', nextSpace)));
  int expTime;
  try
  {
    expTime = stoul(expTimeStr);
  }
  catch (const invalid_argument& ia)
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR invalid expiry time\r\n");
      errFlag = true;
    }
  }

  nextSpace = tempSpace;
  if (((size_t)nextSpace == string::npos) || (nextSpace > nextEndl))
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR bytes not found\r\n");
      errFlag = true;
    }
  }
  nextSpace++;
  string bytesStr = input.substr(nextSpace, 
      (tempSpace = input.find(' ', nextSpace)));
  if (nextSpace > nextEndl)
  {
    // noreply is not there and there is no space between <bytes> and \r\n
    bytesStr = input.substr(nextSpace, 
        (tempSpace = input.find("\r\n", nextSpace)));
  }
  else
  {
    // noreply could be there or just a space between <bytes> and \r\n
    
    nextSpace = tempSpace;
    nextSpace++;
    if (nextSpace == nextEndl)
    {
      // No noreply found
      noreply = false;
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
          errFlag = true;
        }
      }
      noreply = true;
    }
  }
  int bytes;
  try
  {
    bytes = stoul(bytesStr);
  }
  catch (const invalid_argument& ia)
  {
    if (retVal != 0)
    {
      output.assign ("CLIENT_ERROR invalid number of bytes\r\n");
      errFlag = true;
    }
  }

  nextSpace = nextEndl + 2;
  if (input.size() < (size_t)nextSpace)
  {
    // Nothing else is there to read in the string
    if (bytes != 0)
    {
      // We will try to read 'bytes + 2' bytes from the socket
      return (bytes + 2);
    }
  }
  string data = input.substr (nextSpace, string::npos);
  if (data.size() < (size_t)(bytes + 2))
  {
    // We still need to fetch more data
    return (bytes + 2 - data.size());
  }
  // We have all the data we need. Do processing
  return 0;
}		/* -----  end of function cmdProcessSet  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  cmdProcessAdd
 *  Description:  This function processes the given command
 * =============================================================================
 */
int cmdProcessAdd (string &input, string &output)
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
