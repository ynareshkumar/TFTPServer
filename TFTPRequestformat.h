#include <iostream>
#include <stdint.h>

using namespace std;

#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR 5

//Standard TFTP errors
#define EUNDEF          0               //not defined
#define ENOTFOUND       1               //file not found 
#define EACCESS         2               //access violation 
#define ENOSPACE        3               //disk full or allocation exceeded
#define EBADOP          4               //Undefined TFTP Request Type
#define EBADID          5               //unknown transfer ID

//Error messages to communicate with client
const char *errorcodes[] ={ 
"Undefined.",
"File not found.",
"Access Denied",
"Maximum File size reached.Cannot transfer file.",
"Undefined Request type.",
"WRQ Request is not handled by the server.",
"Unknown Transaction ID"
};

//RRQ request parameters
struct RRQrequest{
uint16_t opcode;//Operation requested by client for file transfer
char *filename;//Name of file to be transfered
char *mode;//Mode of transfer(octet)
}__attribute__((packed));

//ACK request parameters
struct ACKrequest{
uint16_t opcode;//Code for ACK request
uint16_t blocknumber;//Block number for which ACK is sent
}__attribute__((packed));

//DATA request parameters
struct DATArequest{
uint16_t opcode;//Code for DATA request
uint16_t blockno;//Block number of the data being sent
char *payload;//Actual data
}__attribute__((packed));

//ERROR request parameters
struct ERRORrequest{
uint16_t opcode;//Code for ERROR request
uint16_t errorcode;//Error code to be sent to client
const char *errormsg;//Error message being sent to client
}__attribute__((packed));

//Details storing the file transfer status for a client
struct clientdetails{
uint16_t blockno;//Block number in progress of a cleint
long noofblocks;//Total no of blocks to be sent to client for file transfer
struct sockaddr_in clientaddr,serveraddr;
size_t readresult;//Current number of bytes being transfered
FILE *pFile;//Pointer to file being transfered
struct timeval lastactivitytime;//Recent activity time used for retransmission in case of missing ACK or DATA packets
};

