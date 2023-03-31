//
//  Test.cpp
//  SINGER
//
//  Created by Yun Deng on 3/17/23.
//

#include "Test.hpp"

void test_read_arg() {
    ARG a = ARG(2e4, 1e6);
    a.read("/Users/yun_deng/Desktop/SINGER/arg_files/continuous_ts_nodes.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/continuous_ts_branches.txt");
    a.discretize(10);
    for (Node *n : a.sample_nodes) {
        int index = n->index;
        string mutation_file = "/Users/yun_deng/Desktop/SINGER/arg_files/continuous_sample_" + to_string(index) + ".txt";
        n->read_mutation(mutation_file);
        a.add_sample(n);
    }
    a.impute_nodes(0, 1e6);
    a.map_mutations(0, 1e6);
    cout << "Number of incompatibilities: " << a.count_incompatibility() <<  endl;
}

void test_pruner() {
    srand(23491256);
    ARG a = ARG(2e4, 1e6);
    a.read("/Users/yun_deng/Desktop/SINGER/arg_files/continuous_ts_100_nodes.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/continuous_ts_100_branches.txt");
    a.discretize(10);
    a.smc_sample_recombinations();
    for (Node *n : a.sample_nodes) {
        int index = n->index;
        string mutation_file = "/Users/yun_deng/Desktop/SINGER/arg_files/continuous_sample_100_" + to_string(index) + ".txt";
        n->read_mutation(mutation_file);
        a.add_sample(n);
    }
    a.impute_nodes(0, 1e6);
    a.map_mutations(0, 1e6);
    map<float, pair<Branch, Node *>> joining_points = a.remove_leaf(99);
    Parsimony_pruner parsimony_pruner = Parsimony_pruner();
    parsimony_pruner.prune_arg(a, a.base_nodes);
    // parsimony_pruner.check_reduction(joining_points);
    parsimony_pruner.print_reduction_size();
}
