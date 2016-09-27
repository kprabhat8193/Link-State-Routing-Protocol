#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <float.h>
#include <time.h> 
#include <sys/time.h>

#define BUFFER	5120
#define POLY 0x8408
#define ALPHA	0.5
#define INFINITY 999
#define MAX 10
#define PKTSIZE 64
#define HDR_SIZE sizeof(struct header)

unsigned long receiver,seq;
int server_sock;
int n,k,ack_check[100];
int *links;
double rtt_cost;
char filepath[256];
//double **adj_mat;

pthread_mutex_t mutexbuf; // mutex to lock the receive buffer.
pthread_mutex_t send_mutexbuf; // mutex for send buffer.
pthread_mutex_t mutexadj;

// structures
struct data {
	unsigned long my_id;
	char ** ips; //  http://stackoverflow.com/questions/8467469/how-to-assign-value-to-the-double-pointer-in-struct-member
	int *ports; 
	int *ftr_sendports;

}router;

struct header
{
	unsigned long router_id;
	unsigned long dest_id;
	unsigned long pkt_type;
	unsigned long checksum;
	unsigned long len; // length of the info in the packet (not the header. only the info in the packet).
	unsigned long seq;
	
}*hdr ;

#define RTT  0
#define FILE  4

struct rtt
{
	unsigned short type;
	unsigned long long time;
	double link_cost;
};
// rtt1 corresponds to rtt from the router with a larger id and rtt2 corresponds to rtt from the smaller id.
#define RTT4	3
#define RTT5	4
#define RTT6	5

//structure for file transfer
struct filetrans
{
//	unsigned long type;
//	double offset;
	unsigned long check;
}*recvdata;


//  diep function for errors
void diep(char *s)
{
	perror(s);	
	exit(1);
}

// checksum
unsigned long crc16(char *data_p, unsigned long length)
{
      unsigned char i;
      unsigned long data;
      unsigned long crc = 0xffff;

      if (length == 0)
            return (~crc);

      do
      {
            for (i=0, data=(unsigned int)0xff & *data_p++;
                 i < 8; 
                 i++, data >>= 1)
            {
                  if ((crc & 0x0001) ^ (data & 0x0001))
                        crc = (crc >> 1) ^ POLY;
                  else  crc >>= 1;
            }
      } while (--length);

      crc = ~crc;
      data = crc;
      crc = (crc << 8) | (data >> 8 & 0xff);

      return (crc);
}


int server_receive(int sock, void *buf, unsigned long len, struct sockaddr *si_other) //  returns a pointer to buffer. 
{
	pthread_mutex_lock (&mutexbuf);// lock receive buffer using mutex
//	puts("buffer locked by mutex");
			
	struct sockaddr_in si_me;
	int slen = sizeof(struct sockaddr_in);
	    
    int rets = recvfrom(sock, buf, len, 0, si_other, &slen);
    
   pthread_mutex_unlock (&mutexbuf); // unlock receive buffer using mutex
	//	puts("buffer unlocked by mutex");
		
    return rets;
}

unsigned long long current_timestamp()   //http://stackoverflow.com/questions/3756323/getting-the-current-time-in-milliseconds
{
	struct timeval te; 
	gettimeofday(&te, NULL);
	unsigned long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
	return milliseconds;
} 

// create packet(join header and data)
void create_pkt(void *buf, unsigned long router_id, unsigned long pkt_type, unsigned long len, void *info, unsigned long seq, unsigned long dest_id)
{
	struct header hdr;
	hdr.router_id = router_id;
	hdr.pkt_type = pkt_type;
	hdr.checksum = crc16((char *) info, len);
	hdr.len = len;
	hdr.seq=seq;
	hdr.dest_id=dest_id;
	
	memcpy(buf, &hdr, HDR_SIZE);
	memcpy(buf + HDR_SIZE, info, len);
}

