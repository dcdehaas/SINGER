//
//  sub_BSP.cpp
//  SINGER
//
//  Created by Yun Deng on 6/17/23.
//

#include "sub_BSP.hpp"

sub_BSP::sub_BSP() {}

sub_BSP::~sub_BSP() {
    vector<vector<double>>().swap(forward_probs);
    map<int, vector<Interval_ptr>>().swap(state_spaces);
}

void sub_BSP::reserve_memory(int length) {
    forward_probs.reserve(length);
}

void sub_BSP::start(set<Branch> &start_branches, set<Interval_info> &start_intervals, double t) {
    cut_time = t;
    curr_index = 0;
    set<Interval_info> empty_set = {};
    update_states(empty_set, start_intervals);
    double lb = 0;
    double ub = 0;
    double p = 0;
    Interval_ptr new_interval = nullptr;
    cc = make_shared<fast_coalescent_calculator>(cut_time);
    cc->start(start_branches);
    for (const Branch &b : reduced_branches) {
        if (b.upper_node->time > cut_time) {
            lb = max(b.lower_node->time, cut_time);
            ub = b.upper_node->time;
            p = cc->prob(lb, ub);
            new_interval = create_interval(b, lb, ub, curr_index);
            new_interval->source_pos = curr_index;
            curr_intervals.emplace_back(new_interval);
            temp_probs.emplace_back(p);
        }
    }
    forward_probs.emplace_back(temp_probs);
    reduced_sums.emplace_back(0.0);
    set_dimensions();
    compute_interval_info();
    state_spaces[curr_index] = curr_intervals;
    temp_probs.clear();
}

void sub_BSP::set_cutoff(double x) {
    cutoff = x;
}

void sub_BSP::set_emission(shared_ptr<Emission> e) {
    eh = e;
}

void sub_BSP::set_check_points(set<double> &p) {
    check_points = p;
}

void sub_BSP::forward(double rho) {
    if (branch_change) {
        update(rho);
    } else {
        regular_forward(rho);
    }
    branch_change = false;
}


void sub_BSP::transfer(Recombination &r) {
    rhos.emplace_back(0);
    prev_rho = -1;
    prev_theta = -1;
    recomb_sums.emplace_back(0);
    reduced_sums.emplace_back(0);
    sanity_check(r);
    curr_index += 1;
    transfer_weights.clear();
    transfer_intervals.clear();
    temp_probs.clear();
    temp_intervals.clear();
    covered_branches.clear();
    for (int i = 0; i < curr_intervals.size(); i++) {
        process_interval(r, i);
    }
    generate_intervals(r);
    state_spaces[curr_index] = curr_intervals;
}

void sub_BSP::regular_forward(double rho) {
    rhos.emplace_back(rho);
    compute_recomb_probs(rho);
    prev_rho = rho;
    curr_index += 1;
    recomb_sum = inner_product(recomb_probs.begin(), recomb_probs.end(), forward_probs[curr_index - 1].begin(), 0.0);
    forward_probs.emplace_back(recomb_probs);
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] = forward_probs[curr_index - 1][i]*(1 - recomb_probs[i]) + recomb_sum*join_weights[i];
    }
    recomb_sums.emplace_back(recomb_sum);
    reduced_sums.emplace_back(reduced_sum);
}

void sub_BSP::update(double rho) {
    double lb, ub, p;
    Interval_ptr prev_interval, new_interval;
    Branch prev_branch;
    rhos.emplace_back(rho);
    compute_recomb_probs(rho);
    prev_rho = -1;
    prev_theta = -1;
    curr_index += 1;
    recomb_sum = inner_product(recomb_probs.begin(), recomb_probs.end(), forward_probs[curr_index - 1].begin(), 0.0);
    temp_probs.clear();
    temp_intervals.clear();
    covered_branches.clear();
    for (int i = 0; i < dim; i++) {
        prev_interval = curr_intervals[i];
        prev_branch = prev_interval->branch;
        if (reduced_branches.count(prev_branch) > 0) {
            temp_intervals.emplace_back(prev_interval);
            p = forward_probs[curr_index - 1][i]*(1 - recomb_probs[i]);
            temp_probs.emplace_back(p);
            covered_branches.insert(prev_branch);
        }
    }
    for (Branch b : reduced_branches) {
        if (covered_branches.count(b) == 0) {
            lb = max(cut_time, b.lower_node->time);
            ub = b.upper_node->time;
            new_interval = create_interval(b, lb, ub, curr_index);
            new_interval->source_pos = curr_index;
            temp_intervals.emplace_back(new_interval);
            temp_probs.emplace_back(0);
        }
    }
    forward_probs.emplace_back(temp_probs);
    curr_intervals = temp_intervals;
    state_spaces[curr_index] = curr_intervals;
    set_dimensions();
    compute_interval_info();
    compute_recomb_probs(rho);
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] += recomb_sum*join_weights[i];
        // assert(!isnan(forward_probs[curr_index][i]) and forward_probs[curr_index][i] >= 0);
    }
    assert(recomb_sum > 0);
    recomb_sums.emplace_back(recomb_sum);
    reduced_sums.emplace_back(reduced_sum);
}

