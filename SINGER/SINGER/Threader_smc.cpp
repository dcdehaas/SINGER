//
//  Threader_smc.cpp
//  SINGER
//
//  Created by Yun Deng on 4/4/23.
//

#include "Threader_smc.hpp"

Threader_smc::Threader_smc(float c, float q, shared_ptr<Emission> e) {
    cutoff = c;
    gap = q;
    eh = e;
}

Threader_smc::~Threader_smc() {
}

void Threader_smc::thread(ARG &a, Node_ptr n) {
    cout << "Iteration: " << a.sample_nodes.size() << endl;
    // cut_time = 0.005;
    cut_time = max(0.5/10000, min(0.005, 0.1/a.sample_nodes.size()));
    a.cut_time = cut_time;
    a.add_sample(n);
    get_boundary(a);
    cout << get_time() << " : begin BSP" << endl;
    run_BSP(a);
    cout << "BSP avg num of states: " << bsp.avg_num_states() << endl;
    cout << get_time() << " : begin sampling branches" << endl;
    sample_joining_branches(a);
    cout << get_time() << " : begin TSP" << endl;
    run_TSP(a);
    cout << get_time() << " : begin sampling points" << endl;
    sample_joining_points(a);
    cout << get_time() << " : begin adding" << endl;
    a.add(new_joining_branches, added_branches);
    cout << get_time() << " : begin sampling recombination" << endl;
    a.smc_sample_recombinations();
    a.clear_remove_info();
    cout << get_time() << " : finish" << endl;
    cout << a.recombinations.size() << endl;
}

void Threader_smc::fast_thread(ARG &a, Node_ptr n) {
    cout << "Iteration: " << a.sample_nodes.size() << endl;
    // cut_time = 0.005;
    cut_time = max(0.5/10000, min(0.005, 0.1/a.sample_nodes.size()));
    a.cut_time = cut_time;
    a.add_sample(n);
    get_boundary(a);
    cout << get_time() << " : begin pruner" << endl;
    run_pruner(a);
    cout << get_time() << " : begin BSP" << endl;
    run_fast_BSP(a);
    cout << "BSP avg num of states: " << fbsp.avg_num_states() << endl;
    cout << get_time() << " : begin sampling branches" << endl;
    sample_fast_joining_branches(a);
    cout << get_time() << " : begin TSP" << endl;
    run_TSP(a);
    cout << get_time() << " : begin sampling points" << endl;
    sample_joining_points(a);
    cout << get_time() << " : begin adding" << endl;
    a.add(new_joining_branches, added_branches);
    cout << get_time() << " : begin sampling recombination" << endl;
    a.smc_sample_recombinations();
    a.clear_remove_info();
    cout << get_time() << " : finish" << endl;
    cout << a.recombinations.size() << endl;
}

void Threader_smc::internal_rethread(ARG &a, tuple<float, Branch, float> cut_point) {
    cut_time = get<2>(cut_point);
    a.remove(cut_point);
    get_boundary(a);
    set_check_points(a);
    run_BSP(a);
    sample_joining_branches(a);
    run_TSP(a);
    sample_joining_points(a);
    float ar = acceptance_ratio(a);
    float q = random();
    if (q < ar) {
        a.add(new_joining_branches, added_branches);
    } else {
        a.add(a.joining_branches, a.removed_branches);
    }
    a.smc_sample_recombinations();
    a.clear_remove_info();
}


void Threader_smc::terminal_rethread(ARG &a, tuple<float, Branch, float> cut_point) {
    cut_time = get<2>(cut_point);
    a.remove(cut_point);
    get_boundary(a);
    set_check_points(a);
    run_BSP(a);
    cout << "BSP avg num states: " << bsp.avg_num_states() << endl;
    sample_joining_branches(a);
    run_TSP(a);
    sample_joining_points(a);
    a.add(new_joining_branches, added_branches);
    a.smc_sample_recombinations();
    a.clear_remove_info();
}