int client_send(char *ip, int port, void *send_buf, unsigned long len) 
{
	pthread_mutex_lock (&send_mutexbuf);// lock buffer using mutex
	//puts("send buffer locked by mutex");
	
	struct sockaddr_in si_other;
	int s, slen = sizeof(si_other); // changed sockaddr_in to si_other 
	
	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	diep("socket");
	
	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
    si_other.sin_port = htons(port);
    if(inet_aton(ip, &si_other.sin_addr) == 0) // stores the ip passed as argument to &si_other.sin_addr
    diep("ip address invalid");
	
	int ret = sendto(s, send_buf, len, 0, (struct sockaddr *)&si_other, slen); 
	//puts("message sent");
	close(s);
	//free(send_buf);
	
	pthread_mutex_unlock (&send_mutexbuf);
	//puts("send buffer unlocked by mutex");
	
	return ret;	
}

// function to change the adjescency matrix.
/*void changeadj(unsigned long a, unsigned long b, int c, unsigned long long diff, double e)
{
	pthread_mutex_lock (&mutexadj);
	if(c == 0)
	{
	adj_mat[a][b] = 0;		
	}
	else if(c==1)
	{
		adj_mat[a][b] = 1;
	}
	else if(c==3)
	{
	adj_mat[a][b] = ALPHA * adj_mat[a][b] + (1 - ALPHA) * diff;
	}
	else if (c==4)
	{
	adj_mat[a][b]= e;
	}
	pthread_mutex_unlock (&mutexadj);
	
}*/

//thread functions

void *filetransfer(void *ptr)
{
	int fd,rd;
	char readbuf[PKTSIZE];
	char sendbuf[1024];
	fd= open(filepath,O_RDONLY);
	struct header hdr;
	void *rdbuf = malloc(PKTSIZE);
	if(fd==-1)
	puts("\nerror reading file");
	else 
	puts("File opened");
//	rd=read(fd,rdbuf,PKTSIZE);
//	printf("\nData read is:%s",rdbuf);
	usleep(12000000);
	unsigned long destid,r;
	int seq =0;
	int j;
	for(j=0;j<n;j++)
	{
		if(links[j]==1)
		{
			r=j;
			break;
		}
	}
//	struct filetrans check;
//	check.check=1993;
//	printf("\nSending data: %lu",check.check);
	seq=0;
	void *buf = malloc(HDR_SIZE + PKTSIZE);
	while(1)
	{
//		puts("Before reading");
		rd=read(fd,rdbuf,PKTSIZE);
//		puts("After reading");
		if(rd==0)
		{
			printf("\nReached the end of the file!\n\nFILE TRANSFER COMPLETE!!!\n");
			break;
		}
		printf("\nSending packet with sequence number %lu!\nData read is:%s",seq,rdbuf);
		destid=6;
		usleep(4000000);
		//void *buf = malloc(HDR_SIZE + PKTSIZE);
		create_pkt(buf,router.my_id,FILE,PKTSIZE,rdbuf,seq, destid);
		client_send(router.ips[r],router.ports[r],buf,HDR_SIZE+PKTSIZE);
		seq=seq+1;
		CHECK:
			//usleep(500000);
			if(ack_check[seq]==0)
			goto CHECK;
//			else
//			{
//				printf("\nSequence number incremented from %lu to %lu\n",seq,seq+1);
//				seq=seq+1;
//			}
		bzero(rdbuf,HDR_SIZE + PKTSIZE);
		//free(buf);
	}

	return NULL;
}

// rtt thread
void *rtt(void *ptr)
{
	//usleep(3000000);
	int i;
	struct rtt msg;
	while(1)
	{
		//if(terminate) break;
		usleep(2000000);
		
		void *buf = malloc(HDR_SIZE + sizeof(struct rtt));
		for(i = 0; i < n; i++)
		{
			if(i == router.my_id) continue;
			if(links[i] == 0) continue;
			//printf("!!!!!!!!!!!!!!!rtt links%f\n",adj_mat[router.my_id][i]);
			if(router.my_id < i) continue;
			
			//puts("rtt sent to router");
			bzero(buf, HDR_SIZE + sizeof(struct rtt));
			msg.type = RTT4;
			msg.time = current_timestamp();
			create_pkt(buf, router.my_id, RTT, sizeof(struct rtt), (void *)&msg,0,0);
		//	usleep(1564 * (rand() % (i + 2)));
			client_send(router.ips[i], router.ports[i], buf, HDR_SIZE + sizeof(struct rtt));
		}
		free(buf);
	}
	return NULL;
}

// server listen

