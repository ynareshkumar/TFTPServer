#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include "TFTPRequestformat.h"

using namespace std; 

#define RECEIVE_BUFFER_SIZE 516  //512 is maximum payload size + 4 bytes of block no and opcode
#define FILE_BLOCK_IN_BYTES 512  //No of bytes the file is divided into so that the block of bytes can be sent to the client.
#define MAX_FILE_SIZE 33553920   //Since Block number is 16 bits,the maximum value of it is 65535. So 65535*512 is the maximum size of file
#define MAX_CLIENTS 100 //Maximum no of concurrent client served.
#define RETRANSMISSION_TIME 4 //Time to wait before retransmission

//Decode the incoming RRQ request
struct RRQrequest parseRRQmsg(char *buf);
//Decode the incoming ACK request
struct ACKrequest parseACKmsg(char *buf);
//Populate the buffer for sending Error message
char *fillerrormsg(int errcode,const char *errmsg);
//Populate the buffer for sending Data message
char *filldatamsg(char *data,int blockno);

int main(int argc, char *argv[])//Arguments is Server IP and Server port
{

struct sockaddr_in serveraddress;//Server address details
int port,sd,newsd,yes = 1;//sd,newsd are the socket descriptors for server and a new client, port denotes the port the server is running
int sendresult;//Return value of sendto function.Used to identify successful delivery of message
int opcode;//Received opcode from the client
char *ipaddress;//Server IP address
struct sockaddr_in remoteaddr; // client address details
struct RRQrequest rrqmsg;//RRQ message
struct ACKrequest ackmsg;//ACK message
socklen_t addrlen;//length of the client address
int receivedbytes;//Return value of receivefrom(). Used to identify successful receipt of message from client.
char *buf = (char *) malloc(RECEIVE_BUFFER_SIZE);//Buffer to store the message from client
char *errorbuf = (char *) malloc(RECEIVE_BUFFER_SIZE);//Buffer that is populated for sending error message to client
char *databuf = (char *) malloc(RECEIVE_BUFFER_SIZE);//Buffer that is populated for sending data message to client
char *data = (char *) malloc(FILE_BLOCK_IN_BYTES);//Buffer that is used to read from the file
long filesize;//Total Size of the file being transfered
struct clientdetails clients[MAX_CLIENTS];//The details of file transfer at a particular instance for a client.
struct timeval timeout,currenttime;//Timeout values and to get current time
fd_set read_fds,master_fds;//file descriptors of master and temporary
int max_fd,selectresult,i;//Maximum number of connections	

if(argc == 3)//Get IP address and port as arguments
	{    
     ipaddress = argv[1]; 	
     port = atoi(argv[2]);    
	}
  else//Force exit
      {
      printf("\n The number of arguments entered is %d",argc-1);
      printf("\n Please enter the 2 arguments in ServerIP Port format \n");
      exit(1); 
      }

//Create a socket 
  sd = socket(AF_INET,SOCK_DGRAM,0);

//Check for any error in socket creation
  if(sd == -1)
    {
       cout<<"\n Unable to create socket";
       exit(1);
    }
  
  cout<<"\n Server Socket created ";

  //Reuse the socket
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  
  addrlen = sizeof(remoteaddr);
  
  //Initialize value to 0 and assign the server details
  memset(&serveraddress,0,sizeof(serveraddress));
  serveraddress.sin_family = AF_INET;
  serveraddress.sin_port = htons(port);//host to network order conversion
  serveraddress.sin_addr.s_addr = inet_addr(ipaddress);
  
  //Assign the socket to a address
  if(bind(sd,(struct sockaddr *)&serveraddress,sizeof(serveraddress)) == -1)
  {
    cout<<"\n Unable to bind socket";
    exit(1);
  }

  cout<<"\n Server Socket is binded";
 
//Set the socket descriptors
  FD_ZERO(&read_fds);  
  FD_ZERO(&master_fds);
  FD_SET(sd, &read_fds);
  max_fd = sd;
  timeout.tv_sec = 0;
  timeout.tv_usec = 1000;

while(1)
{

  master_fds = read_fds;//Copy temporarily to master
  selectresult = select(max_fd+1, &master_fds, NULL,NULL, &timeout);
 
  if(selectresult == -1){		
     perror("select");
     exit(1);
   }
  //cout<<"\n Select done!";
  
  for (i=0;i<=max_fd;i++){	
    //cout<<"\n i is : "<<i<<" ISSET for i is : "<<FD_ISSET(i,&master_fds);
       if(FD_ISSET(i,&master_fds)){
     //cout<<"\n In ISSET";
  	
       //Get current time of day to check for retransmission
       gettimeofday(&currenttime,NULL);
	
	//If the client has not responded within 2s, then retransmit
  	if((currenttime.tv_sec - clients[i].lastactivitytime.tv_sec) >= RETRANSMISSION_TIME && i != sd)
  	{
    	  cout<<"\n Retransmitting..";
	  //Move file pointer to the previous 512 bytes
	  fseek(clients[i].pFile, -clients[i].readresult, SEEK_SET);
          //Read data from file and store in data variable
	  clients[i].readresult = fread(data,1,FILE_BLOCK_IN_BYTES,clients[i].pFile);
          //Construct DATA message packet
	  databuf = filldatamsg(data,clients[i].blockno);      
          //Send to client
	  sendresult = sendto(i,databuf,clients[i].readresult+2 * sizeof(uint16_t),0,(struct sockaddr *)&remoteaddr, addrlen);  
          //Get current time and update it to recent activity time. Used to check for retransmission
          gettimeofday(&clients[i].lastactivitytime,NULL);
          //Make the buffer NULL for reuse
  	  memset(databuf,'\0',RECEIVE_BUFFER_SIZE); 
      	  memset(buf,'\0',RECEIVE_BUFFER_SIZE);
      	  memset(data,'\0',RECEIVE_BUFFER_SIZE);
      	  if(sendresult <= 0)
          {
       		perror("send");
       		exit(1);
      	  }   
        }
  
        else 
 	 {
          //Receive data from client and store in buf variable
 	  receivedbytes = recvfrom(i,buf,RECEIVE_BUFFER_SIZE,0,(struct sockaddr *) &remoteaddr, &addrlen); 
 
	  if(receivedbytes <0)
	  {
	    cout<<"\n Error in receiving from client";
	       exit(1);
	  } 
	  
	  //Get the opcode sent by client. Converted from network to host order
	  opcode = ntohs(*((uint16_t *)buf));

	  //cout<<"\n Received Opcode is : "<<opcode;

  		if(opcode == RRQ)
 		 {    
    
		   //Create a client socket for data flow
		  newsd = socket(AF_INET,SOCK_DGRAM,0);

		  //Check for any error in socket creation
		  if(newsd == -1)
		    {
		       cout<<"\n Unable to create socket";
		       exit(1);
		    }
		  
		  cout<<"\n Socket created for data flow";

		  //Reuse the socket
		  setsockopt(newsd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		  
		  memset(&clients[newsd].serveraddr,0,sizeof(clients[newsd].serveraddr));
		  clients[newsd].clientaddr = remoteaddr;
		  clients[newsd].serveraddr.sin_family = AF_INET;
		  clients[newsd].serveraddr.sin_port = htons(0);  
  
		  //Assign the socket to a address
		  if(bind(newsd,(struct sockaddr *)&clients[newsd].serveraddr,sizeof(clients[newsd].serveraddr)) == -1)
		   {
		    cout<<"\n Unable to bind socket";
		    exit(1);
		   }
		   
		  cout<<"\n Socket binded for data flow.";
                   //Set the socket in file descriptors so that select can monitor for any activity
		   FD_SET(newsd,&read_fds);
		   if(newsd > max_fd)
		      max_fd = newsd;
		      
                    //Decode RRQ message packet
		    rrqmsg = parseRRQmsg(buf);        
		    
		    cout<<"\n Download request from TFTP client for the file "<<rrqmsg.filename;  
                    //Open the file specified by client and return the pointer to the 1st value
		    clients[newsd].pFile = fopen (rrqmsg.filename,"r");
                   //If file is present
		    if (clients[newsd].pFile)
		     {
			      //cout<<"\n File opened successfully"; 
                              //Go to end of file 
			      fseek(clients[newsd].pFile,0,SEEK_END);
                              //Get number of bytes from start to end of file.(i.e) the filesize
			      filesize = ftell(clients[newsd].pFile);
			      //cout<<"\n The file size is :"<<filesize<<" bytes";  
                              //If filesize > MAX size allowable, throw error message to client    
			      if(filesize >= MAX_FILE_SIZE)
			      {
				cout<<"\n Maximum File size reached"; 
                                //Construct error message packet
				errorbuf = fillerrormsg(ENOSPACE,errorcodes[3]);  
                                //Send to the TFTP client
				sendto(newsd,errorbuf,RECEIVE_BUFFER_SIZE,0,(struct sockaddr *)&remoteaddr, addrlen);
                                //Make the uffers to be NULL for reuse
				memset(errorbuf,'\0',RECEIVE_BUFFER_SIZE);  
				memset(buf,'\0',RECEIVE_BUFFER_SIZE);
                                //Clear from the socket descriptors 
				FD_CLR(newsd,&read_fds);
				close(newsd);
				bzero(&clients[newsd],sizeof(struct clientdetails));
				continue;  
			      }
			      //Move the file pointer in the file again to start of file
			      rewind (clients[newsd].pFile);        
                              //Calculate the number of blocks that is needed to sent for the file
			      clients[newsd].noofblocks = ceil((float)filesize/FILE_BLOCK_IN_BYTES);
			      // copy 512 bytes into the buffer data
			      clients[newsd].readresult = fread(data,1,FILE_BLOCK_IN_BYTES,clients[newsd].pFile); 
                              //RRQ message starts the first transfer of block           
			      clients[newsd].blockno = 1;
			      databuf = filldatamsg(data,clients[newsd].blockno); 
                              //Update the recnt activity to current time    
			      gettimeofday(&clients[newsd].lastactivitytime,NULL);  
			      sendresult = sendto(newsd,databuf,clients[newsd].readresult+2 * sizeof(uint16_t),0,(struct sockaddr *)&remoteaddr, addrlen);  			         
			      memset(databuf,'\0',RECEIVE_BUFFER_SIZE); 
			      memset(buf,'\0',RECEIVE_BUFFER_SIZE);
			      memset(data,'\0',RECEIVE_BUFFER_SIZE);
			      if(sendresult <= 0)
			      {
			       perror("send");
			       exit(1);
			      }   
		     }
   		    else//Error in opening file(File not found,Access denied)
   		    {
			    cout<<"\n Error in opening file";
                            cout<<"\n";
                            //File not found scenario
			    if(errno == ENOENT)
			    {
			     errorbuf = fillerrormsg(ENOTFOUND,errorcodes[1]);      
			    }
                            //Access denied scenario
			    else if(errno == EACCES)
			    {
			     errorbuf = fillerrormsg(EACCESS,errorcodes[2]);  
			    }            
                            //Send to client and close connection	   	    	      
			    sendto(newsd,errorbuf,RECEIVE_BUFFER_SIZE,0,(struct sockaddr *)&remoteaddr, addrlen);                    
			    memset(errorbuf,'\0',RECEIVE_BUFFER_SIZE);  
			    memset(buf,'\0',RECEIVE_BUFFER_SIZE); 
			    FD_CLR(newsd,&read_fds);
			    close(newsd);
			    bzero(&clients[newsd],sizeof(struct clientdetails));
			    continue;
		   }
    
                }
  		else if(opcode == ACK)
  		{    
                   //Decode ACK message
		   ackmsg = parseACKmsg(buf);
		   //cout<<"\n ACK received for block number "<<ackmsg.blocknumber;
                  //Verify if data is received from same port. If not, its a wrong call from client
		   if(clients[i].clientaddr.sin_port != remoteaddr.sin_port)
		   {
		       cout<<"\n Wrong TID";
		       errorbuf = fillerrormsg(EBADID,errorcodes[6]); 
		       sendto(i,errorbuf,RECEIVE_BUFFER_SIZE,0,(struct sockaddr *)&remoteaddr, addrlen);
		       memset(errorbuf,'\0',RECEIVE_BUFFER_SIZE);  
		       memset(buf,'\0',RECEIVE_BUFFER_SIZE); 
		       continue;
		   }
                  //Check if we are getting ACK for last sent frame
		   if(clients[i].blockno == ackmsg.blocknumber)
		   {    
                     //Check if it is the last ACK that we are receiving from client. 
		     if(clients[i].blockno == clients[i].noofblocks+1)
		     {
			      cout<<"\n Last ACK received.So closing connection of the TFTP client";
                              cout<<"\n";
			      clients[i].blockno = 0;
			      fclose(clients[i].pFile);    
			      memset(buf,'\0',RECEIVE_BUFFER_SIZE);      
			      FD_CLR(i,&read_fds);
			      close(i);
			      bzero(&clients[i],sizeof(struct clientdetails));    
		     }
		     else//If it is not the last block, increment block number and send next data
		     {
		      	 clients[i].blockno++;
                         //Read 512 bytes from file
			 clients[i].readresult = fread(data,1,FILE_BLOCK_IN_BYTES,clients[i].pFile);
			 databuf = filldatamsg(data,clients[i].blockno); 
                         //Update recent activity time
			 gettimeofday(&clients[newsd].lastactivitytime,NULL);
			 sendresult = sendto(i,databuf,clients[i].readresult+2 * sizeof(uint16_t),0,(struct sockaddr *)&remoteaddr, addrlen);             
			 memset(databuf,'\0',RECEIVE_BUFFER_SIZE); 
			 memset(buf,'\0',RECEIVE_BUFFER_SIZE);
			 memset(data,'\0',RECEIVE_BUFFER_SIZE);
			 if(sendresult <= 0)
			  {
			    perror("send");
			    exit(1);
			  }     	
		     }
		   }
   
                 }
  
		  else if(opcode == WRQ)
		  {//If receives opcode is for WRQ, send message to client stating it is not handled
		   cout<<"\n It is a WRQ request with opcode "<<opcode<<".So the request is discarded";
		   errorbuf = fillerrormsg(EUNDEF,errorcodes[5]);     
		   sendto(i,errorbuf,RECEIVE_BUFFER_SIZE,0,(struct sockaddr *)&remoteaddr, addrlen);
		   memset(errorbuf,'\0',RECEIVE_BUFFER_SIZE);  
		   memset(buf,'\0',RECEIVE_BUFFER_SIZE); 
		   continue;   
		  }
		  else
		   {
                   //For unknown opcode, send error message stating undefined request to client
		   cout<<"\n Undefined TFTP Request type";
		   errorbuf = fillerrormsg(EBADOP,errorcodes[4]);     
		   sendto(i,errorbuf,RECEIVE_BUFFER_SIZE,0,(struct sockaddr *)&remoteaddr, addrlen);
		   memset(errorbuf,'\0',RECEIVE_BUFFER_SIZE);  
		   memset(buf,'\0',RECEIVE_BUFFER_SIZE); 
		   continue;
		   }
  	 }//End of else of no resend
       }//End of ISSET
     }//for max_fd
   }//Infinite loop
 
return 0;
}

/*

Description: Decode the incoming RRQ request
Args: Char pointer which contains the RRQ message details
Returns: struct of RRQrequest datatype with data members appropriately populated

*/
 struct RRQrequest parseRRQmsg(char *buf)
   {
	struct RRQrequest rrqmessage;
        memset(&rrqmessage,0,sizeof(rrqmessage));        
 
	rrqmessage.opcode = ntohs(*((uint16_t *)buf)); 
	rrqmessage.filename = &buf[sizeof(uint16_t)];
	rrqmessage.mode = &buf[sizeof(uint16_t)+strlen(rrqmessage.filename)+1];
	return rrqmessage;
   }

/*

Description: Decode the incoming ACK request
Args: Char pointer which contains the ACK message details
Returns: struct of ACKrequest datatype with data members appropriately populated

*/
 struct ACKrequest parseACKmsg(char *buf)
   {
	struct ACKrequest ackmsg;
        memset(&ackmsg,0,sizeof(ackmsg));
	ackmsg.opcode = ntohs(*((uint16_t *)buf)); 
	ackmsg.blocknumber = ntohs(*((uint16_t *)(buf + sizeof(uint16_t))));	

	return ackmsg;
   }

/*

Description: Populate the buffer for sending Error message
Args: Error code and error message to populate
Returns: Char pointer with the values populated

*/
 char *fillerrormsg(int errcode,const char *errmsg)
   {

	struct ERRORrequest errormessage;
	char *errorbuf = (char *) malloc(RECEIVE_BUFFER_SIZE);

	  errormessage.opcode = htons(ERROR); 
	  errormessage.errorcode = htons(errcode);
	  errormessage.errormsg = errmsg;
	*((uint16_t *)errorbuf) = errormessage.opcode;
	*((uint16_t *)(errorbuf+sizeof(uint16_t))) = errormessage.errorcode;
	memcpy((errorbuf+2 * sizeof(uint16_t)),errormessage.errormsg,strlen(errormessage.errormsg));

	return errorbuf;

    }

/*

Description: Populate the buffer for sending Data message
Args: The data of one block of file(512 bytes atmost) and block number of the packet
Returns: Char pointer with the values populated

*/
 char *filldatamsg(char *data,int blockno)
   {
        struct DATArequest datamsg;
        char *databuf = (char *) malloc(RECEIVE_BUFFER_SIZE);

        datamsg.opcode = htons(DATA); 
	datamsg.blockno = htons(blockno);
	datamsg.payload = data;

        *((uint16_t *)databuf) = datamsg.opcode;
	*((uint16_t *)(databuf+sizeof(uint16_t))) = datamsg.blockno;        
	memcpy((databuf+2 * sizeof(uint16_t)),datamsg.payload,FILE_BLOCK_IN_BYTES);

	return databuf;

   }

