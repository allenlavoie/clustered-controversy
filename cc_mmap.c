/* Read in a list of users (or user groups), compute CC, clustering,
   and controversy scores in parallel, and write those scores to
   disk. */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "score_thread.h"
#include "read_mmap.h"
#include "queue.h"

#define BUFFER_SIZE 10000

int main(int argc, char **argv) {
  if (argc != 6) {
    printf("Usage: %s users_mmap pages_mmap controversy_mmap"
           " userids_file num_threads\n", argv[0]);
    return 1;
  }
  int user_mmapfd, page_mmapfd, controversy_mmapfd;
  const char *user_mmap = open_mmap_read(argv[1], &user_mmapfd);
  const char *page_mmap = open_mmap_read(argv[2], &page_mmapfd);
  const char *controversy_mmap = open_mmap_read(argv[3],
                                                &controversy_mmapfd);
  int64_t num_users;
  const struct mmap_item *users = get_items(user_mmap, &num_users);
  int64_t num_pages;
  const struct mmap_item *pages = get_items(page_mmap, &num_pages);
  int64_t num_controversy;
  const struct mmap_feature *controversy = get_top_level_features(
      controversy_mmap, &num_controversy);
  struct queue *work_queue = init_queue(100);
  int num_threads = atoi(argv[5]);
  struct thread_info *threads = (struct thread_info*)malloc(
      num_threads * sizeof(struct thread_info));
  pthread_t *pths = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
  
  for (int i = 0; i < num_threads; ++i) {
    struct thread_info *tinfo = threads + i;
    tinfo->mmap_pages = page_mmap;
    tinfo->pages = pages;
    tinfo->num_pages = num_pages;
    
    tinfo->mmap_users = user_mmap;
    tinfo->users = users;
    tinfo->num_users = num_users;
    
    tinfo->controversy = controversy;
    tinfo->num_controversy = num_controversy;
    
    tinfo->input_queue = work_queue;
    sprintf(tinfo->cc_output_file, "scores_out_%d", i);
    sprintf(tinfo->c_output_file, "raw_page_stats_out_%d", i);
    pthread_create(pths + i, NULL, generate_scores, threads + i);
  }
  FILE *input_file = fopen(argv[4], "r");
  assert(input_file);
  char line_buffer[BUFFER_SIZE];
  char *s;
  int64_t userid;
  while (fgets(line_buffer, BUFFER_SIZE, input_file) != NULL) {
    num_users = 0;
    s = line_buffer;
    char *last_end = NULL;
    // Count the number of users on this line
    for (s = strtok(s, " "); s != NULL; s = strtok(NULL, " ")) {
      if (strlen(s) == 0 || s[0] == '\n') {
        continue;
      }
      ++num_users;
      if (last_end != NULL) {
        *last_end = ' ';
      }
      last_end = s + strlen(s);
    }
    if (num_users == 0) {
      continue;
    }
    // Allocate space for the users and read them in
    struct user_group *work = malloc(sizeof(struct user_group));
    work->num_users = num_users;
    work->userids = malloc(sizeof(int64_t) * num_users);
    s = line_buffer;
    int i = 0;
    for (s = strtok(s, " "); s != NULL; s = strtok(NULL, " ")) {
      if (strlen(s) == 0 || s[0] == '\n') {
        continue;
      }
      userid = atol(s);
      work->userids[i] = userid;
      ++i;
    }
    // Send this work unit to the worker threads
    push_back(work_queue, work);
  }
  // Tell each thread that there's no more data.
  for (int i = 0; i < num_threads; ++i) {
    push_back(work_queue, NULL);
  }
  for (int i = 0; i < num_threads; ++i) {
    pthread_join(pths[i], NULL);
  }
  free(threads);
  free(pths);
  free_queue(work_queue);
  return 0;
}