void *server_listen(void *ptr)
{
 	char buf[BUFFER];
 
	while(1)
	{
	
		(memset((buf), '\0', (BUFFER)), (void) 0);
		//puts("memset of buffer in serverlisten done");
		
		
		if(server_receive(server_sock, buf, BUFFER, NULL) > 0)// returns a pointer to the buffer.
		{
			//puts("something in buffer\n");
			struct header *hdr = (struct header *)buf; 
			// check if checksum is correct.
			if(hdr->checksum != crc16((void *)(buf + HDR_SIZE), hdr->len)) // get checksum users argument as a buffer pointer and length of the buffer. not sure if void *
			{
			puts("corrupted packet");
			continue;
			// send nack.
			}
			//printf("message from router id %lu\n", hdr->router_id );
	
			switch(hdr->pkt_type)
			{
			
				case RTT:
//				if(((struct rtt *) (buf + HDR_SIZE))->type == RTT4)
//				{
//					struct rtt reply;
//					reply.type = RTT5;
//					reply.time = ((struct rtt *) (buf + HDR_SIZE))->time;
//					void *send_buf = malloc(HDR_SIZE + sizeof(struct rtt));
//					create_pkt(send_buf, router.my_id, RTT, sizeof(struct rtt), (void *)&reply,0,0);
//					client_send(router.ips[hdr->router_id], router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct rtt));
//				//	puts("RTT2 sent");
//					free(send_buf);
//				}
					 if(((struct rtt *) (buf + HDR_SIZE))->type == RTT5)
					 {
					 	rtt_cost=1;
						unsigned long long diff = (current_timestamp() - ((struct rtt *) (buf + HDR_SIZE))->time);
						rtt_cost = ALPHA * rtt_cost + (1 - ALPHA) * diff;
						//changeadj(router.my_id,hdr->router_id,4,0,rtt_cost);
						//changeadj(hdr->router_id,router.my_id,4,0,rtt_cost);
					//	printf("\nlink value connecting  %d: %f",hdr->router_id,adj_mat[router.my_id][hdr->router_id] );
						// send rtt3
						struct rtt reply;
						reply.type = RTT6;
						reply.link_cost= rtt_cost;
					//	printf("\nlink cost being sent in rtt3:%f",adj_mat[router.my_id][hdr->router_id]);
						//reply.time = ((struct rtt *) (buf + HDR_SIZE))->time;
						void *send_buf = malloc(HDR_SIZE + sizeof(struct rtt));
						create_pkt(send_buf, router.my_id, RTT, sizeof(struct rtt), (void *)&reply,0,0);
						client_send(router.ips[hdr->router_id], router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct rtt));
					//	puts("RTT2 sent");
						free(send_buf);
					} 
					else
					{
						
						rtt_cost = ((struct rtt *) (buf + HDR_SIZE))->link_cost;
						//changeadj(router.my_id,hdr->router_id,4,0,rtt_cost);
						//changeadj(hdr->router_id,router.my_id,4,0,rtt_cost);
					//	printf("\nnow my new cost from rtt3:%f",adj_mat[router.my_id][hdr->router_id]);
					}
					
					break;
				case FILE:
					puts("Received an ACK");
					if(strcmp(((void *)(buf+HDR_SIZE)),"ACK")==0)
					{
						printf("\nACK with sequence number %lu received!\n",hdr->seq);
						ack_check[hdr->seq]=1;
//						puts("After ack_check updated");
					}
					
			}
		}
		
		
		//puts("done");
	}
	
	
	
	
}

//functions

int socket_bind(int port)     
{
	struct sockaddr_in si_me;
	int set, s, slen = sizeof(struct sockaddr_in);
	
	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	diep("socket");
	
	puts("socket created");
	bzero((char *) &si_me, slen);
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(s, (struct sockaddr *)&si_me, slen) == -1)
	diep("bind");
	
	printf("bound to %d...\n", port);
	
	//	The SO_RCVTIMEO value is used for socket API calls that are waiting for data to arrive.
		//	SO_RCVTIMEO value for a socket is 5 seconds. If a read API call is issued for the socket and no data arrives in 5 seconds,
		// 	control will be passed back to the application with a return code indicating that the operation timed out.
			struct timeval tv;
			tv.tv_sec = 5;		// 5s
			tv.tv_usec = 0; 
		    set =setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
		    if(set == -1)
			perror("setsock");
	return s;
}


