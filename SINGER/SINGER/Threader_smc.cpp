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

void Threader_smc::thread(ARG &a, Node *n) {
    cut_time = 0.0;
    a.add_sample(n);
    get_boundary(a);
    run_BSP(a);
    run_TSP(a);
    // sample_joining_points(a);
    // a.add(new_joining_branches, added_branches);
    // a.smc_sample_recombinations();
}

void Threader_smc::internal_rethread(ARG &prev_arg, tuple<int, Branch, float> cut_point) {
    ARG next_arg = prev_arg;
    cut_time = get<2>(cut_point);
    get_boundary(next_arg);
    next_arg.remove(cut_point);
    set_check_points(next_arg);
    run_BSP(next_arg);
    run_TSP(next_arg);
    sample_joining_points(next_arg);
    next_arg.add(new_joining_branches, added_branches);
    float start = added_branches.begin()->first;
    float end = added_branches.rbegin()->first;
    float prev_length = prev_arg.get_arg_length(start, end);
    float next_length = next_arg.get_arg_length(start, end);
    float q = random();
    if (q < prev_length/next_length) {
        prev_arg = next_arg;
        prev_arg.clear_memory();
    } else {
        prev_arg.clear_memory(added_branches);
    }
}

void Threader_smc::terminal_rethread(ARG &a, tuple<int, Branch, float> cut_point) {
    cut_time = get<2>(cut_point);
    get_boundary(a);
    a.remove(cut_point);
    set_check_points(a);
    run_BSP(a);
    run_TSP(a);
    sample_joining_points(a);
    a.add(new_joining_branches, added_branches);
    a.clear_memory();
}

string Threader_smc::get_time() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);

    std::tm bt = *std::localtime(&timer);
    std::ostringstream oss;
    oss << "[" << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "]";
    return oss.str();
}

void Threader_smc::get_boundary(ARG &a) {
    start = a.removed_branches.begin()->first;
    end = a.removed_branches.rbegin()->first;
    start_index = a.get_index(start);
    end_index = a.get_index(end);
}

void Threader_smc::set_check_points(ARG &a) {
    set<float> check_points = a.get_check_points();
    bsp.set_check_points(check_points);
    tsp.set_check_points(check_points);
}

void Threader_smc::run_BSP(ARG &a) {
    Tree start_tree = a.get_tree_at(start);
    bsp.set_cutoff(cutoff);
    bsp.set_emission(eh);
    bsp.start(start_tree.branches, cut_time);
    map<float, Recombination>::iterator recomb_it = a.recombinations.upper_bound(start);
    set<float>::iterator mut_it = a.mutation_sites.lower_bound(start);
    map<float, Branch>::iterator query_it = a.removed_branches.begin();
    vector<float> mutations;
    set<float> mut_set = {};
    Node *query_node = nullptr;
    for (int i = start_index; i < end_index; i++) {
        if (a.coordinates[i] == query_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        if (a.coordinates[i] == recomb_it->first) {
            Recombination r = recomb_it->second;
            recomb_it++;
            bsp.transfer(r);
        } else if (a.coordinates[i] != start) {
            bsp.forward(a.rhos[i]);
        }
        mut_set = {};
        while (*mut_it < a.coordinates[i+1]) {
            mut_set.insert(*mut_it);
            mut_it++;
        }
        if (mut_set.size() > 0) {
            // bsp.null_emit(a.thetas[i], query_node);
            bsp.mut_emit(a.thetas[i], a.coordinates[i+1] - a.coordinates[i], mut_set, query_node);
        } else {
            bsp.null_emit(a.thetas[i], query_node);
        }
    }
}

void Threader_smc::run_TSP(ARG &a) {
    new_joining_branches = bsp.sample_joining_branches(start_index, a.coordinates);
    Branch start_branch = new_joining_branches.begin()->second;
    tsp.reserve_memory(end_index - start_index);
    tsp.set_gap(gap);
    tsp.set_emission(eh);
    tsp.start(start_branch, cut_time);
    map<float, Recombination>::iterator recomb_it = a.recombinations.upper_bound(start);
    map<float, Branch>::iterator joining_it = new_joining_branches.upper_bound(start);
    set<float>::iterator mut_it = a.mutation_sites.lower_bound(start);
    map<float, Branch>::iterator query_it = a.removed_branches.lower_bound(start);
    Branch prev_branch = start_branch;
    Branch next_branch = start_branch;
    Node *query_node = nullptr;
    set<float> mut_set = {};
    cout << get_time() << endl;
    for (int i = start_index; i < end_index; i++) {
        if (a.coordinates[i] == query_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        if (a.coordinates[i] == joining_it->first) {
            next_branch = joining_it->second;
            joining_it++;
        }
        if (i == recomb_it->first) {
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
        mut_set = {};
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
    cout << get_time() << endl;
}

void Threader_smc::sample_joining_points(ARG &a) {
    map<float, Node *> added_nodes = tsp.sample_joining_nodes(start_index, a.coordinates);
    map<float, Branch>::iterator query_it = a.removed_branches.begin();
    map<float, Node *>::iterator add_it = added_nodes.begin();
    map<float, Node *>::iterator end_it = added_nodes.end();
    Node *query_node = nullptr;
    Node *added_node = nullptr;
    while (add_it != end_it) {
        added_node = add_it->second;
        if (query_it->first == add_it->first) {
            query_node = query_it->second.lower_node;
            query_it++;
        }
        added_branches[add_it->first] = Branch(query_node, added_node);
        add_it++;
    }
}

float Threader_smc::random() {
    return (float) rand()/RAND_MAX;
}
