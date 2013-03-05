#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>

#include "compute_scores.h"
#include "score_thread.h"
#include "read_mmap.h"
#include "queue.h"

struct feature_iterator {
  const struct user_group *group;
  int *current_positions;
  int64_t feature_id;
  double feature_value;
  const struct thread_info *tinfo;
};

#define USER_BUFFER_SIZE 10000

void init_feature_iterator(struct feature_iterator *it,
                           const struct user_group *group,
                           const struct thread_info *tinfo) {
  it->feature_id = -1;
  it->feature_value = 0.0;
  it->group = group;
  it->current_positions = calloc(group->num_users, sizeof(int));
  it->tinfo = tinfo;
}

int next_feature(struct feature_iterator *it) {
  it->feature_id = -1;
  for (int i = 0; i < it->group->num_users; ++i) {
    assert(it->group->userids[i] < it->tinfo->num_users);
    const struct mmap_item *user
        = it->tinfo->users + it->group->userids[i];
    if (it->current_positions[i] < user->count_features) {
      const struct mmap_feature *user_pages = get_features(
          it->tinfo->mmap_users, user);
      int64_t feature_id = user_pages[
          it->current_positions[i]].feature_number;
      if (it->feature_id == -1 || feature_id < it->feature_id) {
        it->feature_id = feature_id;
      }
    }
  }
  if (it->feature_id == -1) {
    return 0;
  }
  it->feature_value = 0.0;
  for (int i = 0; i < it->group->num_users; ++i) {
    const struct mmap_item *user
        = it->tinfo->users + it->group->userids[i];
    if (it->current_positions[i] < user->count_features) {
      const struct mmap_feature *user_pages = get_features(
          it->tinfo->mmap_users, user);
      if (it->feature_id
          == user_pages[it->current_positions[i]].feature_number) {
        it->feature_value += user_pages[
            it->current_positions[i]].feature_value;
        ++(it->current_positions[i]);
      }
    }
  }
  return 1;
}

void free_feature_iterator(struct feature_iterator *it) {
  free(it->current_positions);
  it->current_positions = NULL;
}

inline double div_ignore_zero(double x, double y) {
  if (y == 0.0) {
    assert(x == 0.0);
    return 0.0;
  } else {
    return (double)x / (double)y;
  }
}

double JSD(
    const char *pages_mfile,
    const struct mmap_item *first_page,
    const struct mmap_item *second_page) {
  const struct mmap_feature *first_features = get_features(
      pages_mfile, first_page);
  const struct mmap_feature *second_features = get_features(
      pages_mfile, second_page);
  if (first_features == NULL || second_features == NULL) {
    return 0.0;
  }
  double first_sum = 0.0;
  for (int i = 0; i < first_page->count_features; ++i) {
    first_sum += first_features[i].feature_value;
  }
  double second_sum = 0.0;
  for (int i = 0; i < second_page->count_features; ++i) {
    second_sum += second_features[i].feature_value;
  }
  double first_entropy = 0.0;
  double second_entropy = 0.0;
  double combined_entropy = 0.0;
  int first_i = 0;
  int second_i = 0;
  int first_feature_num;
  int second_feature_num;
  double first_feature_value;
  double second_feature_value;
  while (first_i < first_page->count_features
         || second_i < second_page->count_features) {
    if (first_i < first_page->count_features) {
      first_feature_num = first_features[first_i].feature_number;
    } else {
      first_feature_num = INT_MAX;
    }
    if (second_i < second_page->count_features) {
      second_feature_num = second_features[second_i].feature_number;
    } else {
      second_feature_num = INT_MAX;
    }
    if (first_feature_num == second_feature_num) {
      first_feature_value = first_features[first_i].feature_value;
      second_feature_value = second_features[second_i].feature_value;
      ++first_i;
      ++second_i;
    } else if (first_feature_num < second_feature_num) {
      first_feature_value = first_features[first_i].feature_value;
      second_feature_value = 0.0;
      ++first_i;
    } else {
      first_feature_value = 0.0;
      second_feature_value = second_features[second_i].feature_value;
      ++second_i;
    }
    first_feature_value /= first_sum;
    second_feature_value /= second_sum;
    if (first_feature_value > 0.0) {
      first_entropy += first_feature_value * log2(first_feature_value);
    }
    if (second_feature_value > 0.0) {
      second_entropy += second_feature_value * log2(second_feature_value);
    }
    if (first_feature_value > 0.0 || second_feature_value > 0.0) {
      double combined_value = (first_feature_value
                               + second_feature_value) / 2.0;
      combined_entropy += combined_value * log2(combined_value);
    }
  }
  // We take the -1 from entropy into account here
  return 1.0 - ((first_entropy + second_entropy) / 2.0 - combined_entropy);
}
  
