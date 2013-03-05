/* Given a local edit graph (nodes are pages, edges are page
   similarities), compute its CC, controversy, and clustering
   scores. */

#include "stdio.h"

struct node_info {
  double controversy;
  double edits;
  int real_id;
  double numerator;
  double denominator;
};

struct dense_graph {
  int num_nodes;
  double *edges;
  struct node_info *nodes;
};

void set_node(struct dense_graph graph, int node_number,
              double controversy, double edits, int real_id);
void set_edge(struct dense_graph graph,
              int first_node, int second_node,
              double weight);
struct dense_graph make_graph(int num_nodes);
void free_graph(struct dense_graph graph);

/* Compute the CC, average controversy, and average clustering scores
   for the graph. Returns the CC score, and writes the page-level scores
   (controversy and clustering) to coeff_out. */
double coeff(struct dense_graph graph, FILE* coeff_out,
             double* avg_cont, double* avg_clust);