double sub_BSP::get_recomb_prob(double rho, double t) {
    // double p = rho*(t - cut_time)*exp(-rho*(t - cut_time));
    double p = rho*(t - cut_time);
    assert(p < 0.2);
    return p;
}

void sub_BSP::null_emit(double theta, Node_ptr query_node) {
    compute_null_emit_prob(theta, query_node);
    prev_theta = theta;
    prev_node = query_node;
    double ws = 0;
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] *= null_emit_probs[i];
        ws += forward_probs[curr_index][i];
    }
    assert(ws > 0);
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] /= ws;
    }
}

void sub_BSP::mut_emit(double theta, double bin_size, set<double> &mut_set, Node_ptr query_node) {
    compute_mut_emit_probs(theta, bin_size, mut_set, query_node);
    double ws = 0;
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] *= mut_emit_probs[i];
        ws += forward_probs[curr_index][i];
    }
    assert(ws > 0);
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] /= ws;
    }
}

map<double, Branch> sub_BSP::sample_joining_branches(int start_index, vector<double> &coordinates) {
    prev_rho = -1;
    map<double, Branch> joining_branches = {};
    int x = curr_index;
    int y = 0;
    double pos = coordinates[x + start_index + 1];
    Interval_ptr interval = sample_curr_interval(x);
    Branch b = interval->branch;
    joining_branches[pos] = b;
    while (x >= 0) {
        x = trace_back_helper(interval, x);
        b = interval->branch;
        pos = coordinates[x + start_index];
        joining_branches[pos] = b;
        y = get_prev_breakpoint(x);
        if (x == 0) {
            break;
        } else if (x == y) {
            x -= 1;
            if (rhos[x] == 0) {
                interval = sample_source_interval(interval, x);
                b = interval->branch;
            } else {
                interval = sample_connection_interval(interval, x);
                b = interval->branch;
            }
        } else {
            x -= 1;
            interval = sample_prev_interval(x);
            b = interval->branch;
        }
    }
    simplify(joining_branches);
    return joining_branches;
}

double sub_BSP::avg_num_states() {
    int span = 0;
    double count = 0;
    auto x = state_spaces.begin();
    ++x;
    while (x->first != INT_MAX) {
        count += x->second.size()*(x->first - prev(x)->first);
        span = x->first;
        ++x;
    }
    double avg = (double) count/span;
    return avg;
}

// private methods:

/*
bool sub_BSP::intercept(Interval_ptr interval) {
    set<Interval_info> &intervals = reduced_intervals[interval->branch];
    for (auto &x : intervals) {
        if (interval->lb <= x.ub and interval->ub >= x.lb) {
            return true;
        }
    }
    return false;
}

bool sub_BSP::intercept(Interval_info &ii) {
    set<Interval_info> &intervals = reduced_intervals[ii.branch];
    for (auto &x : intervals) {
        if (ii.lb <= x.ub and ii.ub >= x.lb) {
            return true;
        }
    }
    return false;
}
 */

void sub_BSP::update_states(set<Interval_info> &deletions, set<Interval_info> &insertions) {
    for (const Interval_info &ii : deletions) {
        const Branch &b = ii.branch;
        set<Interval_info> &intervals = reduced_intervals[b];
        assert(intervals.count(ii) > 0);
        intervals.erase(ii);
        if (intervals.size() == 0) {
            reduced_intervals.erase(b);
            reduced_branches.erase(b);
            branch_change = true;
        }
    }
    for (const Interval_info &ii : insertions) {
        const Branch b = ii.branch;
        if (reduced_branches.count(b) == 0) {
            reduced_branches.insert(b);
            reduced_intervals[b];
            branch_change = true;
        }
        set<Interval_info> &intervals = reduced_intervals[b];
        intervals.insert(ii);
    }
    assert(reduced_branches.size() == reduced_intervals.size() and reduced_branches.size() > 0);
}

