/*
** serverB.c -- UDP ServerB
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define UDP_PORT 	"22982"  
#define MAXDATASIZE 10240
#define ONELINESIZE 50
#define FILE_PATH 	"block2.txt"

#define CHECK_WALLET 0
#define TXCOINS      1
#define WRITE_LOGS 	 2
#define TXLIST 	     3

int main(void)
{
	int sockfd;  							// udp sock_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; 	// connector's address information
	socklen_t addr_len;						// length of their_addr
	char recv_buffer[MAXDATASIZE];			// recvfrom buffer
	char send_buffer[MAXDATASIZE]; 			// all data

	/// Get ServerB's socket address (UDP)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;			// IPv6
	hints.ai_socktype = SOCK_DGRAM;		// SOCK_STREAM for TCP; SOCK_DGRAM for UDP
	hints.ai_flags = AI_PASSIVE; 		// use my IP
	int ret = getaddrinfo(NULL, UDP_PORT, &hints, &servinfo);
	if (ret != 0) 
	{
		fprintf(stderr, "ServerB getaddrinfo(): %s\n", gai_strerror(ret));
		exit(-1);
	}

	/// Bind()
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
		{
			perror("ServerB socket(): FAILED\n");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("ServerB bind(): FAILED\n");
			continue;
		}
		break;
	}
	freeaddrinfo(servinfo);
	if (p == NULL)  
	{
		perror("ServerB failed to bind.\n");
		exit(-1);
	}

	/// Print out booting up info
	fprintf(stdout, "The ServerB is up and running using UDP on port %s.\n", UDP_PORT);
	

	/// Main loop
	while(1) 
	{  
		/// receive request from main server
		addr_len = sizeof(their_addr);
		int numbytes = recvfrom(sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
		if (numbytes == -1) 
		{
			perror("ServerB recvfrom(): FAILED\n");
			exit(-1);
		}
		recv_buffer[numbytes] = '\0';
		// printf("ServerB: packet contains \"%s\"\n", recv_buffer);

		/// extract usenames in request
		int op = recv_buffer[0] - '0';
		if (TXLIST != op)
			fprintf(stdout, "The ServerB received a request from the Main Server.\n");
		char *username1 = NULL;
		char *username2 = NULL;
		if (CHECK_WALLET == op)
		{
			username1 = &recv_buffer[2];
			// printf("username1: %s\n", username1);
		}
		else if (TXCOINS == op)
		{
			username1 = &recv_buffer[2];
			char *tab_ptr = strchr(username1, '\t');
			*tab_ptr = '\0';
			username2 = tab_ptr + 1;
			// printf("username1: %s, username2: %s\n", username1, username2);
		}
		else if (WRITE_LOGS == op)
		{
			FILE *w_fp = fopen(FILE_PATH, "a+");
			if (NULL == w_fp)
			{
				perror("ServerB fopen(): FAILED\n");
				exit(-1);
			}
			// printf("%s", &recv_buffer[2]);
			fprintf(w_fp, "\n%s", &recv_buffer[2]);
			fclose(w_fp);
			send_buffer[0] = '1';
			send_buffer[1] = '\0';
			numbytes = sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&their_addr, addr_len);
			if (numbytes == -1) 
			{
				perror("ServerB sendto(): FAILED\n");
				exit(-1);
			}
			fprintf(stdout, "The ServerB finished sending the response to the Main Server.\n");
			continue;
		}
		else if (TXLIST == op) {}
		else
		{
			perror("ServerB: Invalid Service\n");
			exit(-1);
		}
		
		/// open block.txt
		FILE *fp = fopen(FILE_PATH, "r+");
		if (NULL == fp)
		{
			perror("ServerB fopen(): FAILED\n");
			exit(-1);
		}

		/// extract info from logs
		send_buffer[0] = '\0';
		char buf[ONELINESIZE]; 			// one line length
		int max_serial_no = -1;
        while(fgets(buf, sizeof(buf), fp) != NULL) 
		{
			if (strlen(buf) < 8)
				continue;

			char tmp[ONELINESIZE]; 			
			strcpy(tmp, buf);
			char *log_no = tmp;
			char *space_ptr = strchr(tmp, ' ');
			*space_ptr = '\0';
			char *log_usename1 = space_ptr + 1;
			space_ptr = strchr(log_usename1, ' ');
			*space_ptr = '\0';
			char *log_usename2 = space_ptr + 1;
			space_ptr = strchr(log_usename2, ' ');
			*space_ptr = '\0';

			int serial_no;
			sscanf(log_no, "%d", &serial_no);
			max_serial_no = serial_no > max_serial_no ? serial_no : max_serial_no;
			// printf("max_serial_no: %d\n", max_serial_no);

			if (CHECK_WALLET == op)
			{
				if (strcmp(username1, log_usename1) == 0 || strcmp(username1, log_usename2) == 0)
					strcat(send_buffer, buf);
			}
			else if (TXCOINS == op)
			{
				if (strcmp(username1, log_usename1) == 0 || 
					strcmp(username1, log_usename2) == 0 || 
					strcmp(username2, log_usename1) == 0 || 
					strcmp(username2, log_usename2) == 0)
					strcat(send_buffer, buf);
			}
			else if (TXLIST == op)
			{
				strcat(send_buffer, buf);
			}
			else
			{
				perror("ServerB: Invalid Service\n");
				exit(-1);
			}
			
		}
		int len = strlen(send_buffer);
		if (len > 0 && send_buffer[len-1] == '\n')
			send_buffer[len-1] = '\0';
		
		if (TXCOINS == op && 0 != len)
		{
			len = strlen(send_buffer);
			send_buffer[len] = '\n';
			send_buffer[len+1] = '\0';
			char max_log_no[10]; 
			sprintf(max_log_no,"%d", max_serial_no);
			strcat(send_buffer, max_log_no);
		}
		// printf("sent data: %s\n", send_buffer);

		/// close file descriptor
		fclose(fp);
		
		/// send data to main server
		numbytes = sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&their_addr, addr_len);
		if (numbytes == -1) 
		{
			perror("ServerB sendto(): FAILED\n");
			exit(-1);
		}
		if (TXLIST != op)
			fprintf(stdout, "The ServerB finished sending the response to the Main Server.\n");
	}

	return 0;
}
