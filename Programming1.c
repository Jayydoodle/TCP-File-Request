/*
** Jason Allen
**
** This program demonstrates sending a request for a file over TCP, and returning
** the contents of the file to the requesting client
**
** to run, type 'make' in a unix shell, ./run, and follow the on screen prompts
**
** Prompt 1: Port number
** - Try port 2000 if access denied on lower numbers
**
** Prompt 2: Filename
** - Supply the name of a file in the local directory.  test.txt is included
** for demonstration
**
** demo - ./run  -> prompt 1: 2000  -> prompt 2: test.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define MAXPENDING 5
#define RCVBUFSIZE 1024

typedef struct processThread{

  int tid;
  pthread_t thread;
  struct processThread *next;
  struct processThread *previous;

} processThread_t;

void readFile(char *fileName, int clientSocket);
void DieWithError(char *errorMessage);
void HandleTCPClient(int clientSocket);
void *startServer(void *arg);
void startClient(unsigned short port);

int main(int argc, char *argv[])
{
    unsigned short serverPort;

    while(1)
    { // get user input for the port
      printf("\nEnter the port number you would like to run the server on: ");

      if(scanf("%hd",&serverPort)<=0){
          printf("Not an Integer, exiting program.\n");
          exit(0);
      }
      // create a process thread to run the server in the background
      // this removes the need for 2 separate programs for calling the
      // server and the client
      processThread_t *serverProcess = (processThread_t*)malloc(sizeof(processThread_t));
      pthread_create(&serverProcess->thread, NULL, startServer, (void*)serverPort);

      startClient(serverPort); // start the client
    }

    return 0;
}

/* ====================== readFile =========================
/* Reads a file and sends it's contents back to the client
/* line by line
/*
/* @param fileName - the name of the file (char *)
/* @returns nothing
*/
void readFile(char *fileName, int clientSocket)
{
  FILE *input = fopen(fileName, "rb");
  size_t line_buf_size = 0;
  ssize_t line_size = 0;
  int recvMsgSize = 0;
  int totalBytesSent = 0;

  if(input == NULL)
  {
      // if the file was not found, send error message to client
      char *errorMessage = malloc(100 * sizeof(char));
      sprintf(errorMessage, "Error, invalid file name \"%s\"\n", fileName);
      send(clientSocket, errorMessage, strlen(errorMessage), 0);
  }
  else
  {
      // read file line by line
      while (line_size >= 0)
      {
        char *line_buf = NULL;
        line_size = getline(&line_buf, &line_buf_size, input);

        if(line_size < 0)
          continue;

        totalBytesSent += line_size;

        line_buf[strlen(line_buf) - 1] = 0;
        printf("read line: %s\n", line_buf);

        // send the read line to the client
        if (send(clientSocket, line_buf, line_size, 0) != line_size)
      			DieWithError("send() failed");

        // wait for response from client that message was recieved
        if ((recvMsgSize = recv(clientSocket, line_buf, RCVBUFSIZE, 0)) < 0)
    			DieWithError("recv() failed") ;
      }

      fclose(input);

      printf("\nTotal Bytes Sent: %d\n\n", totalBytesSent);
  }
}

