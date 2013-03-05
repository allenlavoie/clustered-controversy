#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "read_mmap.h"
#include "score_thread.h"

#define MAX_PAGE_DID 5000000
#define MAX_NUM_FEATURES 2000000

struct item_list {
  struct item_list *next;
  int64_t count_items;
  struct mmap_feature *items;
  double sum_or_norm;
  int64_t id;
};

struct tuple_file_iterator {
  int64_t max_buffer_size;
  int64_t current_left_id;
  struct mmap_feature *items_buffer;
  double sum_values;
  int64_t count_values;
  FILE *read_from;
  int square_sum;
};

struct iterator_out {
  int filled;
  int64_t left_id;
  struct mmap_feature *items;
  int64_t count_values;
  double sum_values;
};

void reset_iterator_out(struct iterator_out *output) {
  output->items = NULL;
  output->filled = 0;
}

void init_tuple_file_iterator(struct tuple_file_iterator *workspace,
                              int64_t buffer_size,
                              const char *file_name,
                              int square_sum) {
  workspace->items_buffer = malloc(
      buffer_size * sizeof(struct mmap_feature));
  workspace->read_from = fopen(file_name, "r");
  workspace->current_left_id = -1;
  workspace->max_buffer_size = buffer_size;
  workspace->square_sum = square_sum;
}

void delete_tuple_file_iterator(struct tuple_file_iterator *workspace) {
  free(workspace->items_buffer);
}

// Return 1 while a next item is available, 0 when output is not filled.
int get_next_item(struct tuple_file_iterator *workspace,
                  struct iterator_out *output) {
  if (workspace->read_from == NULL) {
    return 0;
  }
  int64_t new_left_id;
  int64_t middle_value;
  double right_value;
  int fscanf_return = 1;
  while (1) {
    fscanf_return = fscanf(workspace->read_from,
                           "%" PRId64 " %" PRId64 " %lf",
                           &new_left_id, &middle_value, &right_value);
    assert(workspace->current_left_id <= new_left_id);
    if (workspace->current_left_id == -1) {
      workspace->current_left_id = new_left_id;
      workspace->count_values = 0;
      workspace->sum_values = 0.0;
    } else if (workspace->current_left_id != new_left_id
               || fscanf_return == EOF) {
      output->filled = 1;
      output->count_values = workspace->count_values;
      output->items = malloc(
          output->count_values * sizeof(struct mmap_feature));
      memcpy(output->items, workspace->items_buffer,
             output->count_values * sizeof(struct mmap_feature));
      output->sum_values = workspace->sum_values;
      output->left_id = workspace->current_left_id;
      workspace->current_left_id = new_left_id;
      workspace->count_values = 0;
      workspace->sum_values = 0.0;
    }
    if (fscanf_return == EOF) {
      fclose(workspace->read_from);
      workspace->read_from = NULL;
      return 1;
    }
    if (workspace->square_sum) {
      workspace->sum_values += right_value * right_value;
    } else {
      workspace->sum_values += right_value;
    }
    assert(workspace->count_values < workspace->max_buffer_size);
    workspace->items_buffer[workspace->count_values].feature_number
        = middle_value;
    workspace->items_buffer[workspace->count_values].feature_value
        = right_value;
    if (workspace->count_values > 0) {
      assert(
          workspace->items_buffer[
              workspace->count_values - 1].feature_number
          < workspace->items_buffer[
              workspace->count_values].feature_number);
    }
    workspace->count_values += 1;
    if (output->filled) {
      return 1;
    }
  }
}

void mmap_write_items(char* start, struct item_list* items,
                      int64_t num_items, int64_t base_offset) {
  struct item_list* current_item = items;
  struct mmap_item* write_items = (struct mmap_item*)start;
  int64_t write_features_offset = num_items * sizeof(struct mmap_item);
  while (current_item != NULL) {
    int64_t item_number = current_item->id;
    write_items[item_number].id = current_item->id;
    write_items[item_number].sum_or_norm = current_item->sum_or_norm;
    write_items[item_number].count_features = current_item->count_items;
    write_items[item_number].features_offset
        = write_features_offset + base_offset;
    struct mmap_feature* write_features = (struct mmap_feature*)(
      start + write_features_offset);
    int64_t features_size = sizeof(struct mmap_feature)
        * current_item->count_items;
    memcpy(write_features, current_item->items, features_size);
    free(current_item->items);
    write_features_offset += features_size;
    struct item_list *previous_item = current_item;
    current_item = current_item->next;
    if (previous_item != NULL) {
      free(previous_item);
    }
  }
  assert(current_item == NULL);
}

struct item_list* read_items(int64_t *size_bytes,
                             int64_t *num_items,
                             const char *file_name,
                             int square_sum,
                             int64_t item_object_length) {
  struct item_list* item_head = NULL;
  struct item_list* item_tail = NULL;
  int64_t count_bytes = 0;
  int64_t count_items = 0;
  
  struct tuple_file_iterator it_workspace;
  init_tuple_file_iterator(&it_workspace, MAX_NUM_FEATURES,
                           file_name, square_sum);
  struct iterator_out it_out;
  reset_iterator_out(&it_out);
  while (get_next_item(&it_workspace, &it_out)) {
    struct item_list* current_item = (struct item_list*) malloc(
        sizeof(struct item_list));
    current_item->next = NULL;
    if (item_head == NULL) {
      item_head = current_item;
      item_tail = current_item;
    } else {
      item_tail->next = current_item;
      item_tail = current_item;
    }
    count_bytes += it_out.count_values * sizeof(struct mmap_feature);
    current_item->items = it_out.items;
    current_item->count_items = it_out.count_values;
    current_item->id = it_out.left_id;
    if (count_items < current_item->id + 1) {
      count_items = current_item->id + 1;
    }
    if (square_sum) {
      current_item->sum_or_norm = sqrt(it_out.sum_values);
    } else {
      current_item->sum_or_norm = it_out.sum_values;
    }
    reset_iterator_out(&it_out);
  }
  delete_tuple_file_iterator(&it_workspace);
  *num_items = count_items;
  count_bytes += item_object_length * count_items;
  *size_bytes = count_bytes;
  return item_head;
}

char *create_mmap(const char *file_name, int64_t length, int *outfd) {
  *outfd = open(file_name,
                O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
  if (*outfd < 0) {
    fprintf(stderr, "Could not create mmap file\n");
    exit(1);
  }
  lseek(*outfd, length - 1, SEEK_SET);
  if (write(*outfd, "", 1) != 1) {
    fprintf(stderr, "Could not write to file\n");
    exit(1);
  }
  lseek(*outfd, 0, SEEK_SET);
  char *mmap_addr = mmap(NULL, length,
                         PROT_READ | PROT_WRITE, MAP_SHARED, *outfd, 0);
  if (mmap_addr == MAP_FAILED) {
    fprintf(stderr, "Could not memory map file\n");
    exit(1);
  }
  memset(mmap_addr, 0, length);
  return mmap_addr;
}

void set_mmap_header(
    char *mmap, int64_t data_offset, int64_t item_count) {
  struct mmap_header *header = (struct mmap_header*)mmap;
  header->data_offset = data_offset;
  header->item_count = item_count;
}

void write_mmap(const char* file_name, int64_t total_length,
                int64_t count_items, struct item_list *items) {
  printf("Writing %s: %" PRId64 " bytes\n", file_name, total_length);
  int64_t mmap_size = sizeof(struct mmap_header) + total_length;
  int outfd;
  char *mmap = create_mmap(
      file_name,
      mmap_size,
      &outfd);
  set_mmap_header(mmap, sizeof(struct mmap_header),
                  count_items);
  mmap_write_items(mmap + sizeof(struct mmap_header),
                   items, count_items, sizeof(struct mmap_header));
  munmap(mmap, mmap_size);
  close(outfd);
  printf("%s written\n", file_name);
}

void transcribe_items(const char *in_file, const char *out_file,
                      int use_norm) {
  int64_t total_length;
  int64_t num_items;
  printf("Reading %s...\n", in_file);
  struct item_list *items = read_items(
      &total_length, &num_items, in_file, use_norm,
      sizeof(struct mmap_item));
  printf("%s read\n", in_file);
  write_mmap(out_file, sizeof(struct mmap_header) + total_length,
             num_items, items);
}

void transcribe_controversy(const char *in_file, const char *out_file) {
  struct mmap_feature *mmap_feature_buffer = calloc(
      MAX_PAGE_DID, sizeof(struct mmap_feature));
  int64_t max_id = -1;
  FILE *in_fid = fopen(in_file, "r");
  int64_t feature_id;
  double feature_value;
  while (fscanf(in_fid, "%" PRId64 " %lf", &feature_id, &feature_value)
         != EOF) {
    if (feature_id > max_id) {
      max_id = feature_id;
    }
    assert(feature_id < MAX_PAGE_DID);
    mmap_feature_buffer[feature_id].feature_number = feature_id;
    mmap_feature_buffer[feature_id].feature_value = feature_value;
  }
  fclose(in_fid);
  int64_t mmap_size = sizeof(struct mmap_header)
    + (max_id + 1) * sizeof(struct mmap_feature);
  int mmap_outfd;
  char *mmap = create_mmap(out_file, mmap_size, &mmap_outfd);
  set_mmap_header(mmap, sizeof(struct mmap_header), max_id + 1);
  memcpy(mmap + sizeof(struct mmap_header), mmap_feature_buffer,
         (max_id + 1) * sizeof(struct mmap_feature));
  free(mmap_feature_buffer);
  munmap(mmap, mmap_size);
  close(mmap_outfd);
  printf("Wrote %s\n", out_file);
}

int main(int argc, char **argv) {
  if (argc != 4) {
    printf("Usage: %s users_file pages_file controversy_file\n",
           argv[0]);
    exit(1);
  }
  if (strcmp(argv[1], "_") != 0) {
    transcribe_items(argv[1], "users_mmap", 0);
  }
  if (strcmp(argv[2], "_") != 0) {
    transcribe_items(argv[2], "pages_mmap", 1);
  }
  if (strcmp(argv[3], "_") != 0) {
    transcribe_controversy(argv[3], "controversy_mmap");
  }
  return 0;
}
