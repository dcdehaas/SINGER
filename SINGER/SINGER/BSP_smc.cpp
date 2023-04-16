//
//  BSP_smc.cpp
//  SINGER
//
//  Created by Yun Deng on 4/2/23.
//

#include "BSP_smc.hpp"

BSP_smc::BSP_smc() {}

BSP_smc::~BSP_smc() {}

void BSP_smc::reserve_memory(int length) {
    forward_probs.reserve(length);
}

void BSP_smc::start(set<Branch> &branches, float t) {
    cut_time = t;
    curr_index = 0;
    float lb = 0;
    float ub = 0;
    float p = 0;
    Interval *new_interval = nullptr;
    coalescence_times = {cut_time};
    for (Branch b : branches) {
        if (b.upper_node->time > cut_time) {
            coalescence_times.insert(b.upper_node->time);
        }
    }
    calculate_coalescence_stats();
    for (Branch b : branches) {
        if (b.upper_node->time > cut_time) {
            lb = max(b.lower_node->time, cut_time);
            ub = b.upper_node->time;
            p = coalescence_probs.at(ub) - coalescence_probs.at(lb);
            new_interval = new Interval(b, lb, ub, curr_index);
            new_interval->hmm_index = (int) curr_intervals.size();
            curr_intervals.push_back(new_interval);
            temp.push_back(p);
        }
    }
    forward_probs.push_back(temp);
    compute_interval_info();
    set_dimensions();
    state_spaces[curr_index] = curr_intervals;
    temp.clear();
}

void BSP_smc::set_cutoff(float x) {
    cutoff = x;
}

void BSP_smc::set_emission(shared_ptr<Emission> e) {
    eh = e;
}

void BSP_smc::set_check_points(set<float> &p) {
    check_points = p;
}

void BSP_smc::forward(float rho) {
    rhos.push_back(rho);
    compute_recomb_probs(rho);
    compute_recomb_weights(rho);
    prev_rho = rho;
    recomb_sum = inner_product(recomb_probs.begin(), recomb_probs.end(), forward_probs[curr_index].begin(), 0.0);
    curr_index += 1;
    forward_probs.push_back(recomb_probs);
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] = forward_probs[curr_index-1][i]*(1 - recomb_probs[i]) + recomb_sum*recomb_weights[i];
    }
    recomb_sums.push_back(recomb_sum);
    weight_sums.push_back(weight_sum);
}


void BSP_smc::transfer(Recombination &r) {
    rhos.push_back(0);
    prev_rho = -1;
    prev_theta = -1;
    prev_node = nullptr;
    recomb_sums.push_back(0);
    sanity_check(r);
    curr_index += 1;
    temp_weights.clear();
    temp_intervals.clear();
    vector<Interval *> intervals = curr_intervals;
    for (Interval *i : intervals) {
        process_interval(r, i);
    }
    curr_intervals.clear();
    update_coalescence_times(r);
    calculate_coalescence_stats();
    add_new_branches(r);
    generate_intervals(r);
    set_dimensions();
    state_spaces[curr_index] = curr_intervals;
}

float BSP_smc::get_recomb_prob(float rho, float t) {
    float p = 1 - exp(-rho*(t - cut_time));
    return p;
}

void BSP_smc::null_emit(float theta, Node *query_node) {
    compute_null_emit_prob(theta, query_node);
    prev_theta = theta;
    prev_node = query_node;
    float ws = 0;
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] *= null_emit_probs[i];
        ws += forward_probs[curr_index][i];
    }
    assert(ws > 0);
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] /= ws;
    }
}

void BSP_smc::mut_emit(float theta, float bin_size, set<float> &mut_set, Node *query_node) {
    compute_mut_emit_probs(theta, bin_size, mut_set, query_node);
    float ws = 0;
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] *= mut_emit_probs[i];
        ws += forward_probs[curr_index][i];
    }
    assert(ws > 0);
    for (int i = 0; i < dim; i++) {
        forward_probs[curr_index][i] /= ws;
    }
}

