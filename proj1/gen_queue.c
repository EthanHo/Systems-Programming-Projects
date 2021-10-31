#include "gen_queue.h"
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
//appends an item to the end of the Queue.
//Queue does not take ownership of the item, must be allocated/deallocated elsewhere
int gen_append(struct gen_queue * Q, void * item) {
  //lock critical area
  pthread_mutex_lock(&Q->lock);

  struct gen_queue_node* node = (struct gen_queue_node*) malloc(sizeof(struct gen_queue_node));
  node->data = item;
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

  pthread_cond_signal(&Q->pop_ready);
  pthread_mutex_unlock(&Q->lock);

  //no errors
  return 0;
}

void* gen_pop(struct gen_queue * Q, int* waiterCount, bool* kill) {

  pthread_mutex_lock(&Q->lock);
  //only uses numw
  while(gen_is_empty(Q)){
    (*waiterCount)++;
    pthread_cond_wait(&Q->pop_ready, &Q->lock);
    //the kill command was send, we exit
    if(*kill) {
      //printf("kill command recieved\n");
      //need to unlock before exit
      pthread_mutex_unlock(&Q->lock);
      return NULL;
    }
    (*waiterCount)--;
  }
  assert(Q->number_of_nodes != 0);

  struct gen_queue_node* node = Q->head;
  Q->number_of_nodes--;
  //if that was the last node
  if(Q->number_of_nodes == 0){
    Q->head = NULL;
    Q->tail = NULL;
  } else {
    Q->head = Q->head->next;
  }

  void * item = node->data;
  free(node);

  pthread_mutex_unlock(&Q->lock);

  return item;
}
//creates a new gen_queue
struct gen_queue* gen_queue_init(){
  struct gen_queue* Q = (struct gen_queue*)malloc(sizeof(struct gen_queue));
  Q->head = NULL;
  Q->tail = NULL;
  Q->number_of_nodes = 0;
  //TODO: do something with this
  int ret = pthread_mutex_init(&Q->lock, NULL);
  ret = ret || pthread_cond_init(&Q->pop_ready, NULL);
  if(ret != 0) {
    //something horrible has happened
    printf("SOMETHING MUTEX FAILED TO INIT");
    abort();
  }
  return Q;
}

//should be EMPTY by now
int gen_queue_destroy(struct gen_queue* Q){
  assert(Q->number_of_nodes == 0);
  //TODO: do this
  //if these fail i really dont give a shit
  pthread_mutex_destroy(&Q->lock);
  pthread_cond_destroy(&Q->pop_ready);

  free(Q);
  return 0;
}

bool gen_is_empty(struct gen_queue* Q){
  if(Q->number_of_nodes == 0) {
    return true;
  } else {
    return false;
  }
}