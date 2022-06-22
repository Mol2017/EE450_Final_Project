/*
** clientB.c -- stream socket client B
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT        "26982"     // port number of main server client will connect to
#define MAXDATASIZE 10240       // max number of bytes we can get at once 

#define CHECK_WALLET 0
#define TXCOINS      1
#define WRITE_LOGS 	 2
#define TXLIST 	     3

int main(int argc, char *argv[])
{
	int sockfd;                             // TCP socket file descriptor
	struct addrinfo hints, *servinfo, *p;   
	char recv_buffer[MAXDATASIZE];          // recv buffer
    char send_buffer[MAXDATASIZE];
    int op = -1;                            // what service?

    /// Check input argument and determine service
    if (argc == 2)
    {
        if (0 == strcmp(argv[1], "TXLIST"))
            op = TXLIST;
        else
            op = CHECK_WALLET;
    }
    else if (argc == 4)
        op = TXCOINS;
	else
    {
	    fprintf(stderr,
        "Invalid Usage!\n./clientB <username1>\n./clientB <username1> <username2> <transfer amount>\n");
	    exit(-1);
	}

    /// Set up socket address (TCP)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
    int ret = getaddrinfo("localhost", PORT, &hints, &servinfo);
	if (ret != 0) 
    {
		fprintf(stderr, "ClientB getaddrinfo(): %s\n", gai_strerror(ret));
		exit(-1);
	}

	/// Connect()
	for(p = servinfo; p != NULL; p = p->ai_next) 
    {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
        {
			perror("ClientB socket(): FAILED\n");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("ClientB connect(): FAILED\n");
			close(sockfd);
			continue;
		}
		break;
	}
	if (p == NULL) {
		perror("ClientB failed to connect.\n");
		exit(-1);
	}
	freeaddrinfo(servinfo); // all done with this structure

    /// Print out booting up info
    fprintf(stdout, "The client B is up and running.\n");

    /// Send() + Recv()
    if (op == CHECK_WALLET)
    {
        send_buffer[0] = op + '0';
        send_buffer[1] = '\t';
        strcpy(&send_buffer[2], argv[1]);

        if (send(sockfd, send_buffer, strlen(send_buffer), 0) == -1)
        {
            perror("ClientB send(): FAILED\n");
	        exit(-1);
        }
        fprintf(stdout, "\"%s\" sent a balance enquiry request to the main server.\n", argv[1]);
        
        // Recv()
        int numbytes;  
        if ((numbytes = recv(sockfd, recv_buffer, MAXDATASIZE-1, 0)) == -1) {
            perror("ClientB recv(): FAILED\n");
            exit(-1);
        }
        recv_buffer[numbytes] = '\0';

        if (recv_buffer[0] == '0')
            fprintf(stdout, "Unable to proceed with the request as \"%s\" is not part of the network.\n", argv[1]);   
        else if (recv_buffer[0] == '1')
            fprintf(stdout, "The current balance of \"%s\" is : %s alicoins.\n", argv[1], &recv_buffer[2]); 
        else
        {
            perror("ClientB recv_buffer is invalid.\n");
	        exit(-1);
        }  
        // printf("client: received '%s'\n",recv_buffer);
    }
    else if (op == TXCOINS)
    {
        send_buffer[0] = op + '0';
        send_buffer[1] = '\t';
        int tail_idx = 2;
        for (int i = 0; i < 3; i++)
        {
            strcpy(&send_buffer[tail_idx], argv[i+1]);
            tail_idx = strlen(send_buffer);
            send_buffer[tail_idx++] = '\t';     
        }
        send_buffer[--tail_idx] = '\0';

        if (send(sockfd, send_buffer, strlen(send_buffer), 0) == -1)
        {
            perror("ClientB send(): FAILED\n");
	        exit(-1);
        }
        fprintf(stdout, 
        "\"%s\" has requested to transfer %s coins to \"%s\".\n", argv[1], argv[3], argv[2]);

        // Recv()
        int numbytes;  
        if ((numbytes = recv(sockfd, recv_buffer, MAXDATASIZE-1, 0)) == -1) {
            perror("ClientB recv(): FAILED\n");
            exit(-1);
        }
        recv_buffer[numbytes] = '\0';

        // sender 1; receiver: 2; amount: 3
        if (recv_buffer[0] == '0')
        {
            fprintf(stdout, "\"%s\" successfully transferred %s alicoins to \"%s\".\n", argv[1], argv[3], argv[2]);
            fprintf(stdout, "The current balance of \"%s\" is : %s alicoins.\n", argv[1], &recv_buffer[2]); 
        }
        else if (recv_buffer[0] == '1')
        {
            fprintf(stdout, "\"%s\" was unable to transfer %s alicoins to \"%s\" because of insufficient balance.\n", argv[1], argv[3], argv[2]);
            fprintf(stdout, "The current balance of \"%s\" is : %s alicoins\n", argv[1], &recv_buffer[2]); 
        }
        else if (recv_buffer[0] == '2')
            fprintf(stdout, "Unable to proceed with the transaction as \"%s\" is not part of the network.\n", argv[1]); 
        else if (recv_buffer[0] == '3')
            fprintf(stdout, "Unable to proceed with the transaction as \"%s\" is not part of the network.\n", argv[2]); 
        else if (recv_buffer[0] == '4')
            fprintf(stdout, "Unable to proceed with the transaction as \"%s\" and \"%s\" are not part of the network.\n",  argv[1], argv[2]);
        else
        {
            perror("ClientB recv_buffer is invalid.\n");
	        exit(-1);
        }
        // printf("client: received '%s'\n",recv_buffer);
    }
    else if (op == TXLIST)
    {
        send_buffer[0] = op + '0';
        send_buffer[1] = '\0';
        if (send(sockfd, send_buffer, strlen(send_buffer), 0) == -1)
        {
            perror("ClientB send(): FAILED\n");
	        exit(-1);
        }
        fprintf(stdout, "\"client B\" sent a sorted list request to the main server.\n");
        
        // Recv()
        int numbytes;  
        if ((numbytes = recv(sockfd, recv_buffer, MAXDATASIZE-1, 0)) == -1) {
            perror("ClientA recv(): FAILED\n");
            exit(-1);
        }
        recv_buffer[numbytes] = '\0';

        if (recv_buffer[0] == '0')
            fprintf(stdout, "Unable to generate \"alichain.txt\".\n");   
        else if (recv_buffer[0] == '1')
            fprintf(stdout, "\"alichain.txt\" is successfully generated.\n"); 
        else
        {
            perror("ClientA recv_buffer is invalid.\n");
	        exit(-1);
        }  
    }
    else
    {
        perror("ClientB: Invalid Service\n");
	    exit(-1);
    }

    /// close()
	close(sockfd);

	return 0;
}
