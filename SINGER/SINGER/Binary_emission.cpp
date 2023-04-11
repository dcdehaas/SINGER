//
//  Binary_emission.cpp
//  SINGER
//
//  Created by Yun Deng on 4/6/23.
//

#include "Binary_emission.hpp"

Binary_emission::Binary_emission() {}

Binary_emission::~Binary_emission() {}

float Binary_emission::null_emit(Branch branch, float time, float theta, Node *node) {
    float emit_prob = 1;
    float old_prob = 1;
    float ll = time - branch.lower_node->time;
    float lu = branch.upper_node->time - time;
    float l0 = time - node->time;
    emit_prob = calculate_prob(theta, 1, ll, lu, l0, 0, 0, 0);
    old_prob = calculate_prob(theta*(ll + lu), 1, 0);
    emit_prob /= old_prob;
    return emit_prob;
}

float Binary_emission::mut_emit(Branch branch, float time, float theta, float bin_size, set<float> mut_set, Node *node) {
    float emit_prob = 1;
    float old_prob = 1;
    float ll = time - branch.lower_node->time;
    float lu = branch.upper_node->time - time;
    float l0 = time - node->time;
    vector<int> diff = get_diff(mut_set, branch, node);
    emit_prob = calculate_prob(theta, bin_size, ll, lu, l0, diff[0], diff[1], diff[2]);
    old_prob = calculate_prob(theta*(ll + lu), bin_size, diff[3]);
    
    emit_prob /= old_prob;
    return emit_prob;
}

float Binary_emission::calculate_prob(float theta, float bin_size, float ll, float lu, float l0, int sl, int su, int s0) {
    float prob = 1;
    prob *= calculate_prob(ll*theta, bin_size, sl);
    prob *= calculate_prob(lu*theta, bin_size, su);
    prob *= calculate_prob(l0*theta, bin_size, s0);
    return prob;
}

float Binary_emission::calculate_prob(float theta, float bin_size, int s) {
    if (isinf(theta)) {
        return 1.0;
    }
    return exp(-theta)*pow(theta/bin_size, s);
}

vector<int> Binary_emission::get_diff(set<float> mut_set, Branch branch, Node *node) {
    vector<int> diff(4);
    float sl = 0;
    float su = 0;
    float s0 = 0;
    float sm = 0;
    for (float x : mut_set) {
        sl = branch.lower_node->get_state(x);
        su = branch.upper_node->get_state(x);
        s0 = node->get_state(x);
        if (sl + su + s0 > 1.5) {
            sm = 1;
        } else {
            sm = 0;
        }
        diff[0] += abs(sm - sl);
        diff[1] += abs(sm - su);
        diff[2] += abs(sm - s0);
        diff[3] += abs(sl - su);
    }
    return diff;
}
