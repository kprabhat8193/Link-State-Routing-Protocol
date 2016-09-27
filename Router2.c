
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

#define BUFFER	51200
#define POLY 0x8408
#define ALPHA	0.8
#define INFINITY 999
#define MAX 10
#define PKTSIZE 64
// initilizations
int server_sock, ftr_sock,fail_check[5];
int n,neighbor_count,n_count,resolve_check;
int *links;
double **adj_mat;
double **G;
double pred[MAX];
unsigned long *prev_alive_seq;
unsigned long *prev_lsa_seq;
unsigned long long *prev_lsa_ts;
unsigned long *alive_check;
unsigned long receiver,next_hop;
int *blacklist;
int j,k;
char filepath[256],filedata[256];
int hosts[10];


pthread_mutex_t mutexbuf; // mutex to lock the receive buffer.
pthread_mutex_t send_mutexbuf; // mutex for send buffer.
pthread_mutex_t mutexadj; // mutex for changing values of adjescency matrix;


//STRUCTURES
struct sockaddr_in *my_router,server;

struct data {
	unsigned long my_id;
	char ** ips; //  http://stackoverflow.com/questions/8467469/how-to-assign-value-to-the-double-pointer-in-struct-member
	int *ports; 
	int *ftr_sendports; 
	int *ftr_receiveports;	
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
#define ALIVE  1
#define LSA  2
#define NACQ  3
#define FILE  4
#define REGISTER 5
#define RESOLVE 6
#define RESOLV 7

#define HDR_SIZE sizeof(struct header)

// structure rtt 
struct rtt
{
	unsigned short type;
	unsigned long long time;
	double link_cost;
};
// rtt1 corresponds to rtt from the router with a larger id and rtt2 corresponds to rtt from the smaller id.
#define RTT1	0
#define RTT2	1
#define RTT3	2
#define RTT4	3
#define RTT5	4
#define RTT6	5

//structure alive 
struct alive
{
	unsigned long type; 
	unsigned long seq;
};

#define ALIVE_MSG	0
#define ALIVE_REPLY	1

// structure link state advertisement
struct lsa
{
	unsigned long lsa_id;
	unsigned long lsa_type;
	unsigned long long age;
	unsigned long seq;
	unsigned long ttl;
	double lsa_cost[10];
};

#define TRIGGERED	1 // what to do if triggered?
#define PERODIC	0

// structure neighbour acquisition.
struct nacq
{
	unsigned long type;
};

#define BE_NEIGHBOR_REQUEST	0
#define BE_NEIGHBOR_ACCEPT	1
#define BE_NEIGHBOR_REFUSE	2
#define CEASE_NEIGHBOR		3

//structure for file transfer
struct filetrans
{
//	unsigned long type;
//	double offset;
	unsigned long check;
}recvdata;

struct nr 
{
   int router_id;
   struct sockaddr_in r_data;
}neighbors[4];

// FUNCTIONS
void printnr( struct nr *router )
{
    printf( "Router Information of Router %d:\n", router->router_id);
    printf("Router ID: %d\n",router->router_id);
   printf( "The IP address: %s\n", inet_ntoa(router->r_data.sin_addr));
   printf( "The Port number: %u\n", htons(router->r_data.sin_port));
}

void dijkstra(double **G,int n, unsigned long startnode) // G is adj_mat, n is number of nodes and startnode.
{
	//	puts("g matrix\n");
		
	//		for(j=0;j<n;j++)
	//			{
	//				for(k=0;k<n;k++)
	//				printf("%f\n",G[j][k] );
	//			}
	puts("Entered dij thread");   
	double cost[MAX][MAX],distance[MAX];
	   
	double visited[MAX],count,mindistance;
	int i,j,nextnode;
	   
	//pred[] stores the predecessor of each node
	   
	//count gives the number of nodes seen so far
	   
	//create the cost matrix
	   
	for(i=0;i<n;i++)
	       
		for(j=0;j<n;j++)
	           
			if(G[i][j]==0)
		       cost[i][j]=INFINITY;
		    else
	          cost[i][j]=G[i][j];
	                
	                
	//	puts("cost matrix");
	//		for(j=0;j<n;j++)
	//							{
	//								for(k=0;k<n;k++)
	//								printf("%f\n",cost[j][k] );
	//							}
	                
	   
	//initialize pred[],distance[] and visited[]
	puts("After dij first for loop");
	   
	for(i=0;i<n;i++)
	{
	       
		distance[i]=cost[startnode][i];
		pred[i]=startnode;
		visited[i]=0;
	}
	puts("After dij second for loop");   
	distance[startnode]=0;
	visited[startnode]=1;
	count=1;
	puts("Just before while loop in dij");   
	while(count< (n-1))
	{
        mindistance=INFINITY;
	    puts("Entered while loop of dij");   
		//nextnode gives the node at minimum distance
		       
		for(i=0;i<n;i++)
		{
			if( (distance[i]<mindistance) && (!visited[i]))
			           
			{
			    mindistance=distance[i];
			    nextnode=i;
			           
			}
	    }           
			
       puts("After 1st for loop");
		//check if a better path exists through nextnode
		       
		visited[nextnode]=1;
		puts("Just before second for loop");       
		for(i=0;i<n;i++)           
		{
			puts("Entered 2nd for loop");
			if(!visited[i]) 
			{
				puts("Entered if statement");
				printf("\nmindistance: %f", mindistance );
				printf("\n cost of next node : %f",cost[nextnode][i]);
				printf("\ndistance:%f",distance[i]);
				puts("Before 2nd if");
				if((mindistance+cost[nextnode][i]) < distance[i])
                {
                    puts("Entered 2nd if");
					distance[i]=mindistance+cost[nextnode][i];
                    pred[i]=nextnode;
                }
			}	
				
		}	
		       
		count++;
    }
	   
	//print the path and distance of each node
	 puts("Out of while loop in dij");  
	for(i=0;i<n;i++)
	           
		if(i!=startnode)
		           
		{
		    printf("\nDistance of node %d = %f",i,distance[i]);
		    printf("\nPath = %d",i);
		    j=i;
		    do
		        {
		        	j=pred[j];
		                   
					printf(" <-%d",j);
		        }while(j!=startnode);
		           
		}
}

// function to change the adjescency matrix.
void changeadj(unsigned long a, unsigned long b, int c, unsigned long long diff, double e)
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


unsigned long long current_timestamp()   //http://stackoverflow.com/questions/3756323/getting-the-current-time-in-milliseconds
{
	struct timeval te; 
	gettimeofday(&te, NULL);
	unsigned long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
	return milliseconds;
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
// THREAD FUNCTIONS

//void *filetransfer(void *ptr)
//{
//	int fd;
//	unsigned long seq = 0;
//	char readbuf[PKTSIZE];
//
//	usleep(5000000);
//	printf("Please choose a file.\n>> ");
//	scanf("%s",filepath);
//	printf("filepath: %s", filepath);
//	fd = open(filepath, O_RDONLY);
// 	if(fd==-1)
//	puts("\nerror reading file");
//	else 
//	puts("file opened");
//	
//	void *buf = malloc(HDR_SIZE + PKTSIZE);
//	struct header hdr;	
//		while(1)
//	{
//		usleep(500000);
//		int len = read(fd, readbuf, PKTSIZE);
//		//printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!the size of packet is %lu", len);
//		seq++;
//		
//		create_pkt(buf, router.my_id, FILE, len, (void *) readbuf,seq,receiver);
//		int i;
//		for(i=0;i<n;i++)
//		{
//			printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!the next node to reach  node %d is %f",i, pred[i]);
//		}
//	//	client_send()
//		
//		
//	}	
//		
//		
//	//printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!sender and receiver are");
//
//return NULL;
//}

void *dij(void *ptr)
{
	int i,j;
	
	while(1)
	{
		
		usleep(5000000);
		puts("\nCost matrix:\n\t0\t \t1\t \t2\t \t3\t \t4\t \t5\t \t6\n");
		for(i=0;i<n;i++)
		{
			printf("%d",i);
			for(j=0;j<n;j++)
			{
				printf("\t%f",adj_mat[i][j]);
			}
			printf("\n");
		}
		double cost[MAX][MAX],distance[MAX];
	   
		double visited[MAX],count,mindistance;
		int i,j,nextnode;
		unsigned long startnode;
		startnode=router.my_id;
		   
		//pred[] stores the predecessor of each node
		   
		//count gives the number of nodes seen so far
		   
		//create the cost matrix
		   
		for(i=0;i<n;i++)
		       
			for(j=0;j<n;j++)
		           
				if(adj_mat[i][j]==0)
			       cost[i][j]=INFINITY;
			    else
		          cost[i][j]=adj_mat[i][j];
		                
		                
		//	puts("cost matrix");
		//		for(j=0;j<n;j++)
		//							{
		//								for(k=0;k<n;k++)
		//								printf("%f\n",cost[j][k] );
		//							}
		                
		   
		//initialize pred[],distance[] and visited[]
		
		   
		for(i=0;i<n;i++)
		{
		       
			distance[i]=cost[startnode][i];
			pred[i]=startnode;
			visited[i]=0;
		}
		   
		distance[startnode]=0;
		visited[startnode]=1;
		count=1;
		   
		while(count< (n-1))
		{
	        mindistance=INFINITY;
		       
			//nextnode gives the node at minimum distance
			       
			for(i=0;i<n;i++)
			           
				if( (distance[i]<mindistance) && (!visited[i]))
				           
				{
				    mindistance=distance[i];
				    nextnode=i;
				           
				}
			       
			//check if a better path exists through nextnode
			       
			visited[nextnode]=1;
			       
			for(i=0;i<n;i++)           
				if(!visited[i]) 
					if((mindistance+cost[nextnode][i]) < distance[i])
	                {
	                    distance[i]=mindistance+cost[nextnode][i];
	                    pred[i]=nextnode;
	                }
			       
			count++;
	    }
		   
		//print the path and distance of each node
		   
		for(i=0;i<n;i++)
		           
			if(i!=startnode)
			           
			{
			    printf("\nDistance of node %d = %f",i,distance[i]);
			    printf("\nPath = %d",i);
			    j=i;
			    do
			        {
			        	j=pred[j];
			                   
						printf(" <- %d",j);
			        }while(j!=startnode);
			           
			}
//		dijkstra(adj_mat, n, router.my_id);
	}
	
	return NULL;
}

void *adperodic(void *ptr)
{
	//puts("aperodic link started");
	int i, j;
	unsigned long k,l;
	unsigned long long seq = 0;
	struct lsa msg;
	while(1) 
	{
		//if(terminate) break;
		usleep(5000000);
		seq = seq+ 1;
		void *buf = malloc(HDR_SIZE + sizeof(struct lsa));
		for(i = 0; i < n; i++)
		{
			if(i == router.my_id) continue;
			if(hosts[i]==1) continue;
			if(blacklist[i]==1) continue;
			if(links[i] == 0) continue;
			//	printf("aperodic link %f",adj_mat[router.my_id][i] );
			
			if(adj_mat[router.my_id][i] == 0) continue;
			//printf("aperodic link %f",adj_mat[router.my_id][i] );
			
			bzero(buf, HDR_SIZE + sizeof(struct lsa));
			msg.lsa_id = router.my_id;
			msg.seq = seq;
			msg.lsa_type = PERODIC; 
			msg.ttl = 5;			
			msg.age = current_timestamp();
		//	memcpy(msg.lsa_cost, adj_mat[router.my_id], n * sizeof(double));
		//	for(k=0;k<n;k++)
		//	printf("the message that is beiing advertised is %f",msg.lsa_cost[i]);
//			for(k=0;k<n;k++)
//			{
//				for(l=0;l<n;l++)
//				{
//					printf("\nThe adjesency matrix is: %f", adj_mat[k][l]) ;
//				}
//			}
//			printf("\n my id is: %lu", router.my_id);
			for(k=0;k<n;k++)
			{
				msg.lsa_cost[k]= adj_mat[router.my_id][k];
			//	printf("\nmessage that is advertised to router %d is : %f",i, adj_mat[router.my_id][k]);
			//	printf("\nlsa.link_cost is : %f", msg.lsa_cost[k]);
				
			}
			
			//printf("message that is advertised %f", adj_mat[router.my_id]);
			create_pkt(buf, router.my_id, LSA, sizeof(struct lsa), (void *)&msg,0,0);
			client_send(inet_ntoa(neighbors[i].r_data.sin_addr), router.ports[i], buf, HDR_SIZE + sizeof(struct lsa));
//			puts("lsa advertised");
			
			
		}
		free(buf);
		
	
	}
	return NULL;
}



// rtt thread
void *rtt(void *ptr)
{
	usleep(5000000);
	int i;
	struct rtt msg;
	while(1)
	{
		//if(terminate) break;
		usleep(15000000);
		
		void *buf = malloc(HDR_SIZE + sizeof(struct rtt));
		for(i = 0; i < n; i++)
		{
			if(i == router.my_id) continue;
			if(links[i] == 0) continue;
			//printf("!!!!!!!!!!!!!!!rtt links%f\n",adj_mat[router.my_id][i]);
			if(router.my_id < i) continue;
			if(adj_mat[router.my_id][i] == 0) continue;
			
			bzero(buf, HDR_SIZE + sizeof(struct rtt));
			msg.type = RTT1;
			msg.time = current_timestamp();
			create_pkt(buf, router.my_id, RTT, sizeof(struct rtt), (void *)&msg,0,0);
		//	usleep(1564 * (rand() % (i + 2)));
			client_send(inet_ntoa(neighbors[i].r_data.sin_addr), router.ports[i], buf, HDR_SIZE + sizeof(struct rtt));
		}
		free(buf);
	}
	return NULL;
}

// neighbouur acquisition thread
void *nacq(void *ptr)
{
	//printf("before nacq %f: ", adj_mat[1][2]);
	int i;
	struct nacq msg;
	while(1)
	{
	
		
		void *buf = malloc(HDR_SIZE + sizeof(struct nacq));
		for(i = 0; i < n; i++)
		{
			//printf("blacklist of %d = %d\n", i, blacklist[i]);
			if(blacklist[i]==1) continue;
			//printf("blacklist of %d = %d\n", i, blacklist[i]);
			if(i == router.my_id) continue;
			if(links[i] == 0) 
			continue; 
			//printf("link value 2: %f\n",adj_mat[router.my_id][i] );
			if(hosts[i]==1) continue;
			if(adj_mat[router.my_id][i] > 0) 
			continue;
			bzero(buf, HDR_SIZE + sizeof(struct nacq));
			msg.type = BE_NEIGHBOR_REQUEST;
			create_pkt(buf, router.my_id, NACQ, sizeof(struct nacq), (void *)&msg,0,0);
			client_send(inet_ntoa(neighbors[i].r_data.sin_addr), router.ports[i], buf, HDR_SIZE + sizeof(struct nacq));
			//printf("nacq sent to routerid %d\n", i);
			//adj_mat[router.my_id][i] =1; // change
			
		}
		free(buf);
		
			usleep(10000000);
		
	}
	return NULL;
}

// alive
void *alive(void *ptr)
{
	
		//printf("before alive %f: ", adj_mat[1][2]);
	struct alive msg;
	unsigned long seq = 0;
	usleep(10000000);
	
	int i;
	while(1)
	{
		
	
		void *buf = malloc(HDR_SIZE + sizeof(struct alive));
		
		for(i = 0; i < n; i++)
		
		{
			seq=i;
			if(i == router.my_id) continue;
			if(hosts[i]==1) continue;
			if(links[i] == 0) continue;
			if(blacklist[i]==1) continue;
			//printf("!!!!!!!!!!!!!!!!!!!!!!!!alive link %f\n",adj_mat[router.my_id][i]);
			if(adj_mat[router.my_id][i]==0) continue;
			(memset((buf), '\0', (HDR_SIZE + sizeof(struct alive))), (void) 0);
			 msg.seq= seq;
			 msg.type = ALIVE_MSG;
			 alive_check[i]=0;
			 fail_check[i]=0;
			create_pkt(buf, router.my_id, ALIVE, sizeof(struct alive), (void *)&msg,0,0);
			client_send(inet_ntoa(neighbors[i].r_data.sin_addr), router.ports[i], buf, HDR_SIZE+sizeof(struct alive));
			//puts("alive message sent\n");
			prev_alive_seq[i] == seq;
		}
		free(buf);
		
			usleep(5000000);
			for(i=0;i<n;i++)
			{
				if(hosts[i]==1) continue;
				if(i == router.my_id) continue;
				if(links[i] == 0) continue;
				if(blacklist[i]==1) continue;
				if(alive_check[i]==0)
				{
//					fail_check[i]=fail_check[i]+1;
//					if(fail_check[i]==3)
//					{
						changeadj(router.my_id,i,0,0,0);
						printf("\nLink to router id %d FAILED!!! \n",i);
//					}
				}
			}
			
			
		
		// if I do not receive a reply of message type RTT2 with the 
		//same sequence number within the retransmission timeout, I disable the link. 
//		for(i = 0; i < n; i++)
//		{
//			if(prev_alive_seq[i] == seq) continue;
//						
//			
//			else{
//				printf("link to router id %d:%s failed...\n",i, router.ips[i]);
//				adj_mat[router.my_id][i] = changeadj(router.my_id,i,0,0,0);
//				}
//		}
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
				case ALIVE: 
				if(((struct alive *) (buf + HDR_SIZE))->type == ALIVE_MSG)
				{
					struct alive reply;
					reply.type = ALIVE_REPLY;
					reply.seq = ((struct alive *) (buf + HDR_SIZE))->seq;
					void *send_buf = malloc(HDR_SIZE + sizeof(struct alive)); 
					create_pkt(send_buf, router.my_id, ALIVE, sizeof(struct alive), (void *)&reply,0,0);
					client_send(inet_ntoa(neighbors[hdr->router_id].r_data.sin_addr), router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct alive));
				//	puts("Alive reply sent\n");
					free(send_buf);
				}
				else if(((struct alive *) (buf + HDR_SIZE))->type == ALIVE_REPLY)
				{
					if(((struct alive *) (buf + HDR_SIZE))->seq== hdr->router_id)
					{
					//	printf("\nRouter %lu is alive",hdr->router_id);
						alive_check[hdr->router_id]=1;
//						fail_check[hdr->router_id]=0;
					//	puts("101");
					}
					
				}
				break;
				
				case NACQ:
				if(((struct nacq *) (buf + HDR_SIZE))->type == BE_NEIGHBOR_REQUEST)
				{
					if(blacklist[hdr->router_id]==0)
					{
					
					struct nacq reply;
					reply.type = BE_NEIGHBOR_ACCEPT;
					void *send_buf = malloc(HDR_SIZE + sizeof(struct nacq));
					create_pkt(send_buf, router.my_id, NACQ, sizeof(struct nacq), (void *)&reply,0,0);
					client_send(inet_ntoa(neighbors[hdr->router_id].r_data.sin_addr), router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct nacq));
					free(send_buf);
					alive_check[hdr->router_id]=1;
					changeadj(router.my_id,hdr->router_id,1,0,0);
					//printf("link value 1: %f\n",adj_mat[router.my_id][hdr->router_id] );
					//puts("be neighbour request accepted\n");
					//printf("connected to router %d:%s...\n", hdr->router_id, router.ips[hdr->router_id]);
					
					}
					else
					{
					
					struct nacq reply;
					reply.type = BE_NEIGHBOR_REFUSE;
					void *send_buf = malloc(HDR_SIZE + sizeof(struct nacq));
					create_pkt(send_buf, router.my_id, NACQ, sizeof(struct nacq), (void *)&reply,0,0);
					client_send(inet_ntoa(neighbors[hdr->router_id].r_data.sin_addr), router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct nacq));
					free(send_buf);
					
					
				//	printf("link value 1: %f\n",adj_mat[router.my_id][hdr->router_id] );
					printf("be neighbour request refused. router blacklisted: %d\n", hdr->router_id);
						
					}
				}
					
