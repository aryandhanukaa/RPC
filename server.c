//creators: Aryan Dhanuka & Osterndorff
#include <pthread.h>
#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <unistd.h>
#include "udp.h"
#include "server_functions.h"
//global variables
int port;
struct socket s;
int intSz = sizeof(int); 
int charSz = sizeof(char);
int callTblIndex = 0; // index of call table to put new client info
int numClients = 0;   // number of unique clients in call table
struct client{
  int client_id;      // client ID
  int seq_number;     // last sequence number received from client
  int last_return;    // last value returned to client
};
struct threadArgs{//will be used to send the arguments as a void* 
	char type;
	struct socket * s;
	struct packet_info *packet;
	struct client *thisClient;
};
struct client *clients;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//end global variables


/**
 * Extracts the request type identifier from packet.buf.
 *
 * @param:         buf - Pointer to the packet's payload buffer.
 * @return: seq_number - The request type identifier.
 */
char get_type(char *buf)
{
  char type;
  memcpy(&type, &buf[(intSz * 2) ], charSz);
  return type;
}

/**
 * Extracts seq_numbet from packet.buf.
 *
 * @param:         buf - Pointer to the packet's payload buffer.
 * @return: seq_number - The sequence number of the request.
 */
int get_seq_number(char *buf)
{
  int seq_number;
  memcpy(&seq_number, &buf[intSz], intSz);
  return seq_number;
}

/**
 * Extracts client_id from packet.buf.
 *
 * @param:        buf - Pointer to the packet's payload buffer.
 * @return: client_id - The client ID.
 */
int get_client_id(char *buf)
{
  int client_id;
  memcpy(&client_id, &buf[0], intSz);
  return client_id;
}

/**
 * Check is the client exists in the call table, Assigning the address of its 
 * entry in the call table to thisClient if so.
 *
 * @param:  client_id - the client ID of the current request.
 * @param: thisClient - Client struct pointer to be assigned if client in table.
 * @return:       0/1 - 1 if client is new to the server, 0 otherwise.
 */
int check_client_new(int client_id, struct client *thisClient)
{
  for(int i = 0; i < numClients; ++i){
    if(clients[i].client_id == client_id){
      return 0;
    }
  }
  return 1;
}

/**
 * Returns a pointer to the client making the request.
 *
 * @param client_id - The client ID of the current request.
 * @return Pointer to the client making the request.
 */
struct client* get_client(int client_id)
{
  for(int i = 0; i < numClients; ++i){
    if(clients[i].client_id == client_id){
      return &clients[i];
    }
  }
  return NULL;
}

/**
 * Performs the current client request and sends the return value to the
 * client.
 *
 * @param:       type - The type identifier of the request.
 * @param:          s - Pointer to the socket.
 * @param:     packet - Pointer to the packet received from the client.
 * @param: thisClient - Pointer to the client's entry in the call table.
 */
void* perform_request(void *threadag)
{
	//unpackage the thread arguments
	struct threadArgs* abc = (struct threadArgs*) threadag;
	char type = abc->type;
	struct socket* s = abc->s;
	struct packet_info *packet = abc->packet;
	struct client *thisClient = abc->thisClient;
	//initialises all the thread args
  char payload[intSz*3];
  int seqNum = (get_seq_number(packet->buf));
  int clientNum = (get_client_id(packet->buf));
  memcpy(&payload, &clientNum, intSz);
  memcpy(&payload[intSz], &seqNum, intSz);
  int key;
  int ret; // return value of request
  switch(type){
    case 'a': // idle
     int time;
      memcpy(&time, &packet->buf[(intSz * 2) + charSz], intSz);
      idle(time);
      pthread_mutex_lock(&lock);
      ret = time;
      memcpy(&payload[intSz * 2], &time, intSz);
      break;
    case 'b': // get
      pthread_mutex_lock(&lock);
      memcpy(&key, &packet->buf[(intSz * 2) + charSz], intSz);
      ret = get(key);
      memcpy(&payload[intSz * 2], &ret, intSz);
      break;
    case 'c': // put
      int value;
      pthread_mutex_lock(&lock);
      memcpy(&key, &packet->buf[(intSz * 2) + charSz], intSz);
      memcpy(&value, &packet->buf[(intSz * 3) + charSz], intSz);
      ret = put(key, value);
      memcpy(&payload[intSz * 2], &ret, intSz);
      break;
  }
  clients[callTblIndex].last_return = ret;
  send_packet(*s, packet->sock,packet->slen, payload, intSz*3);
  thisClient->last_return = ret;
  pthread_mutex_unlock(&lock);
  pthread_exit(NULL);//exits thread after returning a value
}

/**
 * Sends an acknowledgement to the client upon receipt of packet.
 *
 * @param      s - Pointer to the socket
 * @param packet - Pointer to the packet received from the client.
 */
void send_ack(struct socket *s, struct packet_info *packet)
{
  char payload[] = "ack";
  send_packet(*s, packet->sock,packet->slen, payload, 3); 
}


struct threadArgs* buildThreadargs(char type,
                                   struct socket *s,
                                   struct packet_info* packet,
                                   struct client* thisClient)
{
	struct threadArgs *ab = malloc(sizeof(struct threadArgs));
	ab->type = type;
	ab->s = s;
	ab->packet = packet;
	ab->thisClient = thisClient;
	return ab;
}

/**
 * Main Function. Listens on port for incoming client requests, then checks to
 * see if the client is in the call table. The sequence number is compared
 * against any previous ones made by the client to decide whether to process
 * the request, send the last returned value, or discard the request.
 *
 * @param argc - The number of command line arguments passed to the program.
 * @param argv - The values of the command line arguments.
 */
int main(int argc, char*argv[])
{
	//command line arg:port that the server will bind itself to
	if(argc != 2){
		printf("usage: server <port>");
		return 1;
	}
	port = atoi(argv[1]);//initializes the port number
	s = init_socket(port);
  if(pthread_mutex_init(&lock, NULL) != 0){
    printf("mutex init has failed");
    return 1;
  }
  clients = malloc(sizeof(struct client) * 100);
  while(1){
    struct packet_info packet = receive_packet(s);
    // parse buf
    int client_id = get_client_id(packet.buf);
    int seq_number = get_seq_number(packet.buf);
    char type = get_type(packet.buf);
    // process request
    struct client *thisClient;
    int isNewClient = check_client_new(client_id, thisClient);
    if(isNewClient == 1){
      // add to call table, build client struct, perform call
      ++numClients;
      callTblIndex = callTblIndex % 100;
      clients[callTblIndex].client_id = client_id;
      clients[callTblIndex].seq_number = 0;
      clients[callTblIndex].last_return = -12521;
      ++callTblIndex;
    }
    thisClient = get_client(client_id);
    // check sequence number
    if(seq_number > thisClient->seq_number){ // perform call
      thisClient->seq_number = seq_number;
      struct threadArgs* abc = buildThreadargs(type, &s, &packet, thisClient);//create thread
      pthread_t th;
      if(pthread_create(&th, NULL, perform_request, (void*) abc) != 0){//failed thread creation
        continue;//the next request for the same rpc will handle the call
      }
      pthread_join(th,NULL);
    }
    else if(seq_number == thisClient->seq_number){ // return last output
      send_ack(&s, &packet);
      char payload[intSz*3];
      memcpy(&payload[0], &thisClient->client_id, intSz);
      memcpy(&payload[intSz], &thisClient->seq_number, intSz);
      memcpy(&payload[intSz * 2], &thisClient->last_return, intSz);
      send_packet(s, packet.sock, packet.slen, payload, intSz*3);
    } 
  }
}