void sub_BSP::set_dimensions() {
    dim = (int) curr_intervals.size();
    recomb_probs.resize(dim); recomb_probs.assign(dim, 0);
    join_times.resize(dim); join_times.assign(dim, 0);
    join_weights.resize(dim); join_weights.assign(dim, 0);
    null_emit_probs.resize(dim); null_emit_probs.assign(dim, 0);
    mut_emit_probs.resize(dim); mut_emit_probs.assign(dim, 0);
}

void sub_BSP::compute_recomb_probs(double rho) {
    if (prev_rho == rho) {
        return;
    }
    for (int i = 0; i < dim; i++) {
        recomb_probs[i] = get_recomb_prob(rho, join_times[i]);
    }
}

void sub_BSP::compute_null_emit_prob(double theta, Node_ptr query_node) {
    if (theta == prev_theta and query_node == prev_node) {
        return;
    }
    for (int i = 0; i < dim; i++) {
        null_emit_probs[i] = eh->null_emit(curr_intervals[i]->branch, join_times[i], theta, query_node);
    }
}

void sub_BSP::compute_mut_emit_probs(double theta, double bin_size, set<double> &mut_set, Node_ptr query_node) {
    for (int i = 0; i < dim; i++) {
        mut_emit_probs[i] = eh->mut_emit(curr_intervals[i]->branch, join_times[i], theta, bin_size, mut_set, query_node);
    }
}

void sub_BSP::transfer_helper(Interval_info &next_interval, Interval_ptr &prev_interval, double w) {
    if (reduced_branches.count(next_interval.branch) == 0) {
        return;
    }
    transfer_weights[next_interval].emplace_back(w);
    transfer_intervals[next_interval].emplace_back(prev_interval);
}

void sub_BSP::compute_interval_info() {
    double t;
    double p;
    for (int i = 0; i < curr_intervals.size(); i++) {
        Interval_ptr interval = curr_intervals[i];
        tie(t, p) = cc->compute_time_weights(interval->lb, interval->ub);
        join_times[i] = t;
        assert(t > cut_time);
        if (interval->full(cut_time)) {
            join_weights[i] = p;
        }
    }
    reduced_sum = accumulate(join_weights.begin(), join_weights.end(), 0.0f);
    all_join_times[curr_index] = join_times;
    all_join_weights[curr_index] = join_weights;
}

void sub_BSP::sanity_check(Recombination &r) {
    for (int i = 0; i < curr_intervals.size(); i++) {
        Interval_ptr interval = curr_intervals[i];
        if (interval->lb == interval->ub and interval->lb == r.inserted_node->time and interval->branch != r.target_branch) {
            forward_probs[curr_index][i] = 0;
        }
    }
}

void sub_BSP::get_full_branches(Recombination &r) {
    double lb, ub, p;
    for (auto &x : transfer_weights) {
        const Branch &b = x.first.branch;
        lb = x.first.lb;
        ub = x.first.ub;
        p = accumulate(x.second.begin(), x.second.end(), 0.0f);
        if (lb == max(cut_time, b.lower_node->time) and ub == b.upper_node->time and p > 0) {
            full_branches.insert(b);
        }
    }
    for (int i = 0; i < curr_intervals.size(); i++) {
        p = forward_probs[curr_index - 1][i];
        Interval_ptr prev_interval = curr_intervals[i];
        if (prev_interval->full(cut_time) and p > 0) {
            full_branches.insert(prev_interval->branch);
        }
    }
}