map<float, Branch> BSP_smc::sample_joining_branches(int start_index, vector<float> &coordinates) {
    prev_rho = -1;
    map<float, Branch> joining_branches = {};
    int x = curr_index;
    float pos = coordinates[x + start_index + 1];
    Interval *interval = sample_curr_interval(x);
    Branch b = interval->branch;
    joining_branches[pos] = b;
    while (x >= 0) {
        x = trace_back_helper(interval, x);
        b = interval->branch;
        pos = coordinates[x + start_index];
        joining_branches[pos] = b;
        if (x == 0) {
            break;
        } else if (x == interval->start_pos) {
            x -= 1;
            interval = sample_source_interval(interval);
            b = interval->branch;
        } else {
            x -= 1;
            interval = sample_prev_interval(x);
            b = interval->branch;
        }
    }
    simplify(joining_branches);
    return joining_branches;
}

void BSP_smc::write_forward_probs(string filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Unable to open the file." << std::endl;
    }

    // Write the vector<vector<float>> to the file
    for (const auto &row : forward_probs) {
        for (size_t i = 0; i < row.size(); ++i) {
            file << row[i];
            if (i < row.size() - 1) {
                file << ' '; // separate values with a space (or use another delimiter if you prefer)
            }
        }
        file << '\n'; // add a newline character for each row
    }

    // Close the file
    file.close();
}

// private methods:

void BSP_smc::set_dimensions() {
    dim = (int) curr_intervals.size();
    recomb_probs.resize(dim); recomb_probs.assign(dim, 0);
    recomb_weights.resize(dim); recomb_weights.assign(dim, 0);
    null_emit_probs.resize(dim); null_emit_probs.assign(dim, 0);
    mut_emit_probs.resize(dim); mut_emit_probs.assign(dim, 0);
}

void BSP_smc::compute_recomb_probs(float rho) {
    if (prev_rho == rho) {
        return;
    }
    for (int i = 0; i < dim; i++) {
        recomb_probs[i] = get_recomb_prob(rho, curr_intervals[i]->time);
    }
}

void BSP_smc::compute_recomb_weights(float rho) {
    if (prev_rho == rho) {
        return;
    }
    for (int i = 0; i < dim; i++) {
        if (curr_intervals[i]->full(cut_time)) {
            recomb_weights[i] = recomb_probs[i]*curr_intervals[i]->weight;
        }
    }
    weight_sum = accumulate(recomb_weights.begin(), recomb_weights.end(), 0.0);
    for (int i = 0; i < dim; i++) {
        recomb_weights[i] /= weight_sum;
    }
}

void BSP_smc::compute_null_emit_prob(float theta, Node *query_node) {
    if (theta == prev_theta and query_node == prev_node) {
        return;
    }
    for (int i = 0; i < dim; i++) {
        null_emit_probs[i] = eh->null_emit(curr_intervals[i]->branch, curr_intervals[i]->time, theta, query_node);
    }
}

void BSP_smc::compute_mut_emit_probs(float theta, float bin_size, set<float> &mut_set, Node *query_node) {
    for (int i = 0; i < dim; i++) {
        mut_emit_probs[i] = eh->mut_emit(curr_intervals[i]->branch, curr_intervals[i]->time, theta, bin_size, mut_set, query_node);
    }
}

void BSP_smc::transfer_helper(Interval_info next_interval, Interval *prev_interval, float w) {
    if (w == 0) {
        return;
    }
    if (temp_weights.count(next_interval) > 0) {
        temp_weights.at(next_interval).push_back(w);
        temp_intervals.at(next_interval).push_back(prev_interval);
    } else {
        temp_weights.insert({next_interval, {w}});
        temp_intervals.insert({next_interval, {prev_interval}});
    }
}

