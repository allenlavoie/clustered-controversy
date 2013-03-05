/* Reads user_groups from a queue, computes the CC, clustering, and
   controversy scores, and writes them out to a file. Also writes the
   controversy, edit fraction, and clustering score for each page in
   the group's local edit graph to a separate file. */

#ifndef __score_thread_h__
#define __score_thread_h__

#include <stdio.h>
#include <stdint.h>

struct queue;
struct mmap_item;
struct mmap_feature;

struct user_group {
  int num_users;
  int64_t *userids;
};

struct thread_info {
  const char *mmap_pages;
  const struct mmap_item *pages;
  int num_pages;
  const char *mmap_users;
  const struct mmap_item *users;
  int num_users;
  const struct mmap_feature *controversy;
  int num_controversy;
  struct queue *input_queue;
  char cc_output_file[100];
  char c_output_file[100];
};

/* Determines which type of similarity function to use:
   cosine similarity or Jensen-Shannon divergence */
#define SIM_FUNC cosine_similarity

double JSD(const char *pages_mfile,
           const struct mmap_item *first_page,
           const struct mmap_item *second_page);
double cosine_similarity(const char *pages_mfile,
                         const struct mmap_item *first_page,
                         const struct mmap_item *second_page);

/* Compute CC, controversy, and clustering scores for the items drawn
   from thread_info->input_queue, writing them to the files specified
   in thread_info. */
void* generate_scores(void *thread_info);

#endif
