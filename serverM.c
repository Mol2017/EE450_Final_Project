/*
** serverM.c -- Main Server
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#define UDP_PORT 	"24982"		// server M
#define UDP_PORT_A 	"21982"		// server A
#define UDP_PORT_B 	"22982"		// server B
#define UDP_PORT_C 	"23982"		// server C

#define TCP_PORT_A 	"25982" 
#define TCP_PORT_B 	"26982" 

#define BACKLOG 20				// how many pending connections queue will hold
#define MAXDATASIZE 10240       // max number of bytes we can get at once 
#define ONELINESIZE 50

#define CHECK_WALLET 0
#define TXCOINS      1
#define WRITE_LOGS 	 2
#define TXLIST 	     3

#define SORTED_LOGS_FILE_PATH 	"alichain.txt"


struct addrinfo hints;
struct addrinfo *p;
struct addrinfo *servinfo;
int yes=1;								// setsockopt() argument 

// initialize UDP socket (socket() + bind())
int init_UDP_socket(char *port_num)
{
	int sockfd;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;			// IPv6
	hints.ai_socktype = SOCK_DGRAM;		// SOCK_STREAM for TCP; SOCK_DGRAM for UDP
	hints.ai_flags = AI_PASSIVE; 		// use my IP
	int ret = getaddrinfo(NULL, port_num, &hints, &servinfo);
	if (ret != 0) 
	{
		fprintf(stderr, "ServerM getaddrinfo(): %s\n", gai_strerror(ret));
		exit(-1);
	}
	/// Bind UDP socket
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
		{
			perror("ServerM socket(): FAILED\n");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("ServerM bind(): FAILED\n");
			continue;
		}
		break;
	}
	if (p == NULL)  
	{
		perror("ServerM failed to bind UDP socket.\n");
		exit(-1);
	}
	freeaddrinfo(servinfo);	// all done with this structure
	return sockfd;
}

// initialize TCP socket (socket() + bind() + listen())
int init_TCP_socket(char *port_num)
{
	int sockfd;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;			// IPv6
	hints.ai_socktype = SOCK_STREAM;	// SOCK_STREAM for TCP; SOCK_DGRAM for UDP
	hints.ai_flags = AI_PASSIVE; 		// use my IP
	int ret = getaddrinfo(NULL, port_num, &hints, &servinfo);
	if (ret != 0) 
	{
		fprintf(stderr, "ServerM getaddrinfo(): %s.\n", gai_strerror(ret));
		exit(-1);
	}
	/// Bind TCP socket A
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
		{
			perror("ServerM socket(): FAILED\n");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
		{
			perror("ServerM setsockopt(): FAILED\n");
			exit(-1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
		{
			close(sockfd);
			perror("ServerM bind(): FAILED\n");
			continue;
		}
		break;
	}
	if (p == NULL)  
	{
		perror("ServerM failed to bind TCP socket A.\n");
		exit(-1);
	}
	freeaddrinfo(servinfo); // all done with this structure
	/// listen()
	if (listen(sockfd, BACKLOG) == -1) {
		perror("ServerM listen(): FAILED\n");
		exit(-1);
	}
	return sockfd;
}

// get server UDP socket address
struct addrinfo *get_server_socket_address(char *port_num)
{
	struct addrinfo *servinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	int ret = getaddrinfo("localhost", port_num, &hints, &servinfo);
	if (ret != 0) 
	{
		fprintf(stderr, "ServerM getaddrinfo(): %s.\n", gai_strerror(ret));
		exit(-1);
	}
	if (servinfo == NULL)
	{
		perror("ServerM failed to get serverA's address.\n");
		exit(-1);
	}
	return servinfo;
}

// sorting algo
int get_index(char *ptr)
{
	char tmp[ONELINESIZE];
	strcpy(tmp, ptr);
	char *space_ptr = strchr(tmp, ' ');
	*space_ptr = '\0';
	int ret;
	sscanf(tmp, "%d", &ret);
	return ret;
}

int partition (char *arr[], int low, int high)
{
    int pivot = get_index(arr[high]); // pivot
    int i = (low - 1); // Index of smaller element and indicates the right position of pivot found so far
	char *tmp;

    for (int j = low; j <= high - 1; j++)
    {
        // If current element is smaller than the pivot
        if (get_index(arr[j]) < pivot)
        {
            i++; // increment index of smaller element

			tmp = arr[j];
			arr[j] = arr[i];
			arr[i] = tmp;
        }
    }
	tmp = arr[i + 1];
	arr[i + 1] = arr[high];
	arr[high] = tmp;
    return (i + 1);
}

void quick_sort_logs(char *logs[], int low, int high)
{
	/*
	for (int i = low; i <= high; i++)
		printf("%s\n", logs[i]);
	*/
	if (low < high)
	{
		int pi = partition(logs, low, high);
		quick_sort_logs(logs, low, pi-1);
		quick_sort_logs(logs, pi+1, high);
	}
}


