#include "packet_queue.h"
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

/*
#include <pthread.h>
#include <stdbool.h>
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
int packet_append(struct packet_queue * Q, void * item);

//Takes the item at the front of the queue and removes item
//returns the item at the front of the line
//NOTE: THE ITEM NEEDS TO BE FREE() AT SOME POINT
void packet_remove(struct packet_queue * Q, char* key);

//deletes the item at top of the stack
void packet_pop(struct packet_queue * Q,);

//creates a new packet_queue struct
struct packet_queue* packet_queue_init();

//returns the value associated with that key. If DNE, returns null
char* packet_find(struct packet_queue* Q, char* key);

//destroys the packet_queue struct
//queue must be empty in order to do this
int packet_queue_destroy(struct packet_queue* Q);
*/


char* packet_find(struct packet_queue* Q, char* key) {
	struct packet_queue_node* ptr = Q->head;
	while(ptr != NULL) {
		if(strcmp(ptr->key, key) == 0) {
			return ptr->value;
		}
		ptr = ptr->next;
	}
	return NULL;

}

int packet_remove(struct packet_queue* Q, char* key) {
  	pthread_mutex_lock(&Q->lock);
	if(strcmp(Q->head->key, key) == 0) {
		//if its the first node then just call pop
		//this has its own lock
  		pthread_mutex_unlock(&Q->lock);
		packet_pop(Q);
		return 1;
	}
	struct packet_queue_node* ptr = Q->head;
	struct packet_queue_node* prev = NULL;
	//this will run at least once
	while(ptr != NULL) {
		if(strcmp(ptr->key, key) == 0) {
			break;
		}
		prev = ptr;
		ptr = ptr->next;
	}
	if(ptr == NULL) {
		return 1;
	}
	prev->next = ptr->next;
	free(ptr->key);
	free(ptr->value);
	free(ptr);
	Q->number_of_nodes--;
	
  	pthread_mutex_unlock(&Q->lock);
	return 0;
}

//appends an item to the end of the Queue.
//Queue does take ownership of the item,
int packet_append(struct packet_queue * Q, char * key, char * value) {
  //lock critical area
  pthread_mutex_lock(&Q->lock);

  struct packet_queue_node* node = (struct packet_queue_node*) malloc(sizeof(struct packet_queue_node));
  node->key = key;
  node->value = value;
  node->next = NULL;
  Q->number_of_nodes++;
  //if this was the first node we added
  if(Q->number_of_nodes == 1){
    Q->head = node;
    Q->tail = node;
  } else {
    Q->tail->next = node;
    Q->tail = node;
  }

  pthread_mutex_unlock(&Q->lock);

  //no errors
  return 0;
}

void packet_pop(struct packet_queue * Q) {

  pthread_mutex_lock(&Q->lock);
  assert(Q->number_of_nodes != 0);

  struct packet_queue_node* node = Q->head;
  Q->number_of_nodes--;
  //if that was the last node
  if(Q->number_of_nodes == 0){
    Q->head = NULL;
    Q->tail = NULL;
  } else {
    Q->head = Q->head->next;
  }
  free(node->key);
  free(node->value);
  free(node);
  pthread_mutex_unlock(&Q->lock);
}
//creates a new gen_queue
struct packet_queue* packet_queue_init(){
  struct packet_queue* Q = (struct packet_queue*)malloc(sizeof(struct packet_queue));
  Q->head = NULL;
  Q->tail = NULL;
  Q->number_of_nodes = 0;
  //TODO: do something with this
  int ret = pthread_mutex_init(&Q->lock, NULL);
  if(ret != 0) {
    //something horrible has happened
    printf("SOMETHING MUTEX FAILED TO INIT");
    abort();
  }
  return Q;
}

//should be EMPTY by now
int packet_queue_destroy(struct packet_queue* Q){
  assert(Q->number_of_nodes == 0);
  //TODO: do this
  //if these fail i really dont give a shit
  pthread_mutex_destroy(&Q->lock);

  free(Q);
  return 0;
}
