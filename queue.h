/* A simple thread-safe queue */

#ifndef __queue__h_
#define __queue__h_

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

struct user_group;

#define VALUE_TYPE struct user_group*

struct queue {
  int max_size;
  int first;
  int last;
  int size;
  VALUE_TYPE* values;
  pthread_mutex_t lock;
  sem_t work_sem;
  sem_t free_space_sem;
};

void push_back(struct queue* q, VALUE_TYPE value);
VALUE_TYPE pop_front(struct queue* q);
VALUE_TYPE pop_back(struct queue* q);

struct queue* init_queue(int max_size);
void free_queue(struct queue* q);

#endif
