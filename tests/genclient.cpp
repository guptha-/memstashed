/*==============================================================================
 *
 *       Filename:  genclient.cpp
 *
 *    Description:  This is a client sample for testing. Code mostly taken from
 *                  publib.boulder.ibm.com, with a few modifications
 *
 * =============================================================================
 */

/**************************************************************************/
/* Generic client example is used with connection-oriented server designs */
/**************************************************************************/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT  11211

int main (int argc, char *argv[])
{
   int    len, rc;
   int    sockfd;
   char   send_buf[800];
   char   recv_buf[800];
   struct sockaddr_in   addr;
   unsigned long int keys = 500;
   if (argc == 2)
   {
     keys = atoi(argv[1]);
   }

   /*************************************************/
   /* Create an AF_INET stream socket               */
   /*************************************************/
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0)
   {
      perror("socket");
      exit(-1);
   }

   /*************************************************/
   /* Initialize the socket address structure       */
   /*************************************************/
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port        = htons(SERVER_PORT);

   /*************************************************/
   /* Connect to the server                         */
   /*************************************************/
   rc = connect(sockfd,
                (struct sockaddr *)&addr,
                sizeof(struct sockaddr_in));
   if (rc < 0)
   {
      perror("connect");
      close(sockfd);
      exit(-1);
   }
   printf("Connect completed.\n");

   /*************************************************/
   /* Enter data buffer that is to be sent          */
   /*************************************************/

   int size = strlen(send_buf);

   /*************************************************/
   /* Send data buffer to the worker job            */
   /*************************************************/
   unsigned long i;
   for (i = 0; i < keys; i++)
   {

     memset(send_buf, 0, sizeof(send_buf));
     sprintf (send_buf, "set keychampioningtheraceisthevictoroftheevent%lu flags 1000 5\r\nvalue\r\n", i);
     size = strlen(send_buf);
     len = send(sockfd, send_buf, size, 0);
     //printf("%d bytes sent\n", len);

     /* ************************************************/
     /*  Receive data buffer from the worker job       */
     /* ************************************************/
     memset(&recv_buf, 0, 80);
     len = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
     if (len < 3)
     {
       printf("ERROR in set\r\n");
     }

     memset(send_buf, 0, sizeof(send_buf));
     sprintf (send_buf, "get keychampioningtheraceisthevictoroftheevent%lu\r\n", i);
     size = strlen(send_buf);
     len = send(sockfd, send_buf, size, 0);
     //printf("%d bytes sent\n", len);

     /* ************************************************/
     /*  Receive data buffer from the worker job       */
     /* ************************************************/
     memset(&recv_buf, 0, 80);
     len = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
     if ((len < 3))
     {
       printf("ERROR in get\r\n");
     }
   }
   /*************************************************/
   /* Close down the socket                         */
   /*************************************************/
   close(sockfd);
   return EXIT_SUCCESS;
}
