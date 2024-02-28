CFLAGS= -Wall -g 
CC= gcc  

all: receiver sender

receiver: TCP_Receiver.o
	$(CC) $(CFLAGS) TCP_Receiver.o -o receiver



sender:	TCP_sender.o
	$(CC) $(CFLAGS) TCP_sender.o -o sender

clean: 
	rm -f *.o receiver sender


# ./receiver -p 1234 -algo reno
# ./sender -ip 127.0.0.1 -p 1234  -algo reno