void sub_BSP::generate_intervals(Recombination &r) {
    full_branches.clear();
    get_full_branches(r);
    Branch b;
    double lb;
    double ub;
    double p;
    int min_source;
    vector<Interval_ptr> intervals;
    vector<double> weights;
    Interval_info interval;
    Interval_ptr new_interval;
    auto y = transfer_intervals.begin();
    for (auto x = transfer_weights.begin(); x != transfer_weights.end(); ++x, ++y) {
        interval = x->first;
        auto &weights = x->second;
        auto &intervals = y->second;
        b = interval.branch;
        lb = interval.lb;
        ub = interval.ub;
        p = accumulate(weights.begin(), weights.end(), 0.0);
        min_source = min_source_pos(intervals);
        assert(!isnan(p));
        if (lb == max(cut_time, b.lower_node->time) and ub == b.upper_node->time) { // full intervals
            new_interval = create_interval(b, lb, ub, curr_index);
            new_interval->source_pos = min_source;
            temp_intervals.emplace_back(new_interval);
            temp_probs.emplace_back(p);
            if (weights.size() > 0) {
                new_interval->source_weights = move(weights);
                new_interval->intervals = move(intervals);
            }
            covered_branches.insert(b);
        } else if (full_branches.count(b) == 0) {
            new_interval = create_interval(b, lb, ub, curr_index);
            new_interval->source_pos = min_source;
            temp_intervals.emplace_back(new_interval);
            temp_probs.emplace_back(p);
            if (weights.size() > 0) {
                new_interval->source_weights = move(weights);
                new_interval->intervals = move(intervals);
            }
        } else if (p > cutoff and curr_index - min_source < max_length) {
            new_interval = create_interval(b, lb, ub, curr_index);
            new_interval->source_pos = min_source;
            temp_intervals.emplace_back(new_interval);
            temp_probs.emplace_back(p);
            if (weights.size() > 0) {
                new_interval->source_weights = move(weights);
                new_interval->intervals = move(intervals);
            }
        }
    }
    for (int i = 0; i < curr_intervals.size(); i++) {
        p = forward_probs[curr_index - 1][i];
        Interval_ptr prev_interval = curr_intervals[i];
        if (!r.affect(prev_interval->branch)) {
            if (prev_interval->full(cut_time)) {
                temp_intervals.emplace_back(prev_interval);
                temp_probs.emplace_back(p);
                covered_branches.insert(prev_interval->branch);
            } else if (full_branches.count(prev_interval->branch) == 0) {
                temp_intervals.emplace_back(prev_interval);
                temp_probs.emplace_back(p);
            } else if (p > cutoff and curr_index - prev_interval->source_pos <= max_length) {
                temp_intervals.emplace_back(prev_interval);
                temp_probs.emplace_back(p);
            }
        }
    }
    for (Branch b : reduced_branches) {
        if (covered_branches.count(b) == 0) {
            lb = max(cut_time, b.lower_node->time);
            ub = b.upper_node->time;
            new_interval = create_interval(b, lb, ub, curr_index);
            new_interval->source_pos = curr_index;
            temp_intervals.emplace_back(new_interval);
            temp_probs.emplace_back(0);
        }
    }
    forward_probs.emplace_back(temp_probs);
    curr_intervals = temp_intervals;
    set_dimensions();
    cc->update(r);
    compute_interval_info();
}

double sub_BSP::get_overwrite_prob(Recombination &r, double lb, double ub) {
    if (check_points.count(r.pos) > 0) {
        return 0.0;
    }
    double join_time = r.inserted_node->time;
    double p1 = cc->prob(lb, ub);
    double p2 = cc->prob(max(cut_time, r.start_time), join_time);
    if (p1 == 0 and p2 == 0) {
        return 1.0;
    }
    double overwrite_prob = p2/(p1 + p2);
    return overwrite_prob;
}

void sub_BSP::process_interval(Recombination &r, int i) {
    Interval_ptr &prev_interval = curr_intervals[i];
    Branch &prev_branch = prev_interval->branch;
    if (!r.affect(prev_branch)) {
        ;
    } else if (prev_branch == r.source_branch) {
        process_source_interval(r, i);
    } else if (prev_branch == r.target_branch) {
        process_target_interval(r, i);
    } else {
        process_other_interval(r, i);
    }
}

void sub_BSP::process_source_interval(Recombination &r, int i) {
    double w1, w2, lb, ub = 0;
    Interval_ptr prev_interval = curr_intervals[i];
    double p = forward_probs[curr_index - 1][i];
    double point_time = r.source_branch.upper_node->time;
    double break_time = r.start_time;
    Branch next_branch;
    Interval_info next_interval;
    if (prev_interval->ub <= break_time) {
        lb = prev_interval->lb;
        ub = prev_interval->ub;
        next_branch = r.recombined_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, p);
    } else if (prev_interval->lb >= break_time) {
        lb = point_time;
        ub = point_time;
        next_branch = r.merging_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, p);
    } else {
        w1 = cc->prob(prev_interval->lb, break_time);
        w2 = cc->prob(break_time, prev_interval->ub);
        if (w1 == 0 and w2 == 0) {
            w1 = 1;
            w2 = 0;
        } else {
            w1 = w1/(w1 + w2);
            w2 = 1 - w1;
        }
        lb = prev_interval->lb;
        ub = break_time;
        next_branch = r.recombined_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, w1*p);
        lb = point_time;
        ub = point_time;
        next_branch = r.merging_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, w2*p);
    }
}