				else if(((struct nacq *) (buf + HDR_SIZE))->type == BE_NEIGHBOR_ACCEPT)
				{
					changeadj(router.my_id,hdr->router_id,1,0,0);
					//int a =1;
				//	printf("\nlink value after NACQ ACCEPT: %f", adj_mat[router.my_id][hdr->router_id]);
				//	puts("neighbour accept received. now neighbors");
					printf("\nRouter %d:%s accepted Be Neighbor request!\n", hdr->router_id, router.ips[hdr->router_id]);
				}
				
				else if(((struct nacq *) (buf + HDR_SIZE))->type == BE_NEIGHBOR_REFUSE)
				{
					changeadj(router.my_id,hdr->router_id,0,0,0);
				}
				break;
				
				case RTT:
				if(((struct rtt *) (buf + HDR_SIZE))->type == RTT1)
				{
					struct rtt reply;
					reply.type = RTT2;
					reply.time = ((struct rtt *) (buf + HDR_SIZE))->time;
					void *send_buf = malloc(HDR_SIZE + sizeof(struct rtt));
					create_pkt(send_buf, router.my_id, RTT, sizeof(struct rtt), (void *)&reply,0,0);
					client_send(inet_ntoa(neighbors[hdr->router_id].r_data.sin_addr), router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct rtt));
				//	puts("RTT2 sent");
					free(send_buf);
				}
				 else if(((struct rtt *) (buf + HDR_SIZE))->type == RTT2)
				 {
					unsigned long long diff = (current_timestamp() - ((struct rtt *) (buf + HDR_SIZE))->time);
					
					changeadj(router.my_id,hdr->router_id,3,diff,0);
					changeadj(hdr->router_id,router.my_id,3,diff,0);
				//	printf("\nlink value connecting  %d: %f",hdr->router_id,adj_mat[router.my_id][hdr->router_id] );
					
					// send rtt3
					struct rtt reply;
					reply.type = RTT3;
					reply.link_cost= adj_mat[router.my_id][hdr->router_id];
				//	printf("\nlink cost being sent in rtt3:%f",adj_mat[router.my_id][hdr->router_id]);
					//reply.time = ((struct rtt *) (buf + HDR_SIZE))->time;
					void *send_buf = malloc(HDR_SIZE + sizeof(struct rtt));
					create_pkt(send_buf, router.my_id, RTT, sizeof(struct rtt), (void *)&reply,0,0);
					client_send(inet_ntoa(neighbors[hdr->router_id].r_data.sin_addr), router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct rtt));
				//	puts("RTT2 sent");
					free(send_buf);
				} 
				else if(((struct rtt *) (buf + HDR_SIZE))->type == RTT3)
				{
					
				changeadj(router.my_id,hdr->router_id,4,0,((struct rtt *) (buf + HDR_SIZE))->link_cost);
				changeadj(hdr->router_id,router.my_id,4,0,((struct rtt *) (buf + HDR_SIZE))->link_cost);
			//	printf("\nnow my new cost from rtt3:%f",adj_mat[router.my_id][hdr->router_id]);
				}
				else if(((struct rtt *) (buf + HDR_SIZE))->type == RTT4)
				{
					hosts[hdr->router_id]=1;
					changeadj(router.my_id,hdr->router_id,1,0,0);
					changeadj(hdr->router_id,router.my_id,1,0,0);
					struct rtt reply;
					reply.type = RTT5;
					reply.time = ((struct rtt *) (buf + HDR_SIZE))->time;
					void *send_buf = malloc(HDR_SIZE + sizeof(struct rtt));
					create_pkt(send_buf, router.my_id, RTT, sizeof(struct rtt), (void *)&reply,0,0);
					client_send(inet_ntoa(neighbors[hdr->router_id].r_data.sin_addr), router.ports[hdr->router_id], send_buf, HDR_SIZE + sizeof(struct rtt));
				//	puts("RTT2 sent");
					free(send_buf);	
				}
				else if(((struct rtt *) (buf + HDR_SIZE))->type == RTT6)
				{
					
				changeadj(router.my_id,hdr->router_id,4,0,((struct rtt *) (buf + HDR_SIZE))->link_cost);
				changeadj(hdr->router_id,router.my_id,4,0,((struct rtt *) (buf + HDR_SIZE))->link_cost);
			//	printf("\nnow my new cost from rtt3:%f",adj_mat[router.my_id][hdr->router_id]);
				}
				
				
				break;
				
				case LSA:
					
					if(((struct lsa *)(buf + HDR_SIZE))->lsa_type == PERODIC)
					{
						int i;
						int j;
						int p;
						if(((struct lsa *)(buf + HDR_SIZE))->lsa_id == router.my_id) 
						{
							break;
						}
						if(prev_lsa_seq[((struct lsa *)(buf + HDR_SIZE))->lsa_id] >= ((struct lsa *)(buf + HDR_SIZE))->seq)
						{
							if(prev_lsa_ts[((struct lsa *)(buf + HDR_SIZE))->lsa_id] > ((struct lsa *)(buf + HDR_SIZE))->age)
							{
								break;
							}
						}
							
						prev_lsa_seq[((struct lsa *)(buf + HDR_SIZE))->lsa_id] = ((struct lsa *)(buf + HDR_SIZE))->seq;
						prev_lsa_ts[((struct lsa *)(buf + HDR_SIZE))->lsa_id] = current_timestamp();
						
						
//						pthread_mutex_lock (&mutexadj);
//						memcpy(adj_mat[((struct lsa *)(buf + HDR_SIZE))->lsa_id], ((struct lsa *)(buf + HDR_SIZE))->lsa_cost, n * sizeof(double));
//						pthread_mutex_unlock (&mutexadj);
						pthread_mutex_lock (&mutexadj);
						for(p=0;p<n;p++)
						{
							adj_mat[((struct lsa *)(buf + HDR_SIZE))->lsa_id][p]= ((struct lsa *)(buf + HDR_SIZE))->lsa_cost[p];
							adj_mat[p][((struct lsa *)(buf + HDR_SIZE))->lsa_id]= ((struct lsa *)(buf + HDR_SIZE))->lsa_cost[p];
						//	printf("\nLSA received : %f", adj_mat[((struct lsa *)(buf + HDR_SIZE))->lsa_id][p]);
						}
						pthread_mutex_unlock (&mutexadj);
						//((struct lsa *)(buf + HDR_SIZE))->ttl--;
						if(((struct lsa *)(buf + HDR_SIZE))->ttl == 0)
						{
							break;
						}
						
						void *send_buf = malloc(HDR_SIZE + sizeof(struct lsa));
						create_pkt(send_buf, router.my_id, LSA, sizeof(struct lsa), (void *)((struct lsa *)(buf + HDR_SIZE)),0,0);
						struct lsa readd;
						for(i = 0; i < n; i++)
						{
							if(i==hdr->router_id) continue;
							if(i==router.my_id) continue;
							if(links[i] == 0) continue;
							if(adj_mat[router.my_id][i] == 0) continue;
							
							readd.lsa_id = ((struct lsa *)(buf + HDR_SIZE))->lsa_id;
							readd.seq = ((struct lsa *)(buf + HDR_SIZE))-> seq;
							readd.age = ((struct lsa *)(buf + HDR_SIZE))->age;
							readd.lsa_type = ((struct lsa *)(buf + HDR_SIZE))->lsa_type;
							readd.ttl = ((struct lsa *)(buf + HDR_SIZE))->ttl--;
							for(k=0;k<n;k++)
							{
								readd.lsa_cost[k] = ((struct lsa *)(buf + HDR_SIZE))->lsa_cost[k];
							}
							
							create_pkt(send_buf, router.my_id, LSA, sizeof(struct lsa), (void *)&readd,0,0);
							client_send(inet_ntoa(neighbors[i].r_data.sin_addr), router.ports[i], send_buf, HDR_SIZE + sizeof(struct lsa));
						//	puts("lsa readvertised");
//							for(j=0;j<n;j++)
//							{
//								for(k=0;k<n;k++)
//								printf("%f\n",adj_mat[j][k] );
//							}
							
						}
						free(send_buf);
					}
					break;
				case FILE:
					puts("\n\n\nReceived file for transfer\n\n\n");
