CFLAGS = -Wall -g
CC = gcc

all: TCP_receiver TCP_sender RUDP_receiver RUDP_sender

TCP_receiver: TCP_Receiver.o
	$(CC) $(CFLAGS) TCP_Receiver.o -o TCP_receiver

TCP_sender: TCP_Sender.o
	$(CC) $(CFLAGS) TCP_Sender.o -o TCP_sender

RUDP_receiver: RUDP_Receiver.o RUDP.o
	$(CC) $(CFLAGS) RUDP_Receiver.o RUDP.o -o RUDP_receiver

RUDP_sender: RUDP_Sender.o RUDP.o
	$(CC) $(CFLAGS) RUDP_Sender.o RUDP.o -o RUDP_sender

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o TCP_receiver TCP_sender RUDP_receiver RUDP_sender



# ./TCP_receiver -p 1234 -algo reno
# ./TCP_sender -ip 127.0.0.1 -p 1234  -algo reno
# ./RUDP_receiver -p 1234
# ./RUDP_sender -ip 127.0.0.1 -p 1234