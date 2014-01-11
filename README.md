Overview:		Implementation 30/09/2013
----------

Trivial File Transfer Protocol(TFTP) is used for sending files between a client and server. It is a simple application implemented over UDP and does not involve any authentication mechanism. The client is the native TFTP client.

System Requirements:
---------------------
The application should be executed in the Linux Environment. g++ should be installed in the machine for compiling the .cpp files.

Features of the TFTP Server :
------------------------------

1.The TFTP server can handle concurrent file transfers from multiple clients.
2.The maximum file size that it can transfer is about 33.5 MB.
3.The TFTP server allows for the retrieval of files from the server directory.
4.The server cannot handle the write requests from the native TFTP client.

Error Scenarios:
-----------------

1.When the file requested is not found, the client will get an error message stating File not found.
2.When the file requested does not have necssary permissions, the client will get access denied.
3.When the file requested exceeds the maximum transferable file size, the client will get an error message stating Maximum file size reached.

Client side Commands:
----------------------

1.Run the TFTP client from the client directory.
  tftp <Server-IP> <Server-port>
2.On prompt, enter the mode as octet.
  mode octet
3.Now the client can request for the files.
  get <filename>

How to run the Application:
----------------------------

1. Copy TFTPServer.cpp,TFTPRequestformat.h and makefile to a folder.
2. Go to that folder in terminal and type make.
3. Run the server by executing ./TFTPServer <Server-IP> <Server-Port>
   Eg: ./TFTPServer 127.0.0.1 5000
4. Now the client can request for the files by connecting to the Server IP and port.

