//
//  BSP_smc.hpp
//  SINGER
//
//  Created by Yun Deng on 4/2/23.
//

#ifndef BSP_smc_hpp
#define BSP_smc_hpp

#include <stdio.h>
#include <fstream>
#include "Tree.hpp"
#include "Coalescent_calculator.hpp"
#include "Interval.hpp"
#include "Emission.hpp"

class BSP_smc {
    
public:
    
    // basic setup
    float cut_time = 0.0;
    float cutoff = 0;
    shared_ptr<Emission> eh;
    set<float> check_points = {};
    
    // hmm running results
    vector<float> rhos = {};
    vector<float> recomb_sums = {}; // length: number of blocks - 1
    vector<float> weight_sums = {}; // length: number of blocks
    
    // hmm states
    int curr_index = 0;
    map<int, vector<Interval *>>  state_spaces = {{INT_MAX, {}}};
    vector<Interval *> curr_intervals = {};
    
    // coalescent computation
    shared_ptr<Coalescent_calculator> cc;
    
    // transfer at recombinations
    map<Interval *, vector<Interval *>> source_intervals = {};
    map<Interval *, vector<float>> source_weights = {};
    map<Interval_info, vector<Interval *>> temp_intervals = {};
    map<Interval_info, vector<float>> temp_weights = {};
    
    // cache:
    float prev_rho = -1;
    float prev_theta = -1;
    Node *prev_node = nullptr;
    
    // pruning:
    
    set<Branch> deleted_branches = {};
    set<Branch> inserted_branches = {};
    
    // vector computation:
    int dim = 0;
    float recomb_sum = 0;
    float weight_sum = 0;
    vector<float> temp = {};
    vector<float> recomb_probs = {};
    vector<float> recomb_weights = {};
    vector<float> null_emit_probs = {};
    vector<float> mut_emit_probs = {};
    int sample_index = -1;
    vector<float> trace_back_probs = {};
    vector<vector<float>> forward_probs = {};
    
    BSP_smc();
    
    ~BSP_smc();
    
    void reserve_memory(int length);
    
    void start(set<Branch> &branches, float t);
    
    void set_cutoff(float x);
    
    void set_emission(shared_ptr<Emission> e);
    
    void set_check_points(set<float> &p);
    
    void forward(float rho); // forward pass when there is no recombination (without emission). Don't forget to update recomb_sums and weight_sums.
    
    void transfer(Recombination &r); // forward pass when there is a recombination (without emission), and add a transition object. Don't forget to update active intervals, recomb_sums and weight_sums.
    
    void forward(float rho, set<Branch> &deletions, set<Branch> &insertions);
    
    void transfer(Recombination &r, set<Branch> &deletions, set<Branch> &insertions);
    
    float get_recomb_prob(float rho, float t);
    
    void null_emit(float theta, Node *query_node);
    
    void mut_emit(float theta, float bin_size, set<float> &mut_set, Node *query_node);
    
    map<float, Branch> sample_joining_branches(int start_index, vector<float> &coordinates);
    
    void write_forward_probs(string filename);
    
    // private methods:
    
    void set_dimensions();
    
    void compute_recomb_probs(float rho);
    
    void compute_recomb_weights(float rho);
    
    void compute_null_emit_prob(float theta, Node *query_node);
    
    void compute_mut_emit_probs(float theta, float bin_size, set<float> &mut_set, Node *query_node);
    
    void transfer_helper(Interval_info next_interval, Interval *prev_interval, float w);
    
    void transfer_helper(Interval_info next_interval);
    
    void add_new_branches(Recombination &r);
    
    void compute_interval_info();
    
    void sanity_check(Recombination &r);
    
    void generate_intervals(Recombination &r);
    
    float get_overwrite_prob(Recombination &r, float lb, float ub);
    
    void process_interval(Recombination &r, int i);
    
    void process_source_interval(Recombination &r, int i);
    
    void process_target_interval(Recombination &r, int i);
    
    void process_other_interval(Recombination &r, int i);
    
    float random();
    
    int get_prev_breakpoint(int x);
    
    vector<Interval *> get_state_space(int x);
    
    int get_interval_index(Interval *interval, vector<Interval *> &intervals);
    
    void simplify(map<float, Branch> &joining_branches);
    
    Interval *sample_curr_interval(int x);
    
    Interval *sample_prev_interval(int x);
    
    Interval *sample_source_interval(Interval *interval, int x);
    
    int trace_back_helper(Interval *interval, int x);
    
};

#endif /* BSP_smc_hpp */
