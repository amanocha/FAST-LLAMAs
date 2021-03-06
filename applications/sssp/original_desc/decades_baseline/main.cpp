#include "DECADES/DECADES.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <climits>
#include "../../common/graph.h"
#include "../../../../utils/common.h"

void _kernel_(int* hop, int compute_nodes, int compute_edges, csr_graph G, int * ret, int *in_wl, int* in_index, int *out_wl, int *out_index, int tid, int num_threads) {
  int num_edges = 0;

  for (int i = tid; i < compute_nodes; i+=num_threads) {
    int node = in_wl[i];
    int start = G.node_array[node];
    int end = G.node_array[node+1];
    if (num_edges + (end-start) > compute_edges) {
      end = compute_edges - num_edges + start;
    }
    num_edges += (end-start);
    for (int e = start; e < end; e++) {	
      int edge_index = G.edge_array[e];
      weightT curr_dist = ret[edge_index];
      weightT new_dist = ret[node] + G.node_data[e];	
      if (new_dist < curr_dist) {
        ret[edge_index] = new_dist;	    
	int index = *out_index;
        *out_index = *out_index + 1;
	out_wl[index] = edge_index;
      }
    }
    if (num_edges == compute_edges) {
      printf("num_edges\n");
      break;
    }
  }
}

void sssp(int epoch, int compute_nodes, int compute_edges, csr_graph G, int * ret, int *in_wl, int* in_index, int *out_wl, int *out_index, int tid, int num_threads) {

  int* hop = (int*)malloc(sizeof(int));
  *hop = 1;

  while (in_index[0] > 0) {
    printf("-- epoch %d %d\n", *hop, *in_index);
    int init = tid;
    if (*hop == epoch) {
      if (compute_nodes == -1 || compute_nodes > *in_index) {
	compute_nodes = *in_index;
      }
      printf("----going into kernel! Computing %d nodes\n", compute_nodes);
      auto start = chrono::system_clock::now();
      _kernel_(hop, compute_nodes, compute_edges, G, ret, in_wl, in_index, out_wl, out_index, tid, num_threads);
      auto end = std::chrono::system_clock::now();
      chrono::duration<double> elapsed_seconds = end-start;
      cout << "\ndecades kernel computation time: " << elapsed_seconds.count() << "s\n";
      printf("----finished kernel! doing %d nodes in x86\n", *in_index - compute_nodes);
      return;
      init = compute_nodes;
    }
    for (int i = init; i < in_index[0]; i+=num_threads) {
      int node = in_wl[i];
      for (int e = G.node_array[node]; e < G.node_array[node+1]; e++) {	
	int edge_index = G.edge_array[e];
	weightT curr_dist = ret[edge_index];
        weightT new_dist = ret[node] + G.node_data[e];
	if (new_dist < curr_dist) {
	  ret[edge_index] = new_dist;
	  int index = *out_index;
          *out_index = *out_index + 1;
	  out_wl[index] = edge_index;
	}
      }
    }
    int *tmp = out_wl;
    out_wl = in_wl;
    in_wl = tmp;
    *hop = *hop + 1;
    *in_index = *out_index;
    *out_index = 0;
  }
  free(hop);  
}

int main(int argc, char** argv) {

  char *graph_fname;
  csr_graph G;

  assert(argc >= 2);
  graph_fname = argv[1];
  int epoch = -1;
  if (argc >= 3) {
    epoch = atoi(argv[2]);
  }
  int compute_nodes = -1;
  int compute_edges = -1;
  if (argc >= 4) {
    compute_edges = atoi(argv[3]);
  }
  int seed = 0;
  if (argc >= 5) {
    seed = atoi(argv[4]);
  }
 
  //G = parse_csr_graph(graph_fname);
  G = parse_bin_files(graph_fname);

  int * ret = (int *) malloc(sizeof(int) * G.nodes);

  for (int i = 0; i < G.nodes; i++) {
    ret[i] = weight_max;
  }
  
  int * in_index = (int *) malloc(sizeof(int) * 1);
  *in_index = 0;
  int * out_index = (int *) malloc(sizeof(int) * 1);
  *out_index = 0;
  
  int * in_wl = (int *) malloc(sizeof(int) * G.nodes * 5);
  int * out_wl = (int *) malloc(sizeof(int) * G.nodes * 5);

  for (int i = seed; i < seed+SEEDS; i++) {
    int index = *in_index;
    *in_index = index + 1;
    in_wl[index] = i;
    ret[i] = 0;
  }

  printf("\n\nstarting kernel\n");
  auto start = chrono::system_clock::now();

  sssp(epoch, compute_nodes, compute_edges, G, ret, in_wl, in_index, out_wl, out_index, 0 , 1);

  printf("\nending kernel");
  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  cout << "\nkernel computation time: " << elapsed_seconds.count() << "s\n";

#if defined(OUTPUT_RET)
  ofstream outfile;
  outfile.open ("SSSP_out.txt");
  for (int i = 0; i < G.nodes; i++) {
    outfile << ret[i] << "\n";
  }
  outfile.close();
  return 0;

#endif
  
  free(ret);
  free(in_index);
  free(out_index);
  free(in_wl);
  free(out_wl);
  clean_csr_graph(G);

  return 0;
}
