all: tcpclient tcpserver

tcpclient: tcpclient.c
	gcc tcpclient.c -o tcpclient

tcpserver: tcpserver.c
	gcc tcpserver.c -o tcpserver

clean: 
	rm tcpclient
	rm tcpserver