void BSP_smc::add_new_branches(Recombination &r) { // add recombined branch and merging branch, if legal
    Interval_info next_interval;
    float lb = 0;
    float ub = 0;
    if (r.merging_branch != Branch() and r.merging_branch.upper_node->time > cut_time) {
        lb = max(cut_time, r.merging_branch.lower_node->time);
        ub = r.merging_branch.upper_node->time;
        next_interval = Interval_info(r.merging_branch, lb, ub);
        temp_weights.insert({next_interval, {}});
        temp_intervals.insert({next_interval, {}});
    }
    if (r.recombined_branch != Branch() and r.recombined_branch.upper_node->time > cut_time) {
        lb = max(cut_time, r.recombined_branch.lower_node->time);
        ub = r.recombined_branch.upper_node->time;
        next_interval = Interval_info(r.recombined_branch, lb, ub);
        temp_weights.insert({next_interval, {}});
        temp_intervals.insert({next_interval, {}});
    }
}

void BSP_smc::compute_interval_info() {
    float t;
    float p;
    for (Interval *i : curr_intervals) {
        p = get_prop(i->lb, i->ub);
        t = get_median(i->lb, i->ub);
        i->assign_time(t);
        i->assign_weight(p);
    }
    weight_sums.push_back(0.0);
}

void BSP_smc::sanity_check(Recombination &r) {
    
}

void BSP_smc::generate_intervals(Recombination &r) {
    Branch b;
    float lb;
    float ub;
    float p;
    vector<Interval *> intervals;
    vector<float> weights;
    Interval_info interval;
    Interval *new_interval = nullptr;
    for (auto x : temp_weights) {
        interval = x.first;
        weights = x.second;
        intervals = temp_intervals.at(x.first);
        b = interval.branch;
        lb = interval.lb;
        ub = interval.ub;
        p = accumulate(weights.begin(), weights.end(), 0.0);
        assert(!isnan(p));
        if (lb == max(cut_time, b.lower_node->time) and ub == b.upper_node->time) { // full intervals
            new_interval = new Interval(b, lb, ub, curr_index);
            new_interval->hmm_index = (int) curr_intervals.size();
            curr_intervals.push_back(new_interval);
            temp.push_back(p);
            if (weights.size() > 0) {
                source_weights[new_interval] = weights;
                source_intervals[new_interval] = intervals;
            }
        } else if (p >= cutoff) { // partial intervals
            new_interval = new Interval(b, lb, ub, curr_index);
            new_interval->hmm_index = (int) curr_intervals.size();
            curr_intervals.push_back(new_interval);
            temp.push_back(p);
            if (weights.size() > 0) {
                source_weights[new_interval] = weights;
                source_intervals[new_interval] = intervals;
            }
        }
    }
    forward_probs.push_back(temp);
    compute_interval_info();
    temp.clear();
    // temp_weights.clear();
    // temp_intervals.clear();
}

float BSP_smc::get_prop(float lb, float ub) {
    assert(lb <= ub);
    float x = get_prob(ub) - get_prob(lb);
    assert(!isnan(x));
    return x;
}

float BSP_smc::get_overwrite_prob(Recombination &r, float lb, float ub) {
    if (check_points.count(curr_index) > 0) {
        return 0.0;
    }
    float join_time = r.inserted_node->time;
    float p1 = get_prop(lb, ub);
    float p2 = get_prop(max(cut_time, r.start_time), join_time);
    if (p1 == 0 and p2 == 0) {
        return 1.0;
    }
    float overwrite_prob = p2/(p1 + p2);
    assert(!isnan(overwrite_prob));
    return overwrite_prob;
}

void BSP_smc::update_coalescence_times(Recombination &r) {
    float prev_time = r.deleted_node->time;
    float next_time = r.inserted_node->time;
    if (prev_time >= cut_time) {
        coalescence_times.erase(prev_time);
    }
    if (next_time >= cut_time) {
        coalescence_times.insert(next_time);
    }
}

