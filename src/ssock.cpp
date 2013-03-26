/*==============================================================================
 *
 *       Filename:  ssock.cpp
 *
 *    Description:  This file contains all the socket logic for the server.
 *                  Code in this file has been adapted from 
 *                  http://publib.boulder.ibm.com. It has been modified heavily
 *                  to suit my purposes
 *
 * =============================================================================
 */

#include "../inc/sinc.h"

static mutex masterMutex;
static fd_set master_set;
static atomic<int> max_sd;
static condition_variable threadNotify;
static vector<int> sockDescServiceList;
static atomic<unsigned int> numberActiveThreads;
static mutex sockDescMutex;
static mutex threadNotifyMutex;


/* ===  FUNCTION  ==============================================================
 *         Name:  sockHandleIncomingConn
 *  Description:  This function receives the incoming message and hands it off
 *                to the message parser
 * =============================================================================
 */
void sockHandleIncomingConn (int sockFd)
{
  char   buffer[80];
  int close_conn = FALSE;
  int readMore = 0;
  unsigned int pos = 0;
  string data;
  string output;
  string persistentData;
  bool persistFlag = false;
  /*************************************************/
  /* Receive all incoming data on this socket      */
  /* before we loop back and call select again.    */
  /*************************************************/
  do
  {
    /**********************************************/
    /* Receive data on this connection until the  */
    /* recv fails with EWOULDBLOCK.  If any other */
    /* failure occurs, we will close the          */
    /* connection.                                */
    /**********************************************/
    int rc;
    if (!persistentData.empty())
    {
      persistFlag = true;
      data.append(persistentData);
      persistentData.clear();
    }
    else
    {
      rc = recv(sockFd, buffer, sizeof(buffer), 0);
      if (rc < 0)
      {
        if (errno != EWOULDBLOCK)
        {
          perror("  recv() failed");
          close_conn = TRUE;
        }
        break;
      }

      /**********************************************/
      /* Check to see if the connection has been    */
      /* closed by the client                       */
      /**********************************************/
      if (rc == 0)
      {
        close_conn = TRUE;
        break;
      }

      /**********************************************/
      /* Data was received                          */
      /**********************************************/

      data.append(buffer, rc);
      readMore -= rc; // If readMore was initially 0, it will now be -ve
    }
    unsigned int oldPos = pos + 2;
    pos = data.find("\r\n", oldPos);
    if (pos == string::npos)
    {
      // Cannot find an endline, have to read more from the input stream
      pos = oldPos - 2;
      continue;
    }
    persistentData.assign(data, pos + 2, string::npos);
    data.erase(pos + 2, string::npos);
    if (persistFlag == true)
    {
      readMore -= (pos - oldPos + 2);
      persistFlag = false;
    }
    // If readMore was initially 0, it will now be -ve
    if (readMore > 0)
    {
      // We have to receive more information
      continue;
    }

    // Both text lines as well as unstructured data end with \r\n
    // We optimistically assume a single text line initially and call
    // cmdProcessCommand. If it requires more data, it returns a positive 
    // readMore. Once a readMore is set, we get the exact amount of data 
    // requested. If the client doesn't supply the full data soon, the 
    // connection remains hanging till the client goes away. One solution is to
    // use setsockopt to specify a timeout for recv
    if ((*(data.end() - 2) == '\r') && (*(data.end() - 1) == '\n'))
    {
      // End of command string
      
      if ((readMore = cmdProcessCommand(data, output)) > 0)
      {
        // The only reason this would be > 0 is if the input is incomplete.
        // So, we should read till size more bytes
        continue;
      }
      else if (readMore < 0)
      {
        // Received a quit
        close_conn = true;
        break;
      }
      // Done processing this command
      data.clear();
      char *sendbuf = (char *)output.c_str();
      send(sockFd, sendbuf, output.length(), 0);
      output.clear();
      pos = 0;
    }
    // If the read string doesn't end with a \r\n, continue looking for i/p
  } while (TRUE);
  /*************************************************/
  /* If the close_conn flag was turned on, we need */
  /* to clean up this active connection.  This     */
  /* clean up process includes removing the        */
  /* descriptor from the master set and            */
  /* determining the new maximum descriptor value  */
  /* based on the bits that are still turned on in */
  /* the master set.                               */
  /*************************************************/
  if (close_conn)
  {
    close(sockFd);
  }
}		/* -----  end of function sockHandleIncomingConn  ----- */