void sub_BSP::process_target_interval(Recombination &r, int i) {
    double w0, w1, w2, lb, ub = 0;
    Interval_ptr prev_interval = curr_intervals[i];
    double p = forward_probs[curr_index - 1][i];
    double join_time = r.inserted_node->time;
    Branch next_branch;
    Interval_info next_interval;
    if (prev_interval->lb == prev_interval->ub and prev_interval->lb == join_time) {
        lb = max(cut_time, r.start_time);
        ub = r.recombined_branch.upper_node->time;
        next_branch = r.recombined_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, p);
    } else if (prev_interval->lb >= join_time) {
        lb = prev_interval->lb;
        ub = prev_interval->ub;
        next_branch = r.upper_transfer_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, p);
    } else if (prev_interval->ub <= join_time) {
        lb = prev_interval->lb;
        ub = prev_interval->ub;
        next_branch = r.lower_transfer_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, p);
    } else {
        w0 = get_overwrite_prob(r, prev_interval->lb, prev_interval->ub);
        w1 = cc->prob(prev_interval->lb, join_time);
        w2 = cc->prob(join_time, prev_interval->ub);
        if (w1 + w2 == 0) {
            w1 = 0;
            w2 = 0;
            w0 = 1.0;
        } else {
            w1 = w1/(w1 + w2);
            w2 = 1 - w1;
            w1 *= 1 - w0;
            w2 *= 1 - w0;
        }
        lb = prev_interval->lb;
        ub = join_time;
        next_branch = r.lower_transfer_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, w1*p);
        lb = join_time;
        ub = prev_interval->ub;
        next_branch = r.upper_transfer_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, w2*p);
        lb = max(r.start_time, cut_time);
        ub = r.recombined_branch.upper_node->time;
        next_branch = r.recombined_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, w0*p);
    }
}

void sub_BSP::process_other_interval(Recombination &r, int i) {
    double lb, ub = 0;
    Interval_ptr &prev_interval = curr_intervals[i];
    double p = forward_probs[curr_index - 1][i];
    lb = prev_interval->lb;
    ub = prev_interval->ub;
    Branch &next_branch = r.merging_branch;
    Interval_info next_interval = Interval_info(next_branch, lb, ub);
    transfer_helper(next_interval, prev_interval, p);
}

double sub_BSP::random() {
    double p = uniform_random();
    return p;
}

int sub_BSP::get_prev_breakpoint(int x) {
    auto state_it = state_spaces.upper_bound(x);
    state_it--;
    return state_it->first;
}

vector<Interval_ptr > &sub_BSP::get_state_space(int x) {
    auto state_it = state_spaces.upper_bound(x);
    state_it--;
    return state_it->second;
}

vector<double> &sub_BSP::get_join_times(int x) {
    auto time_it = all_join_times.upper_bound(x);
    time_it--;
    return time_it->second;
}

vector<double> &sub_BSP::get_join_weights(int x) {
    auto weight_it = all_join_weights.upper_bound(x);
    weight_it--;
    return weight_it->second;
}

int sub_BSP::get_interval_index(Interval_ptr interval, vector<Interval_ptr > &intervals) {
    auto it = find(intervals.begin(), intervals.end(), interval);
    int index = (int) distance(intervals.begin(), it);
    return index;
}
 
void sub_BSP::simplify(map<double, Branch> &joining_branches) {
    map<double, Branch> simplified_joining_branches = {};
    Branch curr_branch = joining_branches.begin()->second;
    simplified_joining_branches[joining_branches.begin()->first] = curr_branch;
    for (auto x : joining_branches) {
        if (x.second != curr_branch) {
            simplified_joining_branches.insert(x);
            curr_branch = x.second;
        }
    }
    simplified_joining_branches[joining_branches.rbegin()->first] = joining_branches.rbegin()->second;
    joining_branches = simplified_joining_branches;
}

Interval_ptr sub_BSP::sample_curr_interval(int x) {
    vector<Interval_ptr > &intervals = get_state_space(x);
    double ws = accumulate(forward_probs[x].begin(), forward_probs[x].end(), 0.0);
    double q = random();
    double w = ws*q;
    for (int i = 0; i < intervals.size(); i++) {
        w -= forward_probs[x][i];
        if (w <= 0) {
            sample_index = i;
            return intervals[i];
        }
    }
    cerr << "bsp curr sampling failed" << endl;
    exit(1);
}

