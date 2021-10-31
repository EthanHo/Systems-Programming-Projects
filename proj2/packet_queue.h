
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
struct packet_queue_node {
  struct packet_queue_node * next;
  char* key;
  char* value;
};

struct packet_queue {
  struct packet_queue_node * head; //next node to be poped
  struct packet_queue_node * tail; //last node to be poped
  int number_of_nodes; //the number of nodes in the queue
  pthread_mutex_t lock; //the lock that is used for remove and append
};


//adds the item to the end of the list, 
//the queue does not own the item * will still need to be allocated/deallocated
int packet_append(struct packet_queue * Q, char* key, char* value);

//Takes the item at the front of the queue and removes item
//returns the item at the front of the line
//NOTE: THE ITEM NEEDS TO BE FREE() AT SOME POINT
int packet_remove(struct packet_queue * Q, char* key);

//only good for quickly deleting the list
//deletes the item at top of the stack
void packet_pop(struct packet_queue * Q);
//creates a new packet_queue struct
struct packet_queue* packet_queue_init();

//returns the value associated with that key. If DNE, returns null
char* packet_find(struct packet_queue* Q, char* key);

//destroys the packet_queue struct
//queue must be empty in order to do this
int packet_queue_destroy(struct packet_queue* Q);
