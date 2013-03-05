/* Compute the similarity score for two pages. Mostly useful for
   debugging.*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "score_thread.h"
#include "read_mmap.h"

int main(int argc, char **argv) {
  if (argc != 4) {
    printf("Usage: %s page_mmap first_pageid second_pageid\n", argv[0]);
    return 1;
  }
  int page_mmapfd;
  const char *page_mmap = open_mmap_read(argv[1], &page_mmapfd);
  int64_t num_pages;
  const struct mmap_item *pages = get_items(page_mmap, &num_pages);
  int first_pageid = atoi(argv[2]);
  int second_pageid = atoi(argv[3]);
  assert(first_pageid < num_pages && first_pageid >= 0);
  assert(second_pageid < num_pages && second_pageid >= 0);
  assert(pages[first_pageid].id == first_pageid);
  assert(pages[second_pageid].id == second_pageid);
  printf("%1.4f\n", SIM_FUNC(
      page_mmap,
      pages + first_pageid,
      pages + second_pageid));
  return 0;
}
