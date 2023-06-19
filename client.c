//Created by: Aryan dhanuka & Osterndorff
#include <stdio.h>      // printf
#include <string.h> // memset
#include <stdlib.h> // exit(0);
#include <unistd.h> // close
#include <time.h> //timeval
#include "client.h"
#include "udp.h"

/*
 * @param rpc->the connection upon which we send and recieve messages
 * @param blen- length of the payload buffer
 * @param buf- char array with maximum length of BUFLEN
 * @param ret- pointer to object in which we store the returned packet if returned
 * function: call for an RPC retry_count times
 * if something is returned, stop trying
 * if nothing is returned=> try again
 * if an acknowledgement is returned, delay retries
 * return 0 if successful, -1 if not
 */
int send_receive_packet(struct rpc_connection* rpc,int blen,char buf[BUFLEN],struct packet_info* retVal)
{
	//int rseq= rpc->seq_number;
	int initcid;//need to check build these values from packet because rpc might be a global variable associated with multiple requests
	memcpy(&initcid,buf,sizeof(int));
	int initseq;
	memcpy(&initseq,buf+sizeof(int),sizeof(int));
	/*if(rseq>initseq)
	{
		perror("xxx");
		exit(1);
	}*/
	int retryTime = TIMEOUT_TIME;
	for(int i = 0;i <RETRY_COUNT+5; i++)
	{
		send_packet(rpc->recv_socket, rpc->dst_addr,rpc->dst_len, buf, blen);
	  struct packet_info retPacket = receive_packet_timeout(rpc->recv_socket, retryTime);
    if(strlen(retPacket.buf) == -1)
		//if(retPacket.recv_len == -1) //nothing was returned, retry
		{
			//printf("nothing returned\n");
			continue;
		}	
    else if(strlen(retPacket.buf) == 3)
		//else if(retPacket.recv_len == 3)//ack
		{	
			//printf("acknowledgement recieved\n");
			retryTime++;
		}
		else//genuine packet returned
		{
 			int retClientID;
 			int retSeqNum;
 			memcpy(&retClientID, &retPacket.buf[0], sizeof(int));
 			memcpy(&retSeqNum, &retPacket.buf[sizeof(int)], sizeof(int));
			//printf("seq number recieved : %i \n",retSeqNum);
	    if(retSeqNum==initseq)
		  {//this is infact the request that we were making
    		if(retClientID==initcid)
			  {
			    int returnval;
			    memcpy(&returnval,&retPacket.buf[sizeof(int) * 2],sizeof(int));
			    //printf("seq number:%i, client_id: %i, returnedVal: %i\n",retSeqNum, retClientID, returnval);
			    return returnval;
			  }
		  }			 
			//return  (int)(retPacket.buf);
		}
		sleep(retryTime);
	}
	perror("xxx");
	exit(1);
}
/*
 * @param src_port port num for recv_socket
 * @param dst_port port num for dst socket
 * @param dst_addr dst address as a string
 */
struct rpc_connection RPC_init(int src_port, int dst_port, char dst_addr[])
{
	srand((unsigned)time(NULL));
	 struct rpc_connection rpc;
	//might have memory scope problems
	rpc.recv_socket = init_socket(src_port);//might require changes
	struct sockaddr_storage addr;
	socklen_t addrlen;
	populate_sockaddr(AF_INET,dst_port, dst_addr,&addr,&addrlen);
	rpc.dst_addr= *((struct sockaddr*)(&addr));
	rpc.dst_len =addrlen;
	rpc.client_id=rand();
	rpc.seq_number=0;
	return rpc;
}
/*@param-buf this will store the payload
 * @param rpc this stores the rpc, to get access to the seq_number etc
 * @param name this can either be a, b or c depending on the function
 * constructs the payload 
 * returns the length of the payload
 */ 
int cons_payload(char* buf, struct rpc_connection *rpc, char name )
{
	int i = 0;
  //insert the client id
   memcpy(buf,&(rpc->client_id),sizeof(int));
  i += sizeof(rpc->client_id);
  //insert the sequence number
    rpc->seq_number++;//increment sequence number to reflect the current sequence number in being used
    memcpy(buf+i,&(rpc->seq_number),sizeof(int));
    i += sizeof(rpc->seq_number);
    //insert the name of the function (a,b,c)
   memcpy(buf+i,&name,sizeof(name));
    i += sizeof(name);
    return i;

}
// Sleeps the server thread for a few seconds
void RPC_idle(struct rpc_connection *rpc, int time)
{
	char buf[BUFLEN];
	//name:a
	int i = cons_payload(buf,rpc,'a');
	memcpy((buf+i),&time,sizeof(int));
	i+=sizeof(time);
	struct packet_info abc;
	if(send_receive_packet(rpc,i,buf,&abc)!=-1)
	{
		//deconstruct returned payload
		//payload: __time for which it has been idle, or -1 for failure
		//if nothing is returned=>function performed correctly
		//if something is returned=> function did not perform correctly
	}
	else
	{
		printf("rpc_idle timedout\n");
	}
	
}

// gets the value of a key on the server store
int RPC_get(struct rpc_connection *rpc, int key)
{
	char buf[BUFLEN];
	//name:b
	int i = cons_payload(buf, rpc, 'b');
	memcpy((buf+i),&key,sizeof(int));
	i += sizeof(key); 
	struct packet_info abc;
	int returnval;

  if((returnval=send_receive_packet(rpc,i,buf,&abc))!=-1)
  {
    //deconstruct returned payload
    //payload: integer value which represents either value or -1 for failure
	    /* int returnval;
	     printf("buffer len: %i",abc.recv_len);
	      memcpy(&returnval,&abc.buf,sizeof(int));
          return returnval;*/

	  return returnval;
  }
  else
  {
    printf("rpc_get timedout\n");
    return -1;
  }
  return 0;
}

// sets the value of a key on the server store
int RPC_put(struct rpc_connection *rpc, int key, int value)
{
	char buf[BUFLEN];
	//name:c
	int i = cons_payload(buf, rpc, 'c');
	memcpy((buf+i),&key,sizeof(int));
	i += sizeof(key);
	memcpy((buf+i),&value,sizeof(value));
	i += sizeof(value);
	struct packet_info abc;
	int returnval;
	if((returnval=send_receive_packet(rpc, i, buf, &abc))!=-1)
  {
    //deconstruct returned payload
		//returns an integer
		 /* int returnval;
		  printf("buflen: %i", abc.recv_len);
              memcpy(&returnval,&abc.buf,sizeof(int));*/
          return returnval;
  }
  else
  {
    printf("rpc_put timedout\n");
  }
  return 0;
}
//close connection with RPC
void RPC_close(struct rpc_connection *rpc)
{
    close_socket(rpc->recv_socket);
    //further statements that actually close ?
}
