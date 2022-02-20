
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <getopt.h>

#include<fcntl.h>
#include <unistd.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/time.h>
#include<sys/mman.h>
#include<linux/videodev2.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include <errno.h>

#define CAPTURE_FRAMES 1801
#define SIZE 614400

#define HRES_STR "640"
#define VRES_STR "480"

struct timespec time_sent;

char dumpname[] = "Sent0000.pgm";
char dumpheader[] = "P5\n\n"HRES_STR" "VRES_STR"\n255\n";

int main() {
	char hostname[64];
	char *ip = "127.0.0.1";
	int port = 8080;
	int e;

	int n, dumpfd;
	int framenum=0;
	FILE *fp;
	long int frame_size=0;
	int sockfd, new_sock;
	struct sockaddr_in server_addr, new_addr;
	socklen_t addr_size;
	struct hostent *hp;
	char received_message[] = "received";
	char buffer[350000];
	int i=0, sockarg;
	char som;
	struct linger option;


	if((sockfd = socket(AF_INET, SOCK_STREAM, 0))<0) {
		perror("Error in socket creation");
		exit(1);
	}
	printf("Server socket created successfully\n");

	option.l_onoff=1;
	option.l_linger=0;
	if(setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char *)&option, sizeof(option))) {
		perror("error in setsockopt");
		exit(1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if((e=bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)))<0) {
			perror("Error in socket bind");
			exit(1);
	}

	printf("Server socket bind successful\n");

	if(listen(sockfd, 3)==0) {
		printf("Listening...\n");
	}
	else {
		perror("Error in listening\n");
		exit(1);
	}

	addr_size = sizeof(new_addr);
	new_sock = accept(sockfd, (struct sockaddr*)&new_addr, &addr_size);
	FILE *dump_fd;

	strncat(&dumpheader[3], "\n\n"HRES_STR" "VRES_STR"\n255\n", 14);
	while(1) {
			frame_size=0;
			i=0;

			if(i==0) {
				snprintf(&dumpname[4], 9, "%04d", framenum);
				strncat(&dumpname[8], ".pgm", 5);
				dumpfd = open(dumpname, O_WRONLY | O_NONBLOCK | O_CREAT, 00666);
				write(dumpfd, dumpheader, sizeof(dumpheader)-1);
				i++;
				close(dumpfd);
				dump_fd = open(dumpname, O_WRONLY | O_NONBLOCK | O_APPEND, sizeof(buffer));
			}
	
			do {
				n = recv(new_sock, buffer, sizeof(buffer), 0);
			//	printf("N received: %d\t buffer copied: %d \t\n", n, strlen(buffer));
				write(dumpfd, buffer, strlen(buffer));
				frame_size+=n;
			} while(frame_size<307200);
	close(dumpfd);
	if(n<=0)
		perror("recv");
	if(frame_size>=307200) {
		send(new_sock, &received_message, strlen(received_message),0);
	}
	printf("Data written successfully for frame=%d\n", framenum);
		framenum++;
		bzero(buffer, strlen(buffer));
	//	memset(&buffer[0], 0, sizeof(buffer));
	}
	return 0;
}

