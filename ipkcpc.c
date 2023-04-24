/**
 * @file ipkcpc.c
 * @author nod3zer0 <nod3zer0@ceskar.xyz>
 * @brief
 * @version 1.1.1
 * @date 2023-03-20
 *
 */

#ifdef __linux__ // linux header files

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#elif _WIN32 // windows header files


#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

#define bzero(b, len) (memset((b), '\0', (len)), (void)0)
#define bcopy(b1, b2, len) (memmove((b2), (b1), (len)), (void)0)

#endif

// CONSTANTS
#define BUFSIZE 1024
// GLOBAl TYPE FOR INTERRUPT HANDLER --------------------
/**
 * @brief stores variables for interrupt handler
 *
 */
typedef struct interruptVarriables_t {
  int client_socket;
  bool connected;
  bool interrupted;
} interruptVarriablesT;

interruptVarriablesT intVals;
//-------------------------------------------------------

/**
 * @brief struct for storing arguments from command line for clients
 *
 */
typedef struct args_t {
  char *host;
  int port;
  bool mode; // false = udp, true = tcp
  bool help;
  bool err;
} argsT;

// FUNCTION DECLARATIONS
argsT parseArgs(int argc, const char *argv[]);
void INThandler(int sig);
void printHelp(void);
struct sockaddr_in getServerAddress(const char *server_hostname, int port);
void UDPclient(int client_socket, struct sockaddr_in server_address);
void TCPclient(int client_socket);
int TCPreceive(int client_socket);

/**
 * @brief parses arguments for client
 *
 * @param argc
 * @param argv
 * @return args
 */
argsT parseArgs(int argc, const char *argv[]) {

  // initialize args
  argsT args;
  args.err = false;
  args.help = false;
  args.mode = 0;
  args.port = 0;
  args.host = (char *)malloc(sizeof(char) * 1000);

  // check if there is right number of arguments
  if (argc != 7 && argc != 2) {
    args.err = true;
    return args;
  }
  // main loop for parsing arguments
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      // check if there is value for host and if it is not too long
      if (i + 1 >= argc || strlen(argv[i + 1]) > 1000) {
        args.err = true;
        return args;
      }
      strcpy(args.host, argv[i + 1]);
    }
    if (strcmp(argv[i], "-p") == 0) {
      // check if there is value for port and if it is in range
      if (i + 1 >= argc || atoi(argv[i + 1]) <= 0 ||
          atoi(argv[i + 1]) > 65535) {
        args.err = true;
        return args;
      }
      args.port = atoi(argv[i + 1]);
    }
    if (strcmp(argv[i], "-m") == 0) {
      // check if there is value for mode and if it is valid value
      if (i + 1 >= argc || (strcmp(argv[i + 1], "tcp") != 0 &&
                            strcmp(argv[i + 1], "udp") != 0)) {
        args.err = true;
        return args;
      }
      // set mode
      if (strcmp(argv[i + 1], "tcp") == 0)
        args.mode = 1;
      else
        args.mode = 0;
    }
    if (strcmp(argv[i], "--help") == 0) {
      args.help = true;
    }
  }
  return args;
}

/**
 * @brief interrupt handler for ctrl+c
 *
 * @param sig
 * @return * void
 */
void INThandler(int sig) {

#ifdef __linux__
  // unblock sigint
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {
    perror("sigprocmask");
    exit(EXIT_FAILURE);
  }
  // send BYE and wait for server to respond
  if (!intVals.interrupted && intVals.connected) {
    fprintf(stdout, "Sigint received, sending BYE, waiting for server to "
                    "respond. To skip this process press ctrl+c again.\n");
    fflush(stdout);
    // set flag that interrupt was alreeady received
    intVals.interrupted = true;
    // send BYE
    if (intVals.connected && intVals.client_socket > 0) {
      if (send(intVals.client_socket, "BYE\n", 4, 0) < 0)
        perror("ERR: sending message");
    }
    char buf[BUFSIZE];
    bzero(buf, BUFSIZE);
    // wait for server to respond
    TCPreceive(intVals.client_socket);
  }
#elif _WIN32
  // send BYE and wait for server to respond
  if (intVals.connected) {
    // different mmessage for windows because of different interrupt handling
    fprintf(stdout, "Sigint received, sending BYE, waiting for server to "
                    "respond. To skip this press ctrl+\\.\n");
    fflush(stdout);
    char data;
    // send BYE
    if (intVals.connected && intVals.client_socket > 0) {
      if (send(intVals.client_socket, "BYE\n", 4, 0) < 0)
        perror("ERR: sending message");
    }
    char buf[BUFSIZE];
    bzero(buf, BUFSIZE);
    // wait for server to respond
    if (recv(intVals.client_socket, buf, BUFSIZE, 0) < 0)
      perror("ERR:receiving message");

    printf("%s\n", buf);
  }
#endif

#ifdef __linux__

  close(intVals.client_socket);

#elif _WIN32

  shutdown(intVals.client_socket, SD_BOTH);
  closesocket(intVals.client_socket);
  WSACleanup();
#endif

  exit(EXIT_SUCCESS);
}