//					struct filetrans *recvdata;
//					recvdata.check=((struct filetrans *)(buf+HDR_SIZE))->check;
//					strcpy(filedata,((void *)(buf+HDR_SIZE)));
					printf("Data to be sent: %s\n",((void *)(buf+HDR_SIZE)));
					if(hdr->dest_id==router.my_id)
					{
						puts("File transfer reached destination\n");
					}
					else
					{
						int i;
						k=hdr->dest_id;
						j=pred[hdr->dest_id];
						printf("\nPred of dest is %d\n",j);
						for(i=0;i<n;i++)
						{
//							printf("Iteration: %d\n",i);
							if(router.my_id==j)
							{
								next_hop=k;
								break;
							}
							else
							k=j;
							j=pred[j];
													
						}
						printf("\n Next hop is %lu\n",next_hop);
						if(router.my_id==pred[hdr->dest_id])
						{
							void *send_buf1 = malloc(HDR_SIZE + PKTSIZE);
							create_pkt(send_buf1, hdr->router_id, FILE, PKTSIZE, ((void *)(buf+HDR_SIZE)),hdr->seq,hdr->dest_id);
							client_send(router.ips[next_hop], router.ports[next_hop], send_buf1, HDR_SIZE + PKTSIZE);
							free(send_buf1);
						}
						else
						{
							void *send_buf = malloc(HDR_SIZE + PKTSIZE);
							create_pkt(send_buf, hdr->router_id, FILE, PKTSIZE, ((void *)(buf+HDR_SIZE)),hdr->seq,hdr->dest_id);
							client_send(inet_ntoa(neighbors[next_hop].r_data.sin_addr), router.ports[next_hop], send_buf, HDR_SIZE + PKTSIZE);
							free(send_buf);
						}
					}
					break;
				case REGISTER:
					if(strcmp(((void *)(buf+HDR_SIZE)),"Registered")==0)
					{
						puts("\nRegisteration Successful confirmation received from NR!\n");
					}
					break;
				case RESOLVE:
					//printf("Received a resolve message\n");
					neighbors[((struct nr *) (buf + HDR_SIZE))->router_id].router_id=((struct nr *) (buf + HDR_SIZE))->router_id;
					neighbors[((struct nr *) (buf + HDR_SIZE))->router_id].r_data.sin_addr=((struct nr *) (buf + HDR_SIZE))->r_data.sin_addr;
					neighbors[((struct nr *) (buf + HDR_SIZE))->router_id].r_data.sin_port=((struct nr *) (buf + HDR_SIZE))->r_data.sin_port;
					//printnr(&neighbors[((struct nr *) (buf + HDR_SIZE))->router_id]);
					n_count=n_count+1;
					break;
				case RESOLV:
					if(strcmp(((void *)(buf+HDR_SIZE)),"Resolve UP")==0)
					{
						puts("Got the Resolve UP message from the NR");
						resolve_check=1;
					}
			}
		}
		
		
		//puts("done");
	}
	
	
	
	
}

