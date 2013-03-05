/* Defines the format of data storage memory maps, and provides
   accessor functions for reading them. Memory maps are composed of
   "items" (pages, users), which are feature vectors, or simply a list
   of features (page controversy scores).*/

#ifndef __read_mmap_h__
#define __read_mmap_h__

#include <stdint.h>

struct mmap_header {
  int64_t data_offset;
  int64_t item_count;
};

struct mmap_item {
  int64_t id;
  double sum_or_norm;
  int64_t count_features;
  int64_t features_offset;
  // With count_features mmap_feature structs at features_offset
};

struct mmap_feature {
  int64_t feature_number;
  double feature_value;
};

const struct mmap_item* get_items(const char* mfile, int64_t* num_items);
const struct mmap_feature* get_features(const char* mfile,
                                        const struct mmap_item* item);
const struct mmap_feature* get_top_level_features(const char* mfile,
                                                  int64_t* num_features);
const char *open_mmap_read(const char *file_name, int *mmapfd);
#endif
