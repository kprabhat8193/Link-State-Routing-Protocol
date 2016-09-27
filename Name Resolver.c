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
#define REGISTER 5
#define RESOLVE 6
#define RESOLV 7



int server_sock,send_sock;
pthread_mutex_t mutexbuf; // mutex to lock the receive buffer.
pthread_mutex_t send_mutexbuf;
int reg_count,n;
struct sockaddr_in server;
struct nr 
{
   int router_id;
   struct sockaddr_in r_data;
}r[5],resolve;

struct header
{
	unsigned long router_id;
	unsigned long dest_id;
	unsigned long pkt_type;
	unsigned long checksum;
	unsigned long len; // length of the info in the packet (not the header. only the info in the packet).
	unsigned long seq;
	
}*hdr ;

//struct data {
//	unsigned long my_id;
//	char ** ips; //  http://stackoverflow.com/questions/8467469/how-to-assign-value-to-the-double-pointer-in-struct-member
//	int *ports; 
//	int *ftr_sendports; 
//	int *ftr_receiveports;	
//}r[5],resolve;
#define HDR_SIZE sizeof(struct header)


void printnr( struct nr *router )
{
    printf( "Router Information of Router %d:\n", router->router_id);
    printf("Router ID: %d\n",router->router_id);
   printf( "The IP address: %s\n", inet_ntoa(router->r_data.sin_addr));
   printf( "The Port number: %u\n", htons(router->r_data.sin_port));
}

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

//Send a packet. Send as client. No binding required. 
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



// server receive(receive from client) function
int server_receive(int sock, void *buf, unsigned long len, struct sockaddr *si_other) //  returns a pointer to buffer. 
{
	pthread_mutex_lock (&mutexbuf);// lock receive buffer using mutex
	//puts("buffer locked by mutex");
			
	struct sockaddr_in si_me;
	int slen = sizeof(struct sockaddr_in);
	    
    int rets = recvfrom(sock, buf, len, 0, si_other, &slen);
    
   pthread_mutex_unlock (&mutexbuf); // unlock receive buffer using mutex
		//puts("buffer unlocked by mutex");
		
    return rets;
}


int socket_bind(int port)     // bind to a particular address? instead of any
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

//the thread function
void *server_listen(void *ptr)
{
 	char buf[BUFFER];
 	puts("The server is listening for incoming connections!\n");
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
			switch(hdr->pkt_type)
			{
				case REGISTER:
					printf("Register Request received!\n");
					switch(hdr->router_id)
					{
						case 0:
							r[0].router_id=hdr->router_id;
							r[0].r_data.sin_addr=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_addr;
							r[0].r_data.sin_port=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_port;
							break;
							
						case 1:
							r[1].router_id=hdr->router_id;
							r[1].r_data.sin_addr=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_addr;
							r[1].r_data.sin_port=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_port;
							break;
						case 2:
							r[2].router_id=hdr->router_id;
							r[2].r_data.sin_addr=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_addr;
							r[2].r_data.sin_port=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_port;
							break;
						case 3:
							r[3].router_id=hdr->router_id;
							r[3].r_data.sin_addr=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_addr;
							r[3].r_data.sin_port=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_port;
							break;
						case 4:
							r[4].router_id=hdr->router_id;
							r[4].r_data.sin_addr=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_addr;
							r[4].r_data.sin_port=((struct sockaddr_in *) (buf + HDR_SIZE))->sin_port;
							break;
						default:
							printf("Invalid router ID\n");
					}
					void *reg_buf = malloc(HDR_SIZE + sizeof(struct sockaddr_in));
					create_pkt(reg_buf, 10 , REGISTER, sizeof(struct sockaddr_in), (void *)"Registered",0,0);
					client_send(inet_ntoa(r[hdr->router_id].r_data.sin_addr),htons(r[hdr->router_id].r_data.sin_port), reg_buf, HDR_SIZE + sizeof(struct sockaddr_in));
					free(reg_buf);
					reg_count=reg_count+1;
					if(reg_count>5)
					reg_count=5;
					if(reg_count==5)
					{
						int i;
						puts("All routers have registered successfully!\n");
						for(i=0;i<n;i++)
						printnr(&r[i]);
						puts("\n Sending the resolve service up message to routers\n");
						for (i=0;i<n;i++)
						{
							usleep(100000);
							void *send_buf = malloc(HDR_SIZE + sizeof(struct nr));
							create_pkt(send_buf, 10 , RESOLV, sizeof(struct nr), (void *)"Resolve UP",0,0);
							client_send(inet_ntoa(r[i].r_data.sin_addr),htons(r[i].r_data.sin_port), send_buf, HDR_SIZE + sizeof(struct nr));
							free(send_buf);
						}
						
					}
					break;
			case RESOLVE:
				puts("Resolve Request received!\n");
				switch(((struct nr *) (buf + HDR_SIZE))->router_id)
				{
					case 0:
						resolve.router_id=r[0].router_id;
						resolve.r_data.sin_addr=r[0].r_data.sin_addr;
						resolve.r_data.sin_port=r[0].r_data.sin_port;
						break;
					case 1:
						resolve.router_id=r[1].router_id;
						resolve.r_data.sin_addr=r[1].r_data.sin_addr;
						resolve.r_data.sin_port=r[1].r_data.sin_port;
						break;
					case 2:
						resolve.router_id=r[2].router_id;
						resolve.r_data.sin_addr=r[2].r_data.sin_addr;
						resolve.r_data.sin_port=r[2].r_data.sin_port;
						break;
					case 3:
						resolve.router_id=r[3].router_id;
						resolve.r_data.sin_addr=r[3].r_data.sin_addr;
						resolve.r_data.sin_port=r[3].r_data.sin_port;
						break;
					case 4:
						resolve.router_id=r[4].router_id;
						resolve.r_data.sin_addr=r[4].r_data.sin_addr;
						resolve.r_data.sin_port=r[4].r_data.sin_port;
						break;
					default:
						printf("Invalid router ID\n");
				}
				void *send_buf = malloc(HDR_SIZE + sizeof(struct nr));
				create_pkt(send_buf, 10 , RESOLVE, sizeof(struct nr), (void *)&resolve,0,0);
				client_send(inet_ntoa(r[hdr->router_id].r_data.sin_addr),htons(r[hdr->router_id].r_data.sin_port), send_buf, HDR_SIZE + sizeof(struct nr));
				//printf("Sent the resolve info for router %d YO!",((struct nr *) (buf + HDR_SIZE))->router_id);
				free(send_buf);
			}
		}
	}
}

int main(int argc , char *argv[])
{

    reg_count=0;
    n=5;
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("136.142.227.10");
    server.sin_port = htons( 55000 );
	server_sock = socket_bind(server.sin_port);  
	if((send_sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	diep("socket");
	pthread_mutex_init(&mutexbuf, NULL);  // mutex initialization for receive buffer.
 	pthread_mutex_init(&send_mutexbuf, NULL); // mutex initilization for send buffer.// mutex initilization for send buffer.
	
	pthread_t t_server_listen;
	pthread_create(&t_server_listen, NULL, server_listen, NULL);
	
	pthread_join(t_server_listen, NULL);
    return 0;
}
int reg_count=0; 