//  creating a socket and binding
int socket_bind(int port)     // bind to a particular address? instead of any
{
	struct sockaddr_in si_me;
	int set, s, slen = sizeof(struct sockaddr_in);
	
	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	diep("socket");
	
	puts("Socket created");
	bzero((char *) &si_me, slen);
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(s, (struct sockaddr *)&si_me, slen) == -1)
	diep("bind");
	
	printf("Bound to %d \n", port);
	
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




// MAIN
int main(int argc, char **argv)
{
	int i;
	n = 7;
	router.ips = (char **)malloc(sizeof(char *) * n);
	router.ports = malloc(sizeof(int) * n);
	router.ftr_sendports = malloc(sizeof(int) * n);
	router.ftr_receiveports = malloc(sizeof(int) * n);
	adj_mat = malloc(sizeof(double *) * n);
	alive_check=malloc(sizeof(unsigned long)*n);
	G=  malloc(sizeof(double *) * n);
	links = malloc(sizeof(int) * n);
	blacklist= malloc(sizeof(int) * n);
	prev_alive_seq =  malloc(sizeof(unsigned long)*n);
	prev_lsa_seq = malloc(sizeof(unsigned long) * n);;
	prev_lsa_ts = malloc(sizeof(unsigned long long) * n);
	my_router=malloc(sizeof(struct sockaddr_in));
	
	bzero(prev_alive_seq, sizeof(unsigned long ) * n);
	bzero(prev_lsa_seq, sizeof(unsigned long ) * n);
	bzero(prev_lsa_ts, sizeof(unsigned long long ) * n);
	bzero(blacklist, sizeof(int ) * n);
	bzero(alive_check, sizeof(unsigned long ) * n);
	bzero(my_router,sizeof(struct sockaddr_in));
	
	//puts("1");
	
	for( i = 0; i < n; i++)
	{
		router.ips[i] = (char *)malloc(1024);
		adj_mat[i] = malloc(sizeof(double) * n);
		G[i] = malloc(sizeof(double) * n);
		bzero(adj_mat[i], sizeof(double) * n);
		bzero(G[i], sizeof(double) * n);
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
	
//	printf ("Host: %s\n", hostname);
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
//	  printf ("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
//	          addrstr, res->ai_canonname);
	  res = res->ai_next;
	}
	
//	router.ips[0]=("136.142.227.15");
//	router.ips[1]=("136.142.227.5");
//	router.ips[2]=("136.142.227.6");
//	router.ips[3]=("136.142.227.13");
//	router.ips[4]=("136.142.227.9");
	router.ips[5] =("136.142.227.7");// client - arsenic
	router.ips[6] =("136.142.227.11");	
	//puts("3");
	router.ports[0]=0;
	router.ports[1]=0;
	router.ports[2]=49000;
	router.ports[3]=0;
	router.ports[4]=0;
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
	links[0]=0;
	links[1]=1;
	links[2]=0;
	links[3]=1;
	links[4]=0;
	links[5]=0;
	links[6]=0;
	//puts("6");
	blacklist[0]=0;
	blacklist[1]=0;
	blacklist[2]=0;
	blacklist[3]=0;
	blacklist[4]=0;
	blacklist[5]=0;
	blacklist[6]=0;
	//puts("7");
	
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr("136.142.227.10");
	server.sin_port = htons( 55000 );
	
	
	if(argc != 2)
	{
		printf("Provide a router id. Usage <filename.exe> <routerid>");
	//	exit(-1);
	}
	//puts("8");
	
	router.my_id = atol(argv[1]);  // The C library function long int atol(const char *str) converts the string argument str to a long integer (type long int).
	//puts("9");
//	router.ips[router.my_id]=addrstr;
	printf("Starting router %lu at %s:%d\n", router.my_id, router.ips[router.my_id], router.ports[router.my_id]);
	//puts("10");

	server_sock = socket_bind(router.ports[router.my_id]);
	ftr_sock = socket_bind(router.ftr_sendports[router.my_id]);
	
	//puts("8");
	
	pthread_mutex_init(&mutexbuf, NULL);  // mutex initialization for receive buffer.
 	pthread_mutex_init(&send_mutexbuf, NULL); // mutex initilization for send buffer.// mutex initilization for send buffer.
 	pthread_mutex_init(&mutexadj, NULL);
	
	pthread_t t_server_listen;
	pthread_create(&t_server_listen, NULL, server_listen, NULL);
	
	//finding number of neighbours
	neighbor_count=0;
	n_count=0;
	resolve_check=0;
	for(j=0;j<n;j++)
	{
//		if(j==5) continue;
		if(j == router.my_id) continue;
		if(links[j] == 0) continue;
		if(blacklist[j]==1) continue;
		if(j>4) continue;
		neighbor_count=neighbor_count+1;
	}

	//puts("9");
	my_router->sin_family=AF_INET;
	my_router->sin_addr.s_addr=inet_addr(addrstr);
	my_router->sin_port=htons(router.ports[router.my_id]);
	
	//registering to the NR server
//	puts("Registering to the Name Resolver!\n");
	void *send_buf = malloc(HDR_SIZE + sizeof(struct sockaddr_in));
	create_pkt(send_buf, router.my_id, REGISTER, sizeof(struct sockaddr_in), (void *)my_router,0,0);
	client_send(inet_ntoa(server.sin_addr), (int) server.sin_port, send_buf, HDR_SIZE + sizeof(struct sockaddr_in));
	free(send_buf);
	
	
	CHECK:
		if(resolve_check==0)
		goto CHECK;
	
	usleep(5000000);
	while(1)
	{
		if(n_count==neighbor_count)
		{
			puts("\nInformation of all neighbors obtained:\n");
			for(i=0;i<n;i++)
			{	
				if(i == router.my_id) continue;
				if(links[i] == 0) continue;
				if(blacklist[i]==1) continue;
				if(i>4) continue;
				printnr(&neighbors[i]);
			}
			break;
		}		
		for(i=0;i<n;i++)
		{
			usleep(500000);
//			if(i==5) continue;
			if(i == router.my_id) continue;
			if(links[i] == 0) continue;
			if(blacklist[i]==1) continue;
			if(i>4) continue;
			neighbors[i].router_id=i;
			void *send_buf = malloc(HDR_SIZE + sizeof(struct nr));
			create_pkt(send_buf, router.my_id, RESOLVE, sizeof(struct nr), (void *)&neighbors[i],0,0);
			client_send(inet_ntoa(server.sin_addr), (int) server.sin_port, send_buf, HDR_SIZE + sizeof(struct nr));
			free(send_buf);
		}
	}
	
	for(i=0;i<5;i++)
	{
		if(i == router.my_id) continue;
		if(links[i] == 0) continue;
		if(blacklist[i]==1) continue;
//		router.ips[i]=inet_ntoa(neighbors[i].r_data.sin_addr);
		router.ports[i]=(int)htons(neighbors[i].r_data.sin_port);
	}
	
	pthread_t t_nacq;
	pthread_create(&t_nacq, NULL, nacq, NULL);
	
	pthread_t t_alive;
	pthread_create(&t_alive, NULL, alive, NULL);
	
	
	pthread_t t_rtt;
	pthread_create(&t_rtt, NULL, rtt, NULL);
	
	pthread_t t_adperodic;
	pthread_create(&t_adperodic, NULL, adperodic, NULL);
	
	pthread_t t_dij;
	pthread_create(&t_dij, NULL, dij, NULL);
	




	
	
	
	
//	printf("Enter the receiver id for file transfer\n");
//	scanf("%lu", &receiver);
//	printf("receiver id %lu",receiver);
	
//	pthread_t t_filetransfer;
//	pthread_create(&t_filetransfer, NULL, filetransfer, NULL);
	
	pthread_join(t_server_listen, NULL);
	pthread_join(t_nacq, NULL);
	pthread_join(t_rtt, NULL);
	pthread_join(t_adperodic, NULL);
	pthread_join(t_alive, NULL);
	pthread_join(t_dij, NULL);
//	pthread_join(t_filetransfer, NULL);

	
	

	
	
	
	return 0;
}