int main(void)
{
	fd_set master;    						// master file descriptor list
    fd_set read_fds; 		 				// temp file descriptor list for select()
	int fdmax;								// record max fd

	int udp_sockfd;							// UDP socket 1) bind 2) sendto 3) recvfrom
	int tcp_sockfd_A;						// TCP parent socket A
	int tcp_sockfd_B;						// TCP parent socket B
	int child_fd;

	struct addrinfo *servinfo_A;
	struct addrinfo *servinfo_B;
	struct addrinfo *servinfo_C;

	struct sockaddr_storage their_addr; 	// used by TCP accept() or UDP recvfrom()
	socklen_t addr_len;						// length of their_addr
	
	char recv_buffer[MAXDATASIZE];			// recv buffer
	char send_buffer[MAXDATASIZE];

	int ret;


	/// clear the master and temp sets
	FD_ZERO(&master);    					
    FD_ZERO(&read_fds);

	/// Set up UDP socket
	udp_sockfd = init_UDP_socket(UDP_PORT);

	/// Set up TCP socket A
	tcp_sockfd_A = init_TCP_socket(TCP_PORT_A);
	FD_SET(tcp_sockfd_A, &master);

	/// Set up TCP socket B
	tcp_sockfd_B = init_TCP_socket(TCP_PORT_B);
	FD_SET(tcp_sockfd_B, &master);
	fdmax = tcp_sockfd_A > tcp_sockfd_B ? tcp_sockfd_A : tcp_sockfd_B;

	/// Get serverA's socket address (UDP)
	servinfo_A = get_server_socket_address(UDP_PORT_A);
	servinfo_B = get_server_socket_address(UDP_PORT_B);
	servinfo_C = get_server_socket_address(UDP_PORT_C);

	/// Print out booting up info
	fprintf(stdout, "The main server is up and running.\n");

	/// Main loop
	while(1) 
	{  	
		read_fds = master; // copy it
        if ((ret = select(fdmax+1, &read_fds, NULL, NULL, NULL)) == -1) 
		{
            perror("ServerM select(): FAILED\n");
            exit(-1);
        }
		// printf("ServerM: select() returns %d\n", ret);

		
		for (int i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &read_fds))
			{
				/// Accept()
				addr_len = sizeof(their_addr);
				child_fd = accept(i, (struct sockaddr *)&their_addr, &addr_len);
				if (child_fd == -1) 
				{
					perror("ServerM accept(): FAILED\n");
					continue;
				}
				
				/// Receive request from client
				int numbytes = -1;  
				if ((numbytes = recv(child_fd, recv_buffer, MAXDATASIZE-1, 0)) == -1) 
				{
					perror("ServerM recv(): FAILED\n");
					exit(-1);
				}
				recv_buffer[numbytes] = '\0';
				// printf("ServerM: received \"%s\"\n",recv_buffer); // debug

				/// Contact with Server A, B, C
				int op = recv_buffer[0] - '0'; 	// operation
				struct sockaddr_in sin;			// port number
				socklen_t len = sizeof(sin);
				if (getsockname(child_fd, (struct sockaddr *)&sin, &len) == -1)
				{
					perror("ServerM getsockname(): FAILED\n");
					exit(-1);
				}
				char tcp_connection_port[10]; 
				sprintf(tcp_connection_port,"%d", ntohs(sin.sin_port));
				// printf("tcp_connection_port: %s\n", tcp_connection_port);

				if (CHECK_WALLET == op)
				{
					char data[MAXDATASIZE * 3];	// aggregate info
					data[0] = '\0';

					char tmp_buffer[MAXDATASIZE];
					strcpy(tmp_buffer, recv_buffer);
					char *username = &tmp_buffer[2];
					fprintf(stdout, 
					"The main server received input=\"%s\" from the client using TCP over port %s.\n", 
					username, tcp_connection_port);
					/// Contact server A
					// send request to server A
					if (sendto(udp_sockfd, tmp_buffer, strlen(tmp_buffer), 0, servinfo_A->ai_addr, servinfo_A->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					fprintf(stdout, "The main server sent a request to server A.\n");
					// receive logs to server A
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					fprintf(stdout, "The main server received transactions from Server A using UDP over port %s.\n", UDP_PORT_A);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// Contact server B
					// send request to server B
					if (sendto(udp_sockfd, tmp_buffer, strlen(tmp_buffer), 0, servinfo_B->ai_addr, servinfo_B->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					fprintf(stdout, "The main server sent a request to server B.\n");
					// receive logs to server B
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					fprintf(stdout, "The main server received transactions from Server B using UDP over port %s.\n", UDP_PORT_B);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// Contact server C
					// send request to server C
					if (sendto(udp_sockfd, tmp_buffer, strlen(tmp_buffer), 0, servinfo_C->ai_addr, servinfo_C->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					fprintf(stdout, "The main server sent a request to server C.\n");
					// receive logs to server C
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					fprintf(stdout, "The main server received transactions from Server C using UDP over port %s.\n", UDP_PORT_C);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// send data to client
					char client_id = (0 == strcmp(tcp_connection_port, TCP_PORT_A)) ? 'A' : 'B';
					if (0 == strlen(data))
					{
						send_buffer[0] = '0';
						send_buffer[1] = '\0';
						if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
						{
							perror("ServerM send(): FAILED\n");
							exit(-1);
						}
						fprintf(stdout, "Username was not found on database.\n");
					}
					else
					{
						// printf("data:\n%s\n", data);
						int balance = 1000;
						char *cur_transaction = data;
						char *newline_ptr = strchr(data, '\n');
						while (newline_ptr != NULL)
						{
							*newline_ptr = '\0';
							char *log_name1 = strchr(cur_transaction, ' ') + 1;
							char *space_ptr = strchr(log_name1, ' ');
							*space_ptr = '\0';
							char *log_name2 = space_ptr + 1;
							space_ptr = strchr(log_name2, ' ');
							*space_ptr = '\0';
							char *log_amount = space_ptr + 1;
							int amount;
							sscanf(log_amount, "%d", &amount);

							if (0 == strcmp(username, log_name1))
								balance -= amount;
							
							if (0 == strcmp(username, log_name2))
								balance += amount;

							cur_transaction = newline_ptr + 1;
							newline_ptr = strchr(cur_transaction, '\n');
						}
						// printf("balance: %d\n", balance);
						char cur_balance[10]; 
						sprintf(cur_balance,"%d", balance);
						send_buffer[0] = '1';
						send_buffer[1] = '\t';
						send_buffer[2] = '\0';
						strcat(send_buffer, cur_balance);
						if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
						{
							perror("ServerM send(): FAILED\n");
							exit(-1);
						}
						fprintf(stdout, "The main server sent the current balance to client %c.\n", client_id);

					}
					// printf("data:\n%s\n", data);
				}
				else if (TXCOINS == op)
				{
					char data[MAXDATASIZE * 3];	// aggregate info
					data[0] = '\0';

					int max_serial_no = -1;

					// extract useful info
					char tmp_buffer[MAXDATASIZE];	// recv_buffer e.g. 1\tname1\tname2\tamount
					strcpy(tmp_buffer, recv_buffer);
					char *sender_username = &tmp_buffer[2];
					char *tab_ptr = strchr(sender_username, '\t');
					*tab_ptr = '\0';
					char *receiver_username = tab_ptr + 1;
					tab_ptr = strchr(receiver_username, '\t');
					*tab_ptr = '\0';
					char *transfer_amount = tab_ptr + 1;
					fprintf(stdout, 
					"The main server received from \"%s\" to transfer %s coins to \"%s\" using TCP over port %d.\n", 
					sender_username, transfer_amount, receiver_username, ntohs(sin.sin_port));
					
					strcpy(send_buffer, recv_buffer);
					char *last_tab_ptr = strrchr(send_buffer, '\t');
					*last_tab_ptr = '\0';

					/// Contact server A
					// send request to server A
					if (sendto(udp_sockfd, send_buffer, strlen(send_buffer), 0, servinfo_A->ai_addr, servinfo_A->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					fprintf(stdout, "The main server sent a request to server A.\n");
					// receive logs to server A
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					fprintf(stdout, "The main server received the feedback from Server A using UDP over port %s.\n", UDP_PORT_A);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						char *nl_ptr = strrchr(recv_buffer, '\n');
						*nl_ptr = '\0';
						char *log_serial_no = nl_ptr + 1;
						int no;
						sscanf(log_serial_no, "%d", &no);
						max_serial_no = no > max_serial_no ? no : max_serial_no;
						
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// Contact server B
					// send request to server B
					if (sendto(udp_sockfd, send_buffer, strlen(send_buffer), 0, servinfo_B->ai_addr, servinfo_B->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					fprintf(stdout, "The main server sent a request to server B.\n");
					// receive logs to server B
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					fprintf(stdout, "The main server received the feedback from Server B using UDP over port %s.\n", UDP_PORT_B);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						char *nl_ptr = strrchr(recv_buffer, '\n');
						*nl_ptr = '\0';
						char *log_serial_no = nl_ptr + 1;
						int no;
						sscanf(log_serial_no, "%d", &no);
						max_serial_no = no > max_serial_no ? no : max_serial_no;
						
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// Contact server C
					// send request to server C
					if (sendto(udp_sockfd, send_buffer, strlen(send_buffer), 0, servinfo_C->ai_addr, servinfo_C->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					fprintf(stdout, "The main server sent a request to server C.\n");
					// receive logs to server C
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					fprintf(stdout, "The main server received the feedback from Server C using UDP over port %s.\n", UDP_PORT_C);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						char *nl_ptr = strrchr(recv_buffer, '\n');
						*nl_ptr = '\0';
						char *log_serial_no = nl_ptr + 1;
						int no;
						sscanf(log_serial_no, "%d", &no);
						max_serial_no = no > max_serial_no ? no : max_serial_no;
						
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}
					// printf("max no: %d\n", max_serial_no);
					// printf("data:\n%s", data);

					/// send data to client
					// 0-success; 1-insufficent balance; 2-sender absent; 3-receiver absent; 4-both absent
					char client_id = (0 == strcmp(tcp_connection_port, TCP_PORT_A)) ? 'A' : 'B';
					if (0 == strlen(data))
					{
						send_buffer[0] = '4';
						send_buffer[1] = '\0';
						if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
						{
							perror("ServerM send(): FAILED\n");
							exit(-1);
						}
						fprintf(stdout, "The main server sent the result of the transaction to client %c.\n", client_id);
					}
					else
					{
						// printf("data:\n%s\n", data);
						int sender_balance = 1000;
						bool sender_flag = false;	// whether sender occurs
						bool receiver_flag = false;	// whether receiver occurs
						char *cur_transaction = data;
						char *newline_ptr = strchr(data, '\n');
						while (newline_ptr != NULL)
						{
							*newline_ptr = '\0';
							char *log_name1 = strchr(cur_transaction, ' ') + 1;
							char *space_ptr = strchr(log_name1, ' ');
							*space_ptr = '\0';
							char *log_name2 = space_ptr + 1;
							space_ptr = strchr(log_name2, ' ');
							*space_ptr = '\0';
							char *log_amount = space_ptr + 1;
							int amount;
							sscanf(log_amount, "%d", &amount);

							if (0 == strcmp(sender_username, log_name1))
							{
								sender_balance -= amount;
								sender_flag = true;
							}
							
							if (0 == strcmp(sender_username, log_name2))
							{
								sender_balance += amount;
								sender_flag = true;
							}

							if (0 == strcmp(receiver_username, log_name1))
								receiver_flag = true;
							

							if (0 == strcmp(receiver_username, log_name2))
								receiver_flag = true;
							
							cur_transaction = newline_ptr + 1;
							newline_ptr = strchr(cur_transaction, '\n');
						}
						// printf("balance: %d\n", balance);

						/// decide transaction state
						int tx_amount;
						sscanf(transfer_amount, "%d", &tx_amount);
						if (!sender_flag)
						{
							send_buffer[0] = '2';
							send_buffer[1] = '\0';
							if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
							{
								perror("ServerM send(): FAILED\n");
								exit(-1);
							}
							fprintf(stdout, "The main server sent the result of the transaction to client %c.\n", client_id);
						}
						else if (!receiver_flag)
						{
							send_buffer[0] = '3';
							send_buffer[1] = '\0';
							if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
							{
								perror("ServerM send(): FAILED\n");
								exit(-1);
							}
							fprintf(stdout, "The main server sent the result of the transaction to client %c.\n", client_id);
						}
						else if (sender_balance < tx_amount)
						{
							send_buffer[0] = '1';
							send_buffer[1] = '\t';
							send_buffer[2] = '\0';
							char balance_c[20];	
							sprintf(balance_c,"%d", sender_balance);
							strcat(send_buffer, balance_c);
							if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
							{
								perror("ServerM send(): FAILED\n");
								exit(-1);
							}
							fprintf(stdout, "The main server sent the result of the transaction to client %c.\n", client_id);
						}
						else
						{
							// write to server
							srand((unsigned)time(NULL));
							int random_num;
							random_num = rand() % 3;
							// printf("%d\n", random_num);

							send_buffer[0] = '2';
							send_buffer[1] = '\t';
							send_buffer[2] = '\0';
							char next_serial_no[20];
							char space_c[2] = {' ', '\0'};	
							sprintf(next_serial_no,"%d", max_serial_no + 1);
							strcat(send_buffer, next_serial_no);
							strcat(send_buffer, space_c);
							strcat(send_buffer, sender_username);
							strcat(send_buffer, space_c);
							strcat(send_buffer, receiver_username);
							strcat(send_buffer, space_c);
							strcat(send_buffer, transfer_amount);
							// printf("sent data: %s\n", send_buffer);

							if (0 == random_num)
							{
								// send write request to server A
								if (sendto(udp_sockfd, send_buffer, strlen(send_buffer), 0, servinfo_A->ai_addr, servinfo_A->ai_addrlen) == -1) 
								{
									perror("ServerM sendto(): FAILED\n");
									exit(-1);
								}
								fprintf(stdout, "The main server sent a request to server A.\n");

								// receive logs to server A
								numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
								if (numbytes == -1) 
								{
									perror("ServerM recvfrom(): FAILED\n");
									exit(-1);
								}
								recv_buffer[numbytes] = '\0';
								fprintf(stdout, "The main server received the feedback from Server A using UDP over port %s.\n", UDP_PORT_A);
							}
							else if (1 == random_num)
							{
								// send write request to server B
								if (sendto(udp_sockfd, send_buffer, strlen(send_buffer), 0, servinfo_B->ai_addr, servinfo_B->ai_addrlen) == -1) 
								{
									perror("ServerM sendto(): FAILED\n");
									exit(-1);
								}
								fprintf(stdout, "The main server sent a request to server B.\n");
								// receive logs to server B
								numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
								if (numbytes == -1) 
								{
									perror("ServerM recvfrom(): FAILED\n");
									exit(-1);
								}
								recv_buffer[numbytes] = '\0';
								fprintf(stdout, "The main server received the feedback from Server B using UDP over port %s.\n", UDP_PORT_B);
							}
							else if (2 == random_num)
							{
								// send request to server C
								if (sendto(udp_sockfd, send_buffer, strlen(send_buffer), 0, servinfo_C->ai_addr, servinfo_C->ai_addrlen) == -1) 
								{
									perror("ServerM sendto(): FAILED\n");
									exit(-1);
								}
								fprintf(stdout, "The main server sent a request to server C.\n");
								// receive logs to server C
								numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
								if (numbytes == -1) 
								{
									perror("ServerM recvfrom(): FAILED\n");
									exit(-1);
								}
								recv_buffer[numbytes] = '\0';
								fprintf(stdout, "The main server received the feedback from Server C using UDP over port %s.\n", UDP_PORT_C);
							}


							// contact client
							send_buffer[0] = '0';
							send_buffer[1] = '\t';
							send_buffer[2] = '\0';
							int balance_after = sender_balance - tx_amount;
							char balance_after_c[20];	
							sprintf(balance_after_c,"%d", balance_after);
							strcat(send_buffer, balance_after_c);
							if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
							{
								perror("ServerM send(): FAILED\n");
								exit(-1);
							}
							fprintf(stdout, "The main server sent the result of the transaction to client %c.\n", client_id);
							
						}
					}
				}
				else if (TXLIST == op)
				{
					char data[MAXDATASIZE * 3];	// aggregate info
					data[0] = '\0';

					char tmp_buffer[MAXDATASIZE];
					strcpy(tmp_buffer, recv_buffer);
					fprintf(stdout, "The main server received a sorted list request from the client using TCP over port %s.\n", tcp_connection_port);
					/// Contact server A
					// send request to server A
					if (sendto(udp_sockfd, tmp_buffer, strlen(tmp_buffer), 0, servinfo_A->ai_addr, servinfo_A->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					// fprintf(stdout, "The main server sent a request to server A.\n");
					// receive logs to server A
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					// printf("recv_buffer: %s\n", recv_buffer);
					// fprintf(stdout, "The main server received transactions from Server A using UDP over port %s.\n", UDP_PORT_A);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// Contact server B
					// send request to server B
					if (sendto(udp_sockfd, tmp_buffer, strlen(tmp_buffer), 0, servinfo_B->ai_addr, servinfo_B->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					// fprintf(stdout, "The main server sent a request to server B.\n");
					// receive logs to server B
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					// fprintf(stdout, "The main server received transactions from Server B using UDP over port %s.\n", UDP_PORT_B);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// Contact server C
					// send request to server C
					if (sendto(udp_sockfd, tmp_buffer, strlen(tmp_buffer), 0, servinfo_C->ai_addr, servinfo_C->ai_addrlen) == -1) 
					{
						perror("ServerM sendto(): FAILED\n");
						exit(-1);
					}
					// fprintf(stdout, "The main server sent a request to server C.\n");
					// receive logs to server C
					numbytes = recvfrom(udp_sockfd, recv_buffer, MAXDATASIZE-1 , 0, (struct sockaddr *)&their_addr, &addr_len);
					if (numbytes == -1) 
					{
						perror("ServerM recvfrom(): FAILED\n");
						exit(-1);
					}
					recv_buffer[numbytes] = '\0';
					// fprintf(stdout, "The main server received transactions from Server C using UDP over port %s.\n", UDP_PORT_C);
					// append logs to data
					if (strlen(recv_buffer) != 0)
					{
						strcat(data, recv_buffer);
						int size = strlen(data);
						data[size] = '\n';
						data[size + 1] = '\0'; 
					}

					/// preprocess data for sorting
					// printf("data:\n%s\n", data);
					char *newline_ptr = strchr(data, '\n');
					int line_cnt = 0;
					while (newline_ptr != NULL)
					{
						line_cnt += 1;
						newline_ptr = strchr(++newline_ptr, '\n');
					}
					// printf("line count: %d\n", line_cnt);
					
					if (0 == line_cnt)
					{
						perror("No data in the database.\n");
						exit(-1);
					}
					char *logs[line_cnt];
					newline_ptr = data;
					for (int cnt = 0; cnt < line_cnt; cnt++)
					{
						logs[cnt] = newline_ptr;
						newline_ptr = strchr(logs[cnt], '\n');
						*newline_ptr = '\0';
						newline_ptr++;
					}
					
					FILE *w_fp = fopen(SORTED_LOGS_FILE_PATH, "w");
					/// sort the data
					quick_sort_logs(logs, 0, line_cnt-1);

					/// save it to "alichain.txt"
					for (int i = 0; i < line_cnt; i++)
						fprintf(w_fp, "%s\n", logs[i]);
					fclose(w_fp);

					/// contact client
					char client_id = (0 == strcmp(tcp_connection_port, TCP_PORT_A)) ? 'A' : 'B';
					send_buffer[0] = '1';
					send_buffer[1] = '\0';
					if (send(child_fd, send_buffer, strlen(send_buffer), 0) == -1)
					{
						perror("ServerM send(): FAILED\n");
						exit(-1);
					}
					fprintf(stdout, "The main server finished the sorted list request from the client %c.\n", client_id);
				}
				else
				{
					perror("ServerM: Invalid Service\n");
					exit(-1);
				}
				close(child_fd);
				
			}
		}
		
	}
	freeaddrinfo(servinfo_A);	// all done with this structure
	freeaddrinfo(servinfo_B);
	freeaddrinfo(servinfo_C);
	close(udp_sockfd);
	close(tcp_sockfd_A);
	close(tcp_sockfd_B);
	return 0;
}
