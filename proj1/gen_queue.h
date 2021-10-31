
#include <pthread.h>
#include <stdbool.h>
struct gen_queue_node {
  struct gen_queue_node * next;
  void * data;
};

struct gen_queue {
  struct gen_queue_node * head; //next node to be poped
  struct gen_queue_node * tail; //last node to be poped
  int number_of_nodes; //the number of nodes in the queue
  pthread_mutex_t lock; //the lock that is used for pop and append
  pthread_cond_t pop_ready;//there is a node that is ready to be removed
};


//adds the item to the end of the list, 
//the queue does not own the item * will still need to be allocated/deallocated
int gen_append(struct gen_queue * Q, void * item);

//Takes the item at the front of the queue and removes item
//returns the item at the front of the line
//NOTE: THE ITEM NEEDS TO BE FREE() AT SOME POINT
void* gen_pop(struct gen_queue * Q, int* countWaiting, bool* kill);

//creates a new gen_queue struct
struct gen_queue* gen_queue_init();

//destroys the gen-queue struct
//queue must be empty in order to do this
int gen_queue_destroy(struct gen_queue* Q);

//if the queue is empty, then returns true
//otherwise returns false
bool gen_is_empty(struct gen_queue* Q);