void Threader_smc::fast_internal_rethread(ARG &a, tuple<float, Branch, float> cut_point) {
    cut_time = get<2>(cut_point);
    // a.write("/Users/yun_deng/Desktop/SINGER/arg_files/full_ts_nodes.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/full_ts_branches.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/full_ts_recombs.txt");
    a.remove(cut_point);
    // a.write("/Users/yun_deng/Desktop/SINGER/arg_files/partial_ts_nodes.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/partial_ts_branches.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/partial_ts_recombs.txt");
    get_boundary(a);
    set_check_points(a);
    run_pruner(a);
    run_fast_BSP(a);
    sample_fast_joining_branches(a);
    run_TSP(a);
    sample_joining_points(a);
    float prev_length = a.get_arg_length(a.joining_branches, a.removed_branches);
    float next_length = a.get_arg_length(new_joining_branches, added_branches);
    float q = random();
    if (q < prev_length/next_length) {
        a.add(new_joining_branches, added_branches);
    } else {
        a.add(a.joining_branches, a.removed_branches);
    }
    a.smc_sample_recombinations();
    a.clear_remove_info();
    // a.write("/Users/yun_deng/Desktop/SINGER/arg_files/full_ts_nodes.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/full_ts_branches.txt", "/Users/yun_deng/Desktop/SINGER/arg_files/full_ts_recombs.txt");
}

void Threader_smc::fast_terminal_rethread(ARG &a, tuple<float, Branch, float> cut_point) {
    cut_time = get<2>(cut_point);
    a.remove(cut_point);
    get_boundary(a);
    set_check_points(a);
    run_pruner(a);
    run_fast_BSP(a);
    // cout << "BSP avg number of states: " << fbsp.avg_num_states() << endl;
    sample_fast_joining_branches(a);
    run_TSP(a);
    sample_joining_points(a);
    a.add(new_joining_branches, added_branches);
    a.smc_sample_recombinations();
    a.clear_remove_info();
}

void Threader_smc::get_boundary(ARG &a) {
    start = a.start;
    end = a.end;
    start_index = a.get_index(start);
    end_index = a.get_index(end);
}

void Threader_smc::set_check_points(ARG &a) {
    set<float> check_points = a.get_check_points();
    bsp.set_check_points(check_points);
    tsp.set_check_points(check_points);
}

void Threader_smc::run_pruner(ARG &a) {
    pruner.prune_arg(a);
}

void Threader_smc::run_BSP(ARG &a) {
    bsp.reserve_memory(end_index - start_index);
    bsp.set_cutoff(cutoff);
    // bsp.set_emission(eh);
    bsp.set_emission(e);
    bsp.start(a.start_tree.branches, cut_time);
    auto recomb_it = a.recombinations.upper_bound(start);
    auto mut_it = a.mutation_sites.lower_bound(start);
    auto query_it = a.removed_branches.begin();
    vector<float> mutations;
    set<float> mut_set = {};
    set<Branch> deletions = {};
    set<Branch> insertions = {};
    Node_ptr query_node = nullptr;
    for (int i = start_index; i < end_index; i++) {
        if (a.coordinates[i] == query_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        if (a.coordinates[i] == recomb_it->first) {
            Recombination &r = recomb_it->second;
            recomb_it++;
            bsp.transfer(r);
        } else if (a.coordinates[i] != start) {
            bsp.forward(a.rhos[i - 1]);
        }
        mut_set = {};
        while (*mut_it < a.coordinates[i + 1]) {
            mut_set.insert(*mut_it);
            mut_it++;
        }
        if (mut_set.size() > 0) {
            bsp.mut_emit(a.thetas[i], a.coordinates[i + 1] - a.coordinates[i], mut_set, query_node);
        } else {
            bsp.null_emit(a.thetas[i], query_node);
        }
    }
}


void Threader_smc::run_fast_BSP(ARG &a) {
    fbsp.reserve_memory(end_index - start_index);
    fbsp.set_cutoff(cutoff);
    // fbsp.set_emission(eh);
    fbsp.set_emission(e);
    set<Interval_info> start_intervals = pruner.insertions.begin()->second;
    fbsp.start(a.start_tree.branches, start_intervals, cut_time);
    // fbsp.start(start_intervals, cut_time);
    auto recomb_it = a.recombinations.upper_bound(start);
    auto mut_it = a.mutation_sites.lower_bound(start);
    auto query_it = a.removed_branches.begin();
    auto delete_it = pruner.deletions.upper_bound(start);
    auto insert_it = pruner.insertions.upper_bound(start);
    vector<float> mutations;
    set<float> mut_set = {};
    Node_ptr query_node = nullptr;
    for (int i = start_index; i < end_index; i++) {
        if (a.coordinates[i] == query_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        while (delete_it->first <= a.coordinates[i]) {
            fbsp.update_states(delete_it->second, insert_it->second);
            delete_it++;
            insert_it++;
        }
        if (a.coordinates[i] == recomb_it->first) {
            Recombination &r = recomb_it->second;
            recomb_it++;
            fbsp.transfer(r);
        } else if (a.coordinates[i] != start) {
            fbsp.forward(a.rhos[i - 1]);
        }
        mut_set = {};
        while (*mut_it < a.coordinates[i+1]) {
            mut_set.insert(*mut_it);
            mut_it++;
        }
        if (mut_set.size() > 0) {
            fbsp.mut_emit(a.thetas[i], a.coordinates[i+1] - a.coordinates[i], mut_set, query_node);
        } else {
            fbsp.null_emit(a.thetas[i], query_node);
        }
    }
}

void Threader_smc::run_TSP(ARG &a) {
    tsp.reserve_memory(end_index - start_index);
    tsp.set_gap(gap);
    tsp.set_emission(eh);
    Branch start_branch = new_joining_branches.begin()->second;
    tsp.start(start_branch, cut_time);
    auto recomb_it = a.recombinations.upper_bound(start);
    auto join_it = new_joining_branches.upper_bound(start);
    auto mut_it = a.mutation_sites.lower_bound(start);
    auto query_it = a.removed_branches.lower_bound(start);
    Branch prev_branch = start_branch;
    Branch next_branch = start_branch;
    Node_ptr query_node = nullptr;
    set<float> mut_set = {};
    for (int i = start_index; i < end_index; i++) {
        if (a.coordinates[i] == query_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        if (a.coordinates[i] == join_it->first) {
            next_branch = join_it->second;
            join_it++;
        }
        if (a.coordinates[i] == recomb_it->first) {
            Recombination &r = recomb_it->second;
            recomb_it++;
            tsp.transfer(r, prev_branch, next_branch);
            prev_branch = next_branch;
        } else if (prev_branch != next_branch) {
            tsp.recombine(prev_branch, next_branch);
            prev_branch = next_branch;
        } else if (a.coordinates[i] != start) {
            float rho = a.rhos[i];
            tsp.forward(rho);
        }
        mut_set.clear();
        while (*mut_it < a.coordinates[i+1]) {
            mut_set.insert(*mut_it);
            mut_it++;
        }
        if (mut_set.size() > 0) {
            tsp.mut_emit(a.thetas[i], a.coordinates[i+1] - a.coordinates[i], mut_set, query_node);
        } else {
            tsp.null_emit(a.thetas[i], query_node);
        }
    }
}

void Threader_smc::sample_joining_branches(ARG &a) {
    new_joining_branches = bsp.sample_joining_branches(start_index, a.coordinates);
}

void Threader_smc::sample_fast_joining_branches(ARG &a) {
    new_joining_branches = fbsp.sample_joining_branches(start_index, a.coordinates);
}

void Threader_smc::sample_joining_points(ARG &a) {
    map<float, Node_ptr> added_nodes = tsp.sample_joining_nodes(start_index, a.coordinates);
    auto add_it = added_nodes.begin();
    auto end_it = added_nodes.end();
    Node_ptr query_node = nullptr;
    Node_ptr added_node = nullptr;
    float x;
    while (add_it != end_it) {
        x = add_it->first;
        added_node = add_it->second;
        query_node = a.get_query_node_at(x);
        /*
        if (query_node == nullptr) {
            added_branches[x] = Branch();
        } else {
            added_branches[x] = Branch(query_node, added_node);
        }
         */
        added_branches[x] = Branch(query_node, added_node);
        add_it++;
    }
}

/*
float Threader_smc::acceptance_ratio(ARG &a) {
    float prev_length = a.get_arg_length(a.joining_branches, a.removed_branches);
    float next_length = a.get_arg_length(new_joining_branches, added_branches);
    return prev_length/next_length;
}
 */

float Threader_smc::acceptance_ratio(ARG &a) {
    auto old_join_it = a.joining_branches.upper_bound(a.cut_pos);
    old_join_it--;
    auto new_join_it = new_joining_branches.upper_bound(a.cut_pos);
    new_join_it--;
    auto old_add_it = a.removed_branches.upper_bound(a.cut_pos);
    old_add_it--;
    auto new_add_it = added_branches.upper_bound(a.cut_pos);
    new_add_it--;
    float old_length = a.cut_tree.length();
    float new_length = old_length;
    old_length += old_add_it->second.upper_node->time - a.cut_time;
    if (old_join_it->second.upper_node->index == -1) {
        old_length += old_add_it->second.upper_node->time - old_join_it->second.lower_node->time;
    }
    old_length += new_add_it->second.upper_node->time - a.cut_time;
    if (new_join_it->second.upper_node->index == -1) {
        new_length += new_add_it->second.upper_node->time - new_join_it->second.lower_node->time;
    }
    return old_length/new_length;
}

float Threader_smc::random() {
    return (float) rand()/RAND_MAX;
}

/*
void Threader_smc::run_fast_BSP(ARG &a) {
    fbsp.reserve_memory(end_index - start_index);
    fbsp.set_cutoff(cutoff);
    fbsp.set_emission(eh);
    set<Branch> start_branches = pruner.reductions.begin()->second;
    fbsp.start(start_branches, cut_time);
    auto recomb_it = a.recombinations.upper_bound(start);
    auto mut_it = a.mutation_sites.lower_bound(start);
    auto query_it = a.removed_branches.begin();
    auto reduction_it = pruner.reductions.begin();
    vector<float> mutations;
    set<float> mut_set = {};
    Node_ptr query_node = nullptr;
    for (int i = start_index; i < end_index; i++) {
        if (a.coordinates[i] == query_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        while (reduction_it->first <= a.coordinates[i]) {
            fbsp.set_states(reduction_it->second);
            reduction_it++;
        }
        if (a.coordinates[i] == recomb_it->first) {
            Recombination &r = recomb_it->second;
            recomb_it++;
            fbsp.transfer(r);
        } else if (a.coordinates[i] != start) {
            fbsp.forward(a.rhos[i - 1]);
        }
        mut_set = {};
        while (*mut_it < a.coordinates[i+1]) {
            mut_set.insert(*mut_it);
            mut_it++;
        }
        if (mut_set.size() > 0) {
            fbsp.mut_emit(a.thetas[i], a.coordinates[i+1] - a.coordinates[i], mut_set, query_node);
        } else {
            fbsp.null_emit(a.thetas[i], query_node);
        }
    }
}
 */

/*
void Threader_smc::run_fast_BSP(ARG &a) {
    fbsp.reserve_memory(end_index - start_index);
    fbsp.set_cutoff(cutoff);
    fbsp.set_emission(eh);
    set<Interval_info> start_intervals = pruner.insertions.begin()->second;
    fbsp.start(start_intervals, cut_time);
    auto recomb_it = a.recombinations.upper_bound(start);
    auto mut_it = a.mutation_sites.lower_bound(start);
    auto query_it = a.removed_branches.begin();
    auto delete_it = pruner.deletions.upper_bound(start);
    auto insert_it = pruner.insertions.upper_bound(start);
    vector<float> mutations;
    set<float> mut_set = {};
    Node_ptr query_node = nullptr;
    for (int i = start_index; i < end_index; i++) {
        if (a.coordinates[i] == query_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        while (delete_it->first <= a.coordinates[i]) {
            fbsp.update_states(delete_it->second, insert_it->second);
            delete_it++;
            insert_it++;
        }
        if (a.coordinates[i] == recomb_it->first) {
            Recombination &r = recomb_it->second;
            recomb_it++;
            fbsp.transfer(r);
        } else if (a.coordinates[i] != start) {
            fbsp.forward(a.rhos[i - 1]);
        }
        mut_set = {};
        while (*mut_it < a.coordinates[i+1]) {
            mut_set.insert(*mut_it);
            mut_it++;
        }
        if (mut_set.size() > 0) {
            fbsp.mut_emit(a.thetas[i], a.coordinates[i+1] - a.coordinates[i], mut_set, query_node);
        } else {
            fbsp.null_emit(a.thetas[i], query_node);
        }
    }
}
 */