Interval_ptr sub_BSP::sample_prev_interval(int x) {
    vector<Interval_ptr > &intervals = get_state_space(x);
    vector<double> &prev_times = get_join_times(x);
    double rho = rhos[x];
    double ws = recomb_sums[x];
    double q = random();
    double w = ws*q;
    for (int i = 0; i < intervals.size(); i++) {
        w -= get_recomb_prob(rho, prev_times[i])*forward_probs[x][i];
        if (w <= 0) {
            sample_index = i;
            return intervals[i];
        }
    }
    cerr << "fbsp prev sampling failed" << endl;
    exit(1);
}

Interval_ptr sub_BSP::sample_source_interval(Interval_ptr interval, int x) {
    vector<Interval_ptr> &intervals = interval->intervals;
    vector<double> &weights = interval->source_weights;
    vector<Interval_ptr> &prev_intervals = get_state_space(x);
    if (x == interval->start_pos - 1) {
        double q = random();
        double ws = accumulate(weights.begin(), weights.end(), 0.0);
        double w = ws*q;
        for (int i = 0; i < weights.size(); i++) {
            w -= weights[i];
            if (w <= 0) {
                sample_index = get_interval_index(intervals[i], prev_intervals);
                return intervals[i];
            }
        }
        cerr << "sampling failed" << endl;
        exit(1);
    } else {
        sample_index = get_interval_index(interval, prev_intervals);
        assert(prev_intervals[sample_index] == interval);
        return interval;
    }
}

Interval_ptr sub_BSP::sample_connection_interval(Interval_ptr interval, int x) {
    vector<Interval_ptr > &prev_intervals = get_state_space(x);
    vector<double> &prev_times = get_join_times(x);
    vector<double> &next_weights = get_join_weights(x + 1);
    int n = (int) prev_intervals.size();
    double source_recomb_prob, target_proportion;
    vector<double> weights = vector<double>(n);
    recomb_sum = recomb_sums[x];
    reduced_sum = reduced_sums[x + 1];
    target_proportion = next_weights[sample_index];
    for (int i = 0; i < n; i++) {
        source_recomb_prob = forward_probs[x][i]*get_recomb_prob(rhos[x], prev_times[i]); // probability that goes out from i-state
        if(interval == prev_intervals[i]) {
            weights[i] += forward_probs[x][i] - source_recomb_prob;
        }
        weights[i] += source_recomb_prob*target_proportion;
    }
    double q = random();
    double ws = accumulate(weights.begin(), weights.end(), 0.0);
    double w = ws*q;
    for (int i = 0; i < weights.size(); i++) {
        w -= weights[i];
        if (w <= 0) {
            sample_index = i;
            return prev_intervals[i];
        }
    }
    cerr << "fbsp sample connection interval failed" << endl;
    exit(1);
}

int sub_BSP::trace_back_helper(Interval_ptr interval, int x) {
    int y = get_prev_breakpoint(x);
    if (!interval->full(cut_time)) {
        return y;
    }
    double p = random();
    double q = 1;
    double shrinkage = 0;
    double recomb_prob = 0;
    double non_recomb_prob = 0;
    double all_prob = 0;
    vector<double> &prev_times = get_join_times(x);
    vector<double> &prev_weights = get_join_weights(x);
    assert(sample_index < prev_times.size());
    double t = prev_times[sample_index];
    double w = prev_weights[sample_index];
    while (x > y) {
        recomb_sum = recomb_sums[x - 1];
        reduced_sum = reduced_sums[x];
        if (recomb_sum == 0) {
            shrinkage = 1;
        } else {
            recomb_prob = get_recomb_prob(rhos[x - 1], t);
            non_recomb_prob = (1 - recomb_prob)*forward_probs[x - 1][sample_index];
            // all_prob = non_recomb_prob + recomb_sum*w;
            all_prob = non_recomb_prob + recomb_sum*w + recomb_sum*forward_probs[x - 1][sample_index]*(1 - reduced_sum);
            shrinkage = non_recomb_prob/all_prob;
            assert(!isnan(shrinkage));
            assert(shrinkage >= 0 and shrinkage <= 1);
        }
        q *= shrinkage;
        if (p >= q) {
            return x;
        }
        x -= 1;
    }
    return y;
}

int sub_BSP::min_source_pos(vector<Interval_ptr> &intervals) {
    int min_pos = curr_index;
    for (Interval_ptr i : intervals) {
        min_pos = min(i->source_pos, min_pos);
    }
    return min_pos;
}
