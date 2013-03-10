/*==============================================================================
 *
 *       Filename:  smain.cpp
 *
 *    Description:  The server code executes from here
 *
 * =============================================================================
 */

#include "../inc/sinc.h"

// GLOBALS
atomic<unsigned int> gServMemLimit;
atomic<unsigned int> gServListPort;
atomic<unsigned int> gServWorkerThreads;
// GLOBALS END


/* ===  FUNCTION  ==============================================================
 *         Name:  main
 *  Description:  The command line parameters are parsed and the server is 
 *                initialized.
 * =============================================================================
 */
int main (int argc, char *argv[])
{
  gServMemLimit = SERV_DEF_MEM_LIMIT;
  gServListPort = SERV_DEF_LIST_PORT;          
  gServWorkerThreads = SERV_DEF_WORKER_THREADS;

  if (argc != 1) {
    // Optional parameters have been specified
    unsigned short int optionIndex = 1;
    while (optionIndex < argc) {
      if (argv[optionIndex][0] != '-') {
        cout<<"Use memstashed -h for help"<<endl;
        return EXIT_FAILURE;
      }
      switch (argv[optionIndex][1]) {
        case 't':
          optionIndex++;
          if ((optionIndex >= argc) || (argv[optionIndex][0] == '-')) {
            cout<<"Option to -t missing"<<endl;
            return EXIT_FAILURE;
          }
          gServWorkerThreads = atoi(argv[optionIndex]);
          optionIndex++;
          break;
        case 'p':
          optionIndex++;
          gServListPort = atoi(argv[optionIndex]);
          if ((optionIndex >= argc) || (argv[optionIndex][0] == '-')) {
            cout<<"Option to -t missing"<<endl;
            return EXIT_FAILURE;
          }
          gServListPort = atoi(argv[optionIndex]);
          optionIndex++;
          break;
        case 'h':
          optionIndex++;
          cout<<"The options are:"<<endl;
          cout<<"-t The maximum number of worker threads"<<endl;
          cout<<"-p The listening port number"<<endl;
          return EXIT_SUCCESS;
      }
    }
  }

  // All optional parameters have been parsed. Time to set up the server.

  return EXIT_SUCCESS;
}				/* ----------  end of function main  ---------- */