#ifdef __linux__ // linux

/**
 * @brief Get the Server Address object
 *
 * @param server_hostname
 * @param port
 * @return struct sockaddr_in
 */
struct sockaddr_in getServerAddress(const char *server_hostname, int port) {
  struct hostent *server;
  struct sockaddr_in server_address;
  // gets server address by hostname
  if ((server = gethostbyname(server_hostname)) == NULL) {
    fprintf(stderr, "ERR: no such host as %s\n", server_hostname);
    exit(EXIT_FAILURE);
  }

  bzero((char *)&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr,
        server->h_length);
  server_address.sin_port = htons(port);
  return server_address;
}

#elif _WIN32 // windows

/**
 * @brief Get the Server Address object
 *
 * @param server ipv4 address
 * @param port
 * @return struct sockaddr_in
 */
struct sockaddr_in getServerAddress(const char *server_hostname, int port) {
  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  if (inet_pton(AF_INET, server_hostname, &(server_address.sin_addr)) <= 0) {
    printf("Error");
    exit(-1);
  }
  return server_address;
}

#endif

/**
 * @brief prints help
 *
 */
void printHelp() {
  printf("client for the IPK Calculator Protocol\n\n");
  printf("Usage: ipkcpc -h <host> -p <port> -m <mode>\n\n");
#ifdef __linux__
  printf("  -h <host>   IPv4 address or hostname of the server\n");
#elif _WIN32
  printf("  -h <host>   IPv4 address of the server\n");
#endif
  printf("  -p <port>   port of the server\n");
  printf("  -m <mode>   tcp or udp\n");
  printf(" --help   print this help\n");
  printf("\n\n");
  printf("Example: ipkcpc -h 1.2.3.4 -p 2023 -m udp\n");
}

/**
 * @brief UDP client for the IPK Calculator Protocol
 *
 * @param client_socket
 * @param server_address
 */
void UDPclient(int client_socket, struct sockaddr_in server_address) {
  char buf[BUFSIZE];
#ifdef __linux__
  unsigned int serverA_lenght = sizeof(server_address);
#elif _WIN32
  int serverA_lenght = sizeof(server_address);
#endif

  // infinite loop
  while (true) {

    bzero(buf, BUFSIZE);
    // loads command from user
    if (fgets(buf, BUFSIZE, stdin) == NULL) { // detects EOF
      break;
    }
    // makes space 2 bytes for message length and opcode
    int buf_lenght = strlen(buf);
    for (int i = buf_lenght; i >= 0; i--) {
      buf[i + 2] = buf[i];
    }

    // sets message length and opcode
    buf[1] = buf_lenght;
    buf[0] = 0;
    // sends encoded message
    if (sendto(client_socket, buf, buf_lenght + 2, 0,
               (struct sockaddr *)&server_address, serverA_lenght) < 0)
      perror("ERR:sending message");
    // clear buffer before receiving
    bzero(buf, BUFSIZE);

    if (recvfrom(client_socket, buf, BUFSIZE, 0,
                 (struct sockaddr *)&server_address, &serverA_lenght) < 0)
      perror("ERR:receiving message");
    // loads statuscode
    char statusCode = buf[1];
    if (buf[0] != 1) {
      printf("ERR:wrong opcode\n");
    }

    int messageSize = buf[2];
    // removes first 3 bytes to convert message to string
    for (int i = 3; i < BUFSIZE; i++) {
      buf[i - 3] = buf[i];
    }

    if (buf[messageSize] != '\0') {
      printf("ERR:wrong message size\n");
    }
    // prints response according to statuscode
    if (statusCode) {
      printf("ERR:%s\n", buf);
    } else {
      printf("OK:%s\n", buf);
    }
    fflush(stdout);
  }
}