double cosine_similarity(
    const char *pages_mfile,
    const struct mmap_item *first_page,
    const struct mmap_item *second_page) {
  const struct mmap_feature *first_features = get_features(
      pages_mfile, first_page);
  const struct mmap_feature *second_features = get_features(
      pages_mfile, second_page);
  if (first_features == NULL || second_features == NULL) {
    return 0.0;
  }
  int first_i = 0;
  int second_i = 0;
  double inner_product = 0.0;
  while (first_i < first_page->count_features
         && second_i < second_page->count_features) {
    int first_feature_num = first_features[first_i].feature_number;
    int second_feature_num = second_features[second_i].feature_number;
    if (first_feature_num == second_feature_num) {
      inner_product += first_features[first_i].feature_value
          * second_features[second_i].feature_value;
      ++first_i;
      ++second_i;
    } else if (first_feature_num < second_feature_num) {
      ++first_i;
    } else {
      ++second_i;
    }
  }
  return div_ignore_zero(
      inner_product,
      first_page->sum_or_norm * second_page->sum_or_norm);
}

void print_cc(const struct mmap_item *user,
              const struct mmap_feature *user_pages,
              const char *user_list,
              FILE *fp_cc_out,
              FILE *fp_c_out,
              struct thread_info *tinfo) {
  struct dense_graph graph = make_graph(user->count_features);
  for (int i = 0; i < user->count_features; ++i) {
    int64_t page_num = user_pages[i].feature_number;
    assert(page_num < tinfo->num_pages);
    assert(page_num < tinfo->num_controversy);
    assert(tinfo->controversy[page_num].feature_number == page_num
           || tinfo->controversy[page_num].feature_value == 0.0);
    set_node(graph, i,
             tinfo->controversy[page_num].feature_value,
             div_ignore_zero(user_pages[i].feature_value,
                             user->sum_or_norm),
             page_num);
    for (int j = i + 1; j < user->count_features; ++j) {
      double similarity = SIM_FUNC(
          tinfo->mmap_pages,
          tinfo->pages + page_num,
          tinfo->pages + user_pages[j].feature_number);
      set_edge(graph, i, j, similarity);
    }
  }
  fprintf(fp_c_out, "%s %" PRId64, user_list, user->count_features);
  double clust;
  double cont;
  double cc = coeff(graph, fp_c_out, &cont, &clust);
  fprintf(fp_cc_out, "%s %1.6e %1.6e %1.6e\n",
          user_list, cc, cont, clust);
  fflush(fp_c_out);
  fflush(fp_cc_out);
  free_graph(graph);
}

void* generate_scores(void *thread_info) {
  struct thread_info *tinfo = thread_info;
  FILE *fp_cc_out = fopen(tinfo->cc_output_file, "w");
  assert(fp_cc_out);
  FILE *fp_c_out = fopen(tinfo->c_output_file, "w");
  assert(fp_c_out);
  
  struct user_group *work;
  char user_buffer[USER_BUFFER_SIZE];
  while ((work = pop_front(tinfo->input_queue)) != NULL) {
    if (work->num_users == 1) {
      int64_t userid = work->userids[0];
      assert(userid < tinfo->num_users);
      const struct mmap_item *user = tinfo->users + userid;
      const struct mmap_feature *user_pages = get_features(
          tinfo->mmap_users, user);
      if (user_pages == NULL || user->count_features > 50000) {
        continue;
      }
      snprintf(user_buffer, USER_BUFFER_SIZE, "%" PRId64, userid);
      print_cc(user, user_pages, user_buffer, fp_cc_out, fp_c_out, tinfo);
    } else {
      struct mmap_item group_info;
      struct feature_iterator it;
      init_feature_iterator(&it, work, tinfo);
      group_info.count_features = 0;
      group_info.sum_or_norm = 0.0;
      while (next_feature(&it)) {
        ++group_info.count_features;
        group_info.sum_or_norm += it.feature_value;
      }
      free_feature_iterator(&it);
      group_info.id = -1;
      struct mmap_feature *group_pages = malloc(
          sizeof(struct mmap_feature) * group_info.count_features);
      init_feature_iterator(&it, work, tinfo);
      int64_t i = 0;
      while (next_feature(&it)) {
        assert (i < group_info.count_features);
        group_pages[i].feature_number = it.feature_id;
        group_pages[i].feature_value = it.feature_value;
        ++i;
      }
      assert (i == group_info.count_features);
      free_feature_iterator(&it);
      char *user_buffer_pos = user_buffer;
      for (i = 0; i < work->num_users; ++i) {
        user_buffer_pos += snprintf(
            user_buffer_pos,
            USER_BUFFER_SIZE - (user_buffer_pos - user_buffer),
            "%" PRId64 " ", work->userids[i]);
        
      }
      if (user_buffer_pos > user_buffer) {
        *(user_buffer_pos - 1) = '\0';
      }
      print_cc(&group_info, group_pages, user_buffer,
               fp_cc_out, fp_c_out, tinfo);
      free(group_pages);
    }
    free(work->userids);
    free(work);
  }
  fclose(fp_c_out);
  fclose(fp_cc_out);
  return NULL;
}