void BSP_smc::calculate_coalescence_stats() {
    coalescence_probs.clear();
    coalescence_quantiles.clear();
    coalescence_rates.clear();
    vector<float> sorted_coalescence_times = vector<float>(coalescence_times.begin(), coalescence_times.end());
    int n = (int) sorted_coalescence_times.size();
    int k = n - 1;
    float prev_prob = 1.0;
    float next_prob = 1.0;
    float interval_prob = 0.0;
    float cum_prob = 0.0;
    coalescence_probs.insert({cut_time, 0});
    coalescence_quantiles.insert({0, cut_time});
    coalescence_rates.insert({cut_time, k});
    for (int i = 1; i < n; i++) {
        next_prob = prev_prob*exp(-k*(sorted_coalescence_times[i] - sorted_coalescence_times[i-1]));
        interval_prob = (prev_prob - next_prob)/k;
        cum_prob += interval_prob;
        coalescence_probs[sorted_coalescence_times[i]] = cum_prob;
        coalescence_quantiles[cum_prob] = sorted_coalescence_times[i];
        k -= 1;
        coalescence_rates[sorted_coalescence_times[i]] = k;
        prev_prob = next_prob;
    }
}

float BSP_smc::get_prob(float x) {
    if (coalescence_probs.count(x) > 0) {
        return coalescence_probs.at(x);
    }
    map<float, float>::iterator u_it = coalescence_probs.upper_bound(x);
    map<float, float>::iterator l_it = coalescence_probs.upper_bound(x);
    l_it--;
    int rate = coalescence_rates.at(l_it->first);
    float delta_t = u_it->first - l_it->first;
    float delta_p = u_it->second - l_it->second;
    float new_delta_t = x - l_it->first;
    float new_delta_p = delta_p*(1 - exp(-rate*new_delta_t))/(1 - exp(-rate*delta_t));
    return l_it->second + new_delta_p;
}

float BSP_smc::get_quantile(float p) {
    if (coalescence_quantiles.count(p) > 0) {
        return coalescence_quantiles.at(p);
    }
    map<float, float>::iterator u_it = coalescence_quantiles.upper_bound(p);
    map<float, float>::iterator l_it = coalescence_quantiles.upper_bound(p);
    l_it--;
    int rate = coalescence_rates.at(l_it->second);
    float delta_t = u_it->second - l_it->second;
    float delta_p = u_it->first - l_it->first;
    float new_delta_p = p - l_it->first;
    float new_delta_t = 1 - new_delta_p/delta_p*(1 - exp(-rate*delta_t));
    new_delta_t = -log(new_delta_t)/rate;
    assert(l_it->second + new_delta_t != numeric_limits<float>::infinity());
    return l_it->second + new_delta_t;
}

float BSP_smc::get_median(float lb, float ub) {
    float lq = get_prob(lb);
    float uq = get_prob(ub);
    float t;
    if (ub == numeric_limits<float>::infinity()) {
        return lb + log(2);
    }
    if (ub - lb < 1e-3 or uq - lq < 1e-3) {
        t = 0.5*(lb + ub);
    } else {
        t = get_quantile(0.5*(lq + uq));
    }
    assert(t >= lb and t <= ub);
    return t;
}

void BSP_smc::process_interval(Recombination &r, Interval *prev_interval) {
    Branch prev_branch = prev_interval->branch;
    if (prev_branch == r.source_branch) {
        process_source_interval(r, prev_interval);
    } else if (prev_branch == r.target_branch) {
        process_target_interval(r, prev_interval);
    } else {
        process_other_interval(r, prev_interval);
    }
}

void BSP_smc::process_source_interval(Recombination &r, Interval *prev_interval) {
    float w1, w2, lb, ub = 0;
    float p = forward_probs[curr_index - 1][prev_interval->hmm_index];
    float point_time = r.source_branch.upper_node->time;
    float break_time = r.start_time;
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
        w1 = get_prop(prev_interval->lb, break_time);
        w2 = get_prop(break_time, prev_interval->ub);
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

void BSP_smc::process_target_interval(Recombination &r, Interval *prev_interval) {
    float w0, w1, w2, lb, ub = 0;
    float p = forward_probs[curr_index - 1][prev_interval->hmm_index];
    float join_time = r.inserted_node->time;
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
        w1 = get_prop(prev_interval->lb, join_time);
        w2 = get_prop(join_time, prev_interval->ub);
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
        ub = r.recombined_branch.upper_node->time;;
        next_branch = r.recombined_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, w0*p);
    }
}

