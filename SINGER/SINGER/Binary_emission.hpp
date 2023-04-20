//
//  Binary_emission.hpp
//  SINGER
//
//  Created by Yun Deng on 4/6/23.
//

#ifndef Binary_emission_hpp
#define Binary_emission_hpp

#include <stdio.h>
#include <math.h>
#include "Emission.hpp"

using namespace std;

class Binary_emission : public Emission {
    
public:
    
    vector<float> diff = vector<float>(4);
    
    Binary_emission();
    
    ~Binary_emission();
    
    float null_emit(Branch branch, float time, float theta, Node *node) override;
    
    float mut_emit(Branch branch, float time, float theta, float bin_size, set<float> &mut_set, Node *node) override;
    
    float emit(Branch branch, float time, float theta, float bin_size, vector<float> &emissions, Node *node) override;
    
    float calculate_prob(float theta, float bin_size, float ll, float lu, float l0, int sl, int su, int s0);
    
    float calculate_prob(float theta, float bin_size, int s);
    
    void get_diff(set<float> &mut_set, Branch branch, Node *node);
};

#endif /* Binary_emission_hpp */