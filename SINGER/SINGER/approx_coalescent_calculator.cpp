//
//  approx_coalescent_calculator.cpp
//  SINGER
//
//  Created by Yun Deng on 6/25/23.
//

#include "approx_coalescent_calculator.hpp"

approx_coalescent_calculator::approx_coalescent_calculator(float t) {
    cut_time = t;
}

approx_coalescent_calculator::~approx_coalescent_calculator() {}

void approx_coalescent_calculator::start(set<Branch> &branches) {
    for (auto &x : branches) {
        if (x.upper_node->time > cut_time and x.lower_node->time <= cut_time) {
            n0 += 1;
        }
    }
}

void approx_coalescent_calculator::update(Recombination &r) {
    if (r.deleted_node->time > cut_time) {
        n0 -= 1;
    }
    if (r.inserted_node->time > cut_time) {
        n0 += 1;
    }
}

pair<float, float> approx_coalescent_calculator::compute_time_weights(float x, float y) {
    if (x == y) { // no need to compute when point mass
        return {x, 0};
    }
    float t, w, p, q;
    p = prob(x, y);
    t = find_median(x, y);
    q = p*(t - cut_time);
    w = q*n0/2;
    if (y - x < 0.001) {
        t = 0.5*(x + y);
    }
    assert(t >= x and t <= y);
    assert(q >= 0);
    return {t, w};
}

void approx_coalescent_calculator::compute_first_moment() {
    first_moment = 2.0/n0;
}

float approx_coalescent_calculator::prob(float x, float y) {
    float p1 = prob_integral(x);
    float p2 = prob_integral(y);
    assert(p2 >= p1 - 0.001);
    return max(p2 - p1, 0.0f);
}

float approx_coalescent_calculator::prob_integral(float x) {
    x -= cut_time;
    if (n0 == 1) {
        return 1 - exp(-x);
    }
    float v = n0 + (1 - n0)*exp(-0.5*x);
    float p = -2*log(v) -2*n0/v;
    p = p/(1 - n0)/(1 - n0);
    assert(!isnan(p));
    return p;
}

/*
float approx_coalescent_calculator::find_median(float x, float y) {
    if (x == y) {
        return x;
    }
    if (y - x <= 0.01) {
        return 0.5*(x + y);
    }
    float epsilon = 1e-3;
    float mv = 0.5*(prob_integral(x) + prob_integral(y));
    float l = x, u = y;
    float z = 0, m = 0;
    while (abs(l - u) > epsilon) {
        if (isinf(u)) {
            z = l + 1;
        } else {
            z = 0.5*(l + u);
        }
        m = prob_integral(z);
        if (m < mv) {
            l = z;
        } else {
            u = z;
        }
    }
    return z;
}
*/

float approx_coalescent_calculator::find_median(float x, float y) {
    if (x == y) {
        return x;
    }
    if (y - x <= 0.01) {
        return 0.5*(x + y);
    }
    if (n0 == 1) {
        float q = 0.5*(exp(-x) + exp(-y));
        float t = -log(q);
        assert(t >= x and t <= y);
        return t;
    }
    x -= cut_time;
    y -= cut_time;
    float z = 0;
    float nx = n0 + (1 - n0)*exp(-0.5*x);
    float ny = n0 + (1 - n0)*exp(-0.5*y);
    float nm = sqrt(nx*ny);
    z = 2*log(n0 - 1) - 2*log(n0 - nm);
    assert(z >= x and z <= y);
    z += cut_time;
    return z;
}
