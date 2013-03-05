#include <assert.h>
#include <stdlib.h>

#include "queue.h"

void push_back(struct queue* q, VALUE_TYPE value) {
  sem_wait(&q->free_space_sem);
  pthread_mutex_lock(&q->lock);
  q->last = (q->last + 1) % q->max_size;
  q->values[q->last] = value;
  q->size++;
  pthread_mutex_unlock(&q->lock);
  sem_post(&q->work_sem);
  assert(q->size <= q->max_size);
}

VALUE_TYPE pop_front(struct queue* q) {
  sem_wait(&q->work_sem);
  pthread_mutex_lock(&q->lock);
  VALUE_TYPE ret = *(q->values + q->first);
  q->first = (q->first + 1) % q->max_size;
  q->size--;
  pthread_mutex_unlock(&q->lock);
  sem_post(&q->free_space_sem);
  assert(q->size >= 0);
  return ret;
}

VALUE_TYPE pop_back(struct queue* q) {
  sem_wait(&q->work_sem);
  pthread_mutex_lock(&q->lock);
  VALUE_TYPE ret = *(q->values + q->last);
  q->last = (q->last - 1);
  if (q->last < 0) {
    q->last = q->max_size - 1;
  }
  q->size--;
  pthread_mutex_unlock(&q->lock);
  sem_post(&q->free_space_sem);
  assert(q->size >= 0);
  return ret;
}

struct queue* init_queue(int max_size) {
  struct queue* q = malloc(sizeof(struct queue));
  q->max_size = max_size;
  q->size = 0;
  q->first = 0;
  q->last = max_size - 1;
  q->values = malloc(sizeof(VALUE_TYPE) * max_size);
  pthread_mutex_init(&q->lock, NULL);
  sem_init(&q->free_space_sem, 0, max_size);
  sem_init(&q->work_sem, 0, 0);
  return q;
}

void free_queue(struct queue* q) {
  free(q->values);
  pthread_mutex_destroy(&q->lock);
  sem_destroy(&q->free_space_sem);
  sem_destroy(&q->work_sem);
  free(q);
}