/* ====================== DieWithError =========================
/* Error handler for TCP send and recv methods
/*
/* @param errorMessage - the error message to deliver
/* @returns nothing
*/
void DieWithError(char *errorMessage)
{
	perror(errorMessage);
	exit(1);
}
/* ====================== HandleTCPClient ===================
/* Process file requests sent by the TCP client
/*
/* @param clientSocket - the socket of the requesting client
/* @returns nothing
*/
void HandleTCPClient(int clientSocket)
{
	char fileBuffer[RCVBUFSIZE];
	int recvMsgSize;

  // get the file name from the client
	if ((recvMsgSize = recv(clientSocket, fileBuffer, RCVBUFSIZE, 0)) < 0)
		DieWithError("recv() failed");

  // get rid of any extra bytes
  fileBuffer[recvMsgSize] = '\0';
  printf("Server Recieved Filename: %s\n\n", fileBuffer);

  // read the file and process
  readFile(fileBuffer, clientSocket);

	close(clientSocket); /* Close client socket */
}
/* ====================== startSever ===============================
/* Start the TCP server and wait for a client to connect
/*
/* @param arg - the port number sent as void* to be passed
/* in via a p_thread
/* @returns nothing
*/
void *startServer(void *arg)
{
  int servSock; /* Socket descriptor for server */
  int clntSock; /* Socket descriptor for client */
  struct sockaddr_in echoServAddr; /* Local address */
  struct sockaddr_in echoClntAddr; /* Client address */
  unsigned short *echoServPort; /* Server port */
  unsigned int clntLen; /* Length of client address data structure */

  echoServPort = (unsigned short*)arg;

  /* Create socket for incoming connections */
  if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  DieWithError( "socket () failed") ;

  /* Construct local address structure */
  memset(&echoServAddr, 0, sizeof(echoServAddr)); /* Zero out structure */
  echoServAddr.sin_family = AF_INET; /* Internet address family */
  echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
  echoServAddr.sin_port = htons(echoServPort); /* Local port */

  /* Bind to the local address */
  if (bind(servSock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0)
    DieWithError ( "bind () failed");

  /* Mark the socket so it will listen for incoming connections */
  if (listen(servSock, MAXPENDING) < 0)
    DieWithError("listen() failed") ;

  while(1) /* Run forever */
  {
    /* Set the size of the in-out parameter */
    clntLen = sizeof(echoClntAddr);

    /* Wait for a client to connect */
    if ((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr, &clntLen)) < 0)
      DieWithError("accept() failed");

    /* clntSock is connected to a client! */
    printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));
    HandleTCPClient (clntSock) ;
  }
}
/* ====================== startClient ===============================
/* Start the TCP client and connects to the server
/*
/* @param port - the port number to connect to
/*
/* @returns nothing
*/
void startClient(unsigned short port)
{
  int sock;
	struct sockaddr_in echoServAddr;
	unsigned short echoServPort;
	char *servIP;
	char *echoString;
	char echoBuffer[RCVBUFSIZE];
	unsigned int echoStringLen;
	int bytesRcvd, totalBytesRcvd;
  char *fileName = calloc(128, sizeof(char));

  /*region: This block of code is used to auto generate the
  local IP address of the user to remove the need to have
  it entered manually, and therefore allowing me to combine
  the server and client call into a single program
  */
  char hostbuffer[256];
  char *IPbuffer;
  struct hostent *host_entry;

  // Retrieve hostname
  gethostname(hostbuffer, sizeof(hostbuffer));

  // Retrieve host information
  host_entry = gethostbyname(hostbuffer);

  // Convert network address to ascii string
  IPbuffer = inet_ntoa(*((struct in_addr*)
                         host_entry->h_addr_list[0]));

  printf("Hostname: %s\n", hostbuffer);
  printf("Host IP: %s\n", IPbuffer);

  /*End Region*/

  // Set the ip address and port
  servIP = IPbuffer;
  echoServPort = port;

  // eat any chars in the stdin buffer
  int c = getchar();

  while(1)
  {
    // request user input for the file name
    printf("Enter the name of the file: ");
    if(fgets(fileName, 128, stdin) == NULL){
        printf("\n");
        continue;
    }
    else{
      break;
    }
  }

  // remove any extra bytes
  fileName[strlen(fileName) - 1] = '\0';
  echoString = fileName;

  printf("Processing file: %s\n", fileName);

  sleep(2); // sleep client to ensure server starts first

	/*Create a socket using TCP*/
	if((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))<0)
		DieWithError("socket() failed");

	/*Construct the server address structure*/
	memset(&echoServAddr, 0, sizeof(echoServAddr));
	echoServAddr.sin_family = AF_INET;
	echoServAddr.sin_addr.s_addr = inet_addr(servIP);
	echoServAddr.sin_port = htons(echoServPort);

	/*Establish connection to server*/
	if (connect(sock, (struct scokaddr*) &echoServAddr, sizeof (echoServAddr))<0)
		DieWithError("connect( ) failed");

	echoStringLen = strlen(fileName);

	// send the file name to the client
	if (send(sock, echoString, echoStringLen, 0)!=echoStringLen)
		DieWithError("send() sent a different number of bytes than expected");

	totalBytesRcvd=0;

  // recieve the file from the server line by line
	while (bytesRcvd != 0)
	{
		if((bytesRcvd = recv(sock, echoBuffer, RCVBUFSIZE-1, 0)) <= 0)
			break;

		totalBytesRcvd += bytesRcvd;

		printf("Client Recieved: %s\n", echoBuffer);

    send(sock, "finished", 8, 0); // send message to the server to send next line
	}

  printf("Total Bytes Recieved: %d\n\n", totalBytesRcvd);

	close(sock);
	exit(0);
}
