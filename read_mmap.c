#include "stdio.h"
#include "stdlib.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "read_mmap.h"
#include "score_thread.h"

const struct mmap_item* get_items(const char* mfile, int64_t* num_items) {
  struct mmap_header* header = (struct mmap_header*)mfile;
  *num_items = header->item_count;
  return (const struct mmap_item*)(mfile + header->data_offset);
}

const struct mmap_feature* get_features(const char* mfile,
                                        const struct mmap_item* item) {
  if (item->features_offset == 0) {
    return NULL;
  }
  return (const struct mmap_feature*)(mfile + item->features_offset);
}

const struct mmap_feature* get_top_level_features(const char* mfile,
                                                  int64_t* num_features) {
  const struct mmap_header* header = (const struct mmap_header*)mfile;
  *num_features = header->item_count;
  return (const struct mmap_feature*)(mfile + header->data_offset);
}

const char *open_mmap_read(const char *file_name, int *mmapfd) {
  *mmapfd = open(file_name, O_RDONLY);
  if (*mmapfd < 0) {
    fprintf(stderr, "Could not open mmap file %s\n",
            file_name);
    exit(1);
  }
  struct stat statbuf;
  if (fstat(*mmapfd, &statbuf) == -1) {
    fprintf(stderr, "Could not stat file %s\n", file_name);
    exit(1);
  }
  lseek(*mmapfd, 0, SEEK_SET);
  char *mmap_addr = mmap(NULL, statbuf.st_size,
                         PROT_READ, MAP_PRIVATE, *mmapfd, 0);
  if (mmap_addr == MAP_FAILED) {
    fprintf(stderr, "Could not memory map file %s\n",
            file_name);
    exit(1);
  }
  return mmap_addr;
}