void BSP_smc::process_other_interval(Recombination &r, Interval *prev_interval) {
    float lb, ub = 0;
    float p = forward_probs[curr_index - 1][prev_interval->hmm_index];
    Branch prev_branch = prev_interval->branch;
    Branch next_branch;
    Interval_info next_interval;
    if (r.affect(prev_branch)) {
        lb = prev_interval->lb;
        ub = prev_interval->ub;
        next_branch = r.merging_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, p);
    } else {
        lb = prev_interval->lb;
        ub = prev_interval->ub;
        next_branch = prev_branch;
        next_interval = Interval_info(next_branch, lb, ub);
        transfer_helper(next_interval, prev_interval, p);
    }
}

float BSP_smc::random() {
    return (float) rand()/RAND_MAX;
}

int BSP_smc::get_prev_breakpoint(int x) {
    map<int, vector<Interval *>>::iterator state_it = state_spaces.upper_bound(x);
    state_it--;
    return state_it->first;
}

vector<Interval *> BSP_smc::get_state_space(int x) {
    map<int, vector<Interval *>>::iterator state_it = state_spaces.upper_bound(x);
    state_it--;
    return state_it->second;
}

void BSP_smc::simplify(map<float, Branch> &joining_branches) {
    map<float, Branch> simplified_joining_branches = {};
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

Interval *BSP_smc::sample_curr_interval(int x) {
    vector<Interval *> intervals = get_state_space(x);
    float ws = accumulate(forward_probs[x].begin(), forward_probs[x].end(), 0.0);
    float q = random();
    float w = ws*q;
    for (int i = 0; i < intervals.size(); i++) {
        w -= forward_probs[x][i];
        if (w <= 0) {
            sample_index = i;
            return intervals[i];
        }
    }
    cerr << "sampling failed" << endl;
    exit(1);
}

Interval *BSP_smc::sample_prev_interval(int x) {
    vector<Interval *> intervals = get_state_space(x);
    float rho = rhos[x];
    float ws = recomb_sums[x];
    float q = random();
    float w = ws*q;
    for (int i = 0; i < intervals.size(); i++) {
        w -= get_recomb_prob(rho, intervals[i]->time)*forward_probs[x][i];
        if (w <= 0) {
            sample_index = i;
            return intervals[i];
        }
    }
    cerr << "sampling failed" << endl;
    exit(1);
}

Interval *BSP_smc::sample_source_interval(Interval *interval) {
    assert(source_intervals.count(interval) > 0);
    vector<Interval *> intervals = source_intervals[interval];
    vector<float> weights = source_weights[interval];
    float q = random();
    float ws = accumulate(weights.begin(), weights.end(), 0.0);
    float w = ws*q;
    for (int i = 0; i < weights.size(); i++) {
        w -= weights[i];
        if (w <= 0) {
            sample_index = intervals[i]->hmm_index;
            return intervals[i];
        }
    }
    cerr << "sampling failed" << endl;
    exit(1);
}

int BSP_smc::trace_back_helper(Interval *interval, int x) {
    if (!interval->full(cut_time)) {
        return interval->start_pos;
    }
    float p = random();
    float q = 1;
    float shrinkage = 0;
    float recomb_prob = 0;
    float non_recomb_prob = 0;
    float all_prob = 0;
    while (x > interval->start_pos) {
        recomb_sum = recomb_sums[x - 1];
        weight_sum = weight_sums[x];
        recomb_prob = get_recomb_prob(rhos[x - 1], interval->time);
        non_recomb_prob = (1 - recomb_prob)*forward_probs[x - 1][sample_index];
        all_prob = non_recomb_prob + recomb_sum*interval->weight*recomb_prob/weight_sum;
        shrinkage = non_recomb_prob/all_prob;
        q *= shrinkage;
        if (p >= q) {
            return x;
        }
        x -= 1;
    }
    return interval->start_pos;
}