int main(int argc, char **argv)
{
	int i;
	n = 7; // in case I want to change to change the connections later. 
	router.ips= (char **)malloc(sizeof(char *) * n);
	router.ports = malloc(sizeof(int) * n);
	router.ftr_sendports = malloc(sizeof(int) * n);
	links = malloc(sizeof(int) * n);
	//adj_mat = malloc(sizeof(double *) * n);
	bzero(ack_check,sizeof(ack_check));
	for( i = 0; i < n; i++)
	{
		router.ips[i] = (char *)malloc(256);
		//adj_mat[i] = malloc(sizeof(double) * n);
		bzero(router.ips[i], sizeof(char) * 256);
	}
	//Get IP address -------- Ref. Dr. T.Znati
	struct addrinfo hints, *res;
	int errcode;
	char addrstr[100];
	void *ptr;
	
	char hostname[1024];
	
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	
	errcode = getaddrinfo (hostname, NULL, &hints, &res);
	if (errcode != 0)
	{
	  perror ("getaddrinfo");
	  return -1;
	}
	
	printf ("Host: %s\n", hostname);
	while (res)
	{
	  inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);
	
	  switch (res->ai_family)
	    {
	    case AF_INET:
	      ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
	      break;
	    case AF_INET6:
	      ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
	      break;
	    }
	  inet_ntop (res->ai_family, ptr, addrstr, 100);
	  printf ("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
	          addrstr, res->ai_canonname);
	  res = res->ai_next;
	}
	
	router.ips[0]=("136.142.227.15");
	router.ips[1]=("136.142.227.5");
	router.ips[2]=("136.142.227.6");
	router.ips[3]=("136.142.227.13");
	router.ips[4]=("136.142.227.9");
	router.ips[5] =("136.142.227.7");// client - arsenic
	router.ips[6] =("136.142.227.11");	
	//puts("3");
	router.ports[0]=47000;
	router.ports[1]=48000;
	router.ports[2]=49000;
	router.ports[3]=50000;
	router.ports[4]=51000;
	router.ports[5]=52000; // 5 is client
	router.ports[6]=53000; // 6 is server.
	//puts("4");
	router.ftr_sendports[0]=6741;
	router.ftr_sendports[1]=6742;
	router.ftr_sendports[2]=6743;
	router.ftr_sendports[3]=6744;
	router.ftr_sendports[4]=6745;
	router.ftr_sendports[5]=6746;
	router.ftr_sendports[6]=6747;
	//puts("5");
	links[0]=1;
	links[1]=0;
	links[2]=0;
	links[3]=0;
	links[4]=0;
	links[5]=0;
	links[6]=0;
	//puts("6");

	
	if(argc != 2)
	{
		printf("Provide a client id. Usage <filename.exe> <client id>");
	//	exit(-1);
	}
	
	router.my_id = atol(argv[1]);
	printf("starting client at %s:%d...\n",router.ips[router.my_id], router.ports[router.my_id]);
	
	// bind to socket for rtt callculation 
	server_sock = socket_bind(router.ports[router.my_id]);
	
	pthread_mutex_init(&mutexbuf, NULL);  // mutex initialization for receive buffer.
 	pthread_mutex_init(&send_mutexbuf, NULL); // mutex initilization for send buffer.// mutex initilization for send buffer.
 
	printf("\nPlease choose a file path\n>>");
	scanf("%s",filepath);
	printf("filepath: %s", filepath);
	
	
	pthread_t t_server_listen;
	pthread_create(&t_server_listen, NULL, server_listen, NULL);
	
	pthread_t t_rtt;
	pthread_create(&t_rtt, NULL, rtt, NULL);
	
	pthread_t t_filetransfer;
	pthread_create(&t_filetransfer, NULL, filetransfer, NULL);
	
	pthread_join(t_server_listen, NULL);
	pthread_join(t_rtt, NULL);
	pthread_join(t_filetransfer, NULL);

	
//	printf("Enter the receiver id for file transfer\n");
//	scanf("%lu", &receiver);
//	printf("receiver id %lu",receiver);
	// selecting and opening a file
	
}