/* ===  FUNCTION  ==============================================================
 *         Name:  socketThread
 *  Description:  This is the entry for each thread in the thread pool
 * =============================================================================
 */
void socketThread ()
{
  unique_lock<mutex> l(threadNotifyMutex);

  while (true)
  {
    threadNotify.wait(l, [](){ return sockDescServiceList.size() != 0;} );

    /* The main thread has notified, and there is an element in the 
     * sockDescServiceList */
    sockDescMutex.lock();
    if (sockDescServiceList.size() == 0)
    {
      continue;
    }
    vector<int>::iterator it = sockDescServiceList.begin();
    int sockFd = *it;
    sockDescServiceList.erase(it);
    sockDescMutex.unlock();

    // Now, we can unlock for the other threads
    l.unlock();

    numberActiveThreads++;
    sockHandleIncomingConn (sockFd);
    
    // We are going to wait again
    numberActiveThreads--;
    l.lock();
  }

  return;
}		/* -----  end of function socketThread  ----- */

/* ===  FUNCTION  ==============================================================
 *         Name:  socketMain
 *  Description:  This function starts off the main socket flow for the server.
 * =============================================================================
 */
void socketMain ()
{
  int    i, rc, on = 1;
  int    listen_sd, new_sd;
  int    desc_ready, end_server = FALSE;
  sockaddr_in   addr;
  fd_set        working_set;

  vector<thread> threadList;

  numberActiveThreads = 0;
  unsigned int numberThreads = 0;

  /* Spawning the thread pool */
  while (numberThreads < gServWorkerThreads)
  {
    threadList.push_back(thread(socketThread));
    numberThreads++;
  }

  /*************************************************************/
  /* Create an AF_INET stream socket to receive incoming       */
  /* connections on                                            */
  /*************************************************************/
  listen_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sd < 0)
  {
    perror("socket() failed");
    exit(-1);
  }

  /*************************************************************/
  /* Allow socket descriptor to be reuseable                   */
  /*************************************************************/
  rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
      (char *)&on, sizeof(on));
  if (rc < 0)
  {
    perror("setsockopt() failed");
    close(listen_sd);
    exit(-1);
  }

  /*************************************************************/
  /* Set socket to be non-blocking.  All of the sockets for    */
  /* the incoming connections will also be non-blocking since  */
  /* they will inherit that state from the listening socket.   */
  /*************************************************************/
  rc = ioctl(listen_sd, FIONBIO, (char *)&on);
  if (rc < 0)
  {
    perror("ioctl() failed");
    close(listen_sd);
    exit(-1);
  }

  /*************************************************************/
  /* Bind the socket                                           */
  /*************************************************************/
  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port        = htons(SERV_DEF_LIST_PORT);
  rc = bind(listen_sd,
      (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0)
  {
    perror("bind() failed");
    close(listen_sd);
    exit(-1);
  }

  /*************************************************************/
  /* Set the listen back log                                   */
  /*************************************************************/
  rc = listen(listen_sd, 32);
  if (rc < 0)
  {
    perror("listen() failed");
    close(listen_sd);
    exit(-1);
  }

  /*************************************************************/
  /* Initialize the master fd_set                              */
  /*************************************************************/
  masterMutex.lock();
  FD_ZERO(&master_set);
  max_sd = listen_sd;
  FD_SET(listen_sd, &master_set);
  masterMutex.unlock();

  /*************************************************************/
  /* Loop waiting for incoming connects or for incoming data   */
  /* on any of the connected sockets.                          */
  /*************************************************************/
  do
  {
    /**********************************************************/
    /* Copy the master fd_set over to the working fd_set.     */
    /**********************************************************/
    masterMutex.lock();
    memcpy(&working_set, &master_set, sizeof(master_set));
    masterMutex.unlock();

    /**********************************************************/
    /* Call select()                                          */
    /**********************************************************/
    rc = select(max_sd + 1, &working_set, NULL, NULL, NULL);

    /**********************************************************/
    /* Check to see if the select call failed.                */
    /**********************************************************/
    if (rc < 0)
    {
      perror("  select() failed");
      break;
    }

    /**********************************************************/
    /* One or more descriptors are readable.  Need to         */
    /* determine which ones they are.                         */
    /**********************************************************/
    desc_ready = rc;
    for (i=0; i <= max_sd  &&  desc_ready > 0; ++i)
    {
      /*******************************************************/
      /* Check to see if this descriptor is ready            */
      /*******************************************************/
      if (FD_ISSET(i, &working_set))
      {
        /****************************************************/
        /* A descriptor was found that was readable - one   */
        /* less has to be looked for.  This is being done   */
        /* so that we can stop looking at the working set   */
        /* once we have found all of the descriptors that   */
        /* were ready.                                      */
        /****************************************************/
        desc_ready -= 1;

        /****************************************************/
        /* Check to see if this is the listening socket     */
        /****************************************************/
        if (i == listen_sd)
        {
          /*************************************************/
          /* Accept all incoming connections that are      */
          /* queued up on the listening socket before we   */
          /* loop back and call select again.              */
          /*************************************************/
          do
          {
            /**********************************************/
            /* Accept each incoming connection.  If       */
            /* accept fails with EWOULDBLOCK, then we     */
            /* have accepted all of them.  Any other      */
            /* failure on accept will cause us to end the */
            /* server.                                    */
            /**********************************************/
            new_sd = accept(listen_sd, NULL, NULL);
            if (new_sd < 0)
            {
              if (errno != EWOULDBLOCK)
              {
                perror("  accept() failed");
                end_server = TRUE;
              }
              break;
            }

            if (numberActiveThreads >= gServWorkerThreads)
            {
              cout<<"Too many connections: Cannot read from more"<<endl;
              close (new_sd);
              continue;
            }

            /**********************************************/
            /* Add the new incoming connection to the     */
            /* master read set                            */
            /**********************************************/
            masterMutex.lock();
            FD_SET(new_sd, &master_set);
            if (new_sd > max_sd)
              max_sd = new_sd;
            masterMutex.unlock();

            /**********************************************/
            /* Loop back up and accept another incoming   */
            /* connection                                 */
            /**********************************************/
          } while (new_sd != -1);
        }

        /****************************************************/
        /* This is not the listening socket, therefore an   */
        /* existing connection must be readable             */
        /****************************************************/
        else
        {
          sockDescMutex.lock();
          sockDescServiceList.push_back(i);;
          sockDescMutex.unlock();
          // Clearing from select for now
          masterMutex.lock();
          FD_CLR(i, &master_set);
          if (i == max_sd)
          {
            while (FD_ISSET(max_sd, &master_set) == FALSE)
              max_sd -= 1;
          }
          masterMutex.unlock();
          /* We are notifying one member of the thread pool */
          threadNotify.notify_one();

        } /* End of existing connection is readable */
      } /* End of if (FD_ISSET(i, &working_set)) */
    } /* End of loop through selectable descriptors */

  } while (end_server == FALSE);

  /*************************************************************/
  /* Cleanup all of the sockets that are open                  */
  /*************************************************************/
  masterMutex.lock();
  for (i=0; i <= max_sd; ++i)
  {
    if (FD_ISSET(i, &master_set))
      close(i);
  }
  masterMutex.unlock();
}		/* -----  end of function socketMain  ----- */
