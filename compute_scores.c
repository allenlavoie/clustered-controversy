#include "stdlib.h"

#include "compute_scores.h"

void set_node(struct dense_graph graph, int node_number,
              double controversy, double edits, int real_id) {
  struct node_info *node = graph.nodes + node_number;
  node->controversy = controversy;
  node->edits = edits;
  node->real_id = real_id;
}

void set_edge(struct dense_graph graph,
              int first_node, int second_node,
              double weight) {
  graph.edges[graph.num_nodes * first_node + second_node] = weight;
  graph.edges[graph.num_nodes * second_node + first_node] = weight;
}

struct dense_graph make_graph(int num_nodes) {
  struct dense_graph ret;
  ret.num_nodes = num_nodes;
  ret.edges = (double *)calloc((size_t)num_nodes * (size_t)num_nodes,
                               sizeof(double));
  ret.nodes = (struct node_info*)calloc((size_t)num_nodes,
                                        sizeof(struct node_info));
  return ret;
}

void free_graph(struct dense_graph graph) {
  free(graph.edges);
  free(graph.nodes);
  graph.edges = NULL;
  graph.nodes = NULL;
}

double coeff(struct dense_graph graph, FILE* coeff_out,
             double* avg_cont, double* avg_clust) {
  for (int i = 0; i < graph.num_nodes; ++i) {
    graph.nodes[i].numerator = 0.0;
    graph.nodes[i].denominator = 0.0;
  }
  for (int i = 0; i < graph.num_nodes; ++i) {
    for (int j = i + 1; j < graph.num_nodes; ++j) {
      double ij_edge = graph.edges[graph.num_nodes * i + j];
      for (int k = j + 1; k < graph.num_nodes; ++k) {
        double ik_edge = graph.edges[graph.num_nodes * i + k];
        double jk_edge = graph.edges[graph.num_nodes * j + k];
        double numerator_addition = ij_edge * jk_edge * ik_edge;
	
        double editfraction = graph.nodes[j].edits * graph.nodes[k].edits
            * graph.nodes[j].controversy * graph.nodes[k].controversy;
        graph.nodes[i].numerator += numerator_addition * editfraction;
        graph.nodes[i].denominator += ij_edge * ik_edge * editfraction;
        
        editfraction = graph.nodes[i].edits * graph.nodes[k].edits
            * graph.nodes[i].controversy * graph.nodes[k].controversy;
        graph.nodes[j].numerator += numerator_addition * editfraction;
        graph.nodes[j].denominator += ij_edge * jk_edge * editfraction;
        
        editfraction = graph.nodes[i].edits * graph.nodes[j].edits
            * graph.nodes[i].controversy * graph.nodes[j].controversy;
        graph.nodes[k].numerator += numerator_addition * editfraction;
        graph.nodes[k].denominator += jk_edge * ik_edge * editfraction;
      }
    }
  }
  double average_coeff = 0.0;
  double average_cont = 0.0;
  double average_clust = 0.0;
  for (int i = 0; i < graph.num_nodes; ++i) {
    double vertex_coeff;
    double numerator = graph.nodes[i].numerator;
    double denominator = graph.nodes[i].denominator;
    if (denominator == 0.0) {
      vertex_coeff = 0.0;
    } else {
      vertex_coeff = numerator / denominator;
    }
    if (coeff_out != NULL) {
      fprintf(coeff_out,
              " %d:%1.6e/%1.6e/%1.6e",
              graph.nodes[i].real_id,
              graph.nodes[i].controversy,
              vertex_coeff,
              graph.nodes[i].edits);
    }
    average_coeff += graph.nodes[i].edits
        * graph.nodes[i].controversy
        * vertex_coeff;
    average_cont += graph.nodes[i].edits
        * graph.nodes[i].controversy;
    average_clust += graph.nodes[i].edits
        * vertex_coeff;
  }
  if (avg_cont != NULL) {
    *avg_cont = average_cont;
  }
  if (avg_clust != NULL) {
    *avg_clust = average_clust;
  }
  if (coeff_out != NULL) {
    fprintf(coeff_out, "\n");
  }
  return average_coeff;
}