/**
 * @brief Receives messages from server and prints them to stdout. Loads
 * complete messages until newline is found.
 *
 * @param client_socket
 * @return true
 * @return 1 - if serever sends BYE, 0 - if message is not BYE
 */
int TCPreceive(int client_socket) {
  // initialize varriables
  char buf[BUFSIZE];
  bzero(buf, BUFSIZE);
  char temp[BUFSIZE];
  bzero(buf, BUFSIZE);
  int tempPointer = 0;

  // receives message from server until newline is found
  do {
    bzero(buf, BUFSIZE);

    if (recv(client_socket, buf, BUFSIZE - 1, 0) < 0)
      perror("ERR:receiving message");
    // prints response
    printf("%s", buf);
    fflush(stdout);

    // if message is longer than 5 characters, it is not BYE
    if (tempPointer < 5) {
      strcpy(temp + tempPointer, buf);
      tempPointer++;
    }

    // checks if server sends BYE
    if (tempPointer < 5 && strcmp(temp, "BYE\n") == 0) {
      return 1;
    }
    // if message is empty, continue receiving
    if (strlen(buf) < 1) {
      continue;
    }
  } while (buf[strlen(buf) - 1] != '\n');
  return 0;
}

/**
 * @brief TCP client for the IPK Calculator Protocol
 *
 * @param client_socket
 */
void TCPclient(int client_socket) {
  char buf[BUFSIZE];
  // infinite loop
  while (true) {
    // loads command from user
    if (fgets(buf, BUFSIZE, stdin) == NULL) { // detects EOF
      send(client_socket, "BYE\n", 4, 0);
      TCPreceive(client_socket);
      break;
    }
    if (send(client_socket, buf, strlen(buf), 0) < 0)
      perror("ERR:sending message");

    bzero(buf, BUFSIZE);

    // if BYE is received - break
    if (TCPreceive(client_socket) == 1) {
      break;
    }
  }
}

int main(int argc, const char *argv[]) {

// windpows socket init
#ifdef _WIN32
  WSADATA Data;
  int err = WSAStartup(MAKEWORD(2, 2), &Data);
  if (err != 0) {
    printf("Error starting up socket library!");
    exit(-1);
  }
#endif

// sigint setup
#ifdef __linux__
  struct sigaction sa;
  sa.sa_handler = INThandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
#elif _WIN32
  signal(SIGINT, INThandler);
#endif

  intVals.client_socket = 0;
  intVals.connected = false;
  intVals.interrupted = false;

  argsT args = parseArgs(argc, argv);
  if (args.err == true) {
    fprintf(stderr, "wrong arguments\n");
    printHelp();
    return 1;
  } else if (args.help == true) {
    printHelp();
    return 0;
  }
  // init local variables
  struct sockaddr_in server_address;
  char buf[BUFSIZE];

  server_address = getServerAddress(args.host, args.port);

  // use right client
  if (args.mode) { // TCP
    // creates socket
    if ((intVals.client_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
      perror("ERR:socket");
      exit(EXIT_FAILURE);
    }
    // prepares buffer for incoming data
    bzero(buf, BUFSIZE);
    // connects to server
    if (connect(intVals.client_socket, (const struct sockaddr *)&server_address,
                sizeof(server_address)) != 0) {
      perror("ERR:connect");
      exit(EXIT_FAILURE);
    }
    intVals.connected = true;
    TCPclient(intVals.client_socket);
  } else { // UDP
    // creates socket
    if ((intVals.client_socket = socket(AF_INET, SOCK_DGRAM, 0)) <= 0) {
      perror("ERR:socket");
      exit(EXIT_FAILURE);
    }
    UDPclient(intVals.client_socket, server_address);
  }

#ifdef __linux__ // linux close socket

  close(intVals.client_socket);

#elif _WIN32 // windows close socket
  shutdown(intVals.client_socket, SD_BOTH);
  closesocket(intVals.client_socket);
  WSACleanup();

#endif

  return 0;
}
