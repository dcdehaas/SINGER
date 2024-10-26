//
//  Coalescent_calculator.cpp
//  SINGER
//
//  Created by Yun Deng on 4/17/23.
//

#include "Coalescent_calculator.hpp"

Coalescent_calculator::Coalescent_calculator(double t) {
    cut_time = t;
}

Coalescent_calculator::~Coalescent_calculator() {}

void Coalescent_calculator::compute(set<Branch> &branches) {
    compute_rate_changes(branches);
    compute_rates();
    assert(rates.size() == rate_changes.size());
    compute_probs_quantiles();
    assert(rates.size() == rate_changes.size());
}

double Coalescent_calculator::weight(double lb, double ub) {
    double p = prob(ub) - prob(lb);
    assert(!isnan(p));
    return p;
}

double Coalescent_calculator::time(double lb, double ub) {
    double lq = prob(lb);
    double uq = prob(ub);
    double mid = 0;
    double t;
    if (isinf(ub)) {
        return lb + log(2);
    }
    if (ub - lb < 1e-3 or uq - lq < 1e-3) {
        t = 0.5*(lb + ub);
    } else {
        mid = 0.5*(lq + uq);
        t = quantile(mid);
    }
    assert(t >= lb and t <= ub);
    return t;
}

void Coalescent_calculator::compute_rate_changes(set<Branch> &branches) {
    rate_changes.clear();
    double lb = 0;
    double ub = 0;
    for (Branch b : branches) {
        lb = max(cut_time, b.lower_node->time);
        ub = b.upper_node->time;
        rate_changes[lb] += 1;
        rate_changes[ub] -= 1;
    }
    min_time = rate_changes.begin()->first;
    max_time = rate_changes.rbegin()->first;
}

void Coalescent_calculator::compute_rates() {
    rates.clear();
    int curr_rate = 0;
    for (auto &x : rate_changes) {
        curr_rate += x.second;
        rates[x.first] = curr_rate;
    }
    assert(rates.size() == rate_changes.size());
}

void Coalescent_calculator::compute_probs_quantiles() {
    probs.clear();
    quantiles.clear();
    int curr_rate = 0;
    double prev_time = 0.0;
    double next_time = 0.0;
    double prev_prob = 1.0;
    double next_prob = 1.0;
    double cum_prob = 0.0;
    for (auto it = rates.begin(); it != prev(rates.end()); it++) {
        curr_rate = it->second;
        prev_time = it->first;
        next_time = next(it)->first;
        if (curr_rate > 0) {
            next_prob = prev_prob*exp(-curr_rate*(next_time - prev_time));
            cum_prob += (prev_prob - next_prob)/curr_rate;
        } else {
            next_prob = prev_prob;
        }
        assert(cum_prob <= 1.0001);
        probs.insert({next_time, cum_prob});
        quantiles.insert({next_time, cum_prob});
        prev_prob = next_prob;
    }
    probs.insert({min_time, 0});
    quantiles.insert({min_time, 0});
    assert(probs.size() == rates.size());
    assert(quantiles.size() == rates.size());
}

double Coalescent_calculator::prob(double x) {
    if (x > max_time) {
        x = max_time;
    } else if (x < min_time) {
        x = min_time;
    }
    pair<double, double> query = {x, -1.0f};
    auto it = probs.find(query);
    if (it != probs.end()) {
        return it->second;
    }
    auto u_it = probs.upper_bound(query);
    auto l_it = probs.upper_bound(query);
    l_it--;
    double base_prob = l_it->second;
    int rate = rates[l_it->first];
    if (rate == 0) {
        return base_prob;
    }
    double delta_t = u_it->first - l_it->first;
    double delta_p = u_it->second - l_it->second;
    double new_delta_t = x - l_it->first;
    // double new_delta_p = delta_p*(1 - exp(-rate*new_delta_t))/(1 - exp(-rate*delta_t));
    double new_delta_p = delta_p*expm1(-rate*new_delta_t)/expm1(-rate*delta_t);
    assert(!isnan(new_delta_p));
    return base_prob + new_delta_p;
}

double Coalescent_calculator::quantile(double p) {
    pair<double, double> query = {-1.0f, p};
    auto u_it = quantiles.upper_bound(query);
    auto l_it = quantiles.upper_bound(query);
    l_it--;
    double base_time = l_it->first;
    int rate = rates[l_it->first];
    double delta_t = u_it->first - l_it->first;
    double delta_p = u_it->second - l_it->second;
    double new_delta_p = p - l_it->second;
    double new_delta_t = 1 - new_delta_p/delta_p*(1 - exp(-rate*delta_t));
    new_delta_t = -log(new_delta_t)/rate;
    assert(!isnan(new_delta_t));
    return base_time + new_delta_t;
}
