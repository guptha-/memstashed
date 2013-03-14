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
  int len = 0;
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
    cout<<"On recv"<<endl;
    int rc = recv(sockFd, buffer, sizeof(buffer), 0);
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
      printf("  Connection closed\n");
      close_conn = TRUE;
      break;
    }

    /**********************************************/
    /* Data was recevied                          */
    /**********************************************/
    len = rc;
    printf("  %d bytes received\n", len);

    /* Do something here to make sure the data received is proper and
     * not incomplete. Have to distinguish between different packets. When it
     * is complete, send to handler. */

  } while (TRUE);
  cout<<"Control reaches close"<<endl;
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
    threadNotify.wait_for(l,  chrono::milliseconds(SERV_MAX_LATENCY), 
                  []() { return sockDescServiceList.size() != 0; });
    {
      /* If size == 0, timed out but sockDescList has not got an element */
      continue;
    }

    cout<<"Element in sockDesc"<<endl;
    /* Flow reaches here only if sockDescServiceList is not empty */
    sockDescMutex.lock();
    vector<int>::iterator it = sockDescServiceList.begin();
    int sockFd = *it;
    sockDescServiceList.erase(it);
    sockDescMutex.unlock();

    sockHandleIncomingConn (sockFd);
    // We are going to wait again
    numberActiveThreads--;
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
  while (numberThreads++ < gServWorkerThreads)
  {
    threadList.push_back(thread(socketThread));
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
    printf("Waiting on select()...\n");
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
          printf("  Listening socket is readable\n");
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

            /**********************************************/
            /* Add the new incoming connection to the     */
            /* master read set                            */
            /**********************************************/
            printf("  New incoming connection - %d\n", new_sd);
            masterMutex.lock();
            FD_SET(new_sd, &master_set);
            masterMutex.unlock();
            if (new_sd > max_sd)
              max_sd = new_sd;

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
          printf("  Descriptor %d is readable\n", i);
          
          sockDescMutex.lock();
          sockDescServiceList.push_back(i);;
          cout<<"sockDescSize: "<<sockDescServiceList.size()<<endl;
          sockDescMutex.unlock();
          // Clearing from select till the next accept is reached
          masterMutex.lock();
          FD_CLR(i, &master_set);
          if (i == max_sd)
          {
            while (FD_ISSET(max_sd, &master_set) == FALSE)
              max_sd -= 1;
          }
          masterMutex.unlock();
          /* We are notifying one member of the thread pool */
          numberActiveThreads++;
          if (numberActiveThreads >= gServWorkerThreads)
          {
            numberActiveThreads--;
            cout<<"Too many connections"<<endl;
            close(i);
          }
          threadNotify.notify_all();
          threadNotify.notify_all();

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