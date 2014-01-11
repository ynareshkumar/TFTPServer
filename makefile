all:TFTPServer
TFTPServer: TFTPServer.o TFTPRequestformat.h
	g++ -Wall -o TFTPServer TFTPServer.o 
clean:
	rm -f *.o
	rm -f TFTPServer
