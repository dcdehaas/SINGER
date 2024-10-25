//
//  Node.hpp
//  SINGER
//
//  Created by Yun Deng on 3/31/22.
//

#ifndef Node_hpp
#define Node_hpp

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <cassert>
#include <climits>
#include <algorithm>
#include <cmath>
#include <memory>
using namespace std;

class Node {
    
public:
    map<double, double> mutation_sites = {{-1, 0}, {INT_MAX, 0}};
    map<double, double>::iterator it = next(mutation_sites.begin());
    
    int index = 0;
    
    double time = 0;
    
    Node(double t);
    
    void set_index(int index);
    
    void print();
    
    void add_mutation(double pos);
    
    double get_state(double pos);
    
    void write_state(double pos, double s);
    
    void read_mutation(string filename);
    
    void move_iterator(double m);

/*
public:
    
    unordered_set<double> mutation_sites = {};
    // unordered_set<double> ambiguous_sites = {};
    
    int index = 0;
    
    double time = 0;
    
    Node(double t);
    
    void set_index(int index);
    
    void print();
    
    void add_mutation(double pos);
    
    double get_state(double pos);
    
    void write_state(double pos, double s);
    
    void read_mutation(string filename);
 */
};

shared_ptr<Node> new_node(double t);

struct compare_node {
    
    bool operator() (const shared_ptr<Node> n1, const shared_ptr<Node> n2) const {
        if (n1->time != n2->time) {
            return n1->time < n2->time;
        } else if (n1->index != n2->index) {
            return n1->index < n2->index;
        } else {
            if (n1 != n2) {
                cerr << "bad comparison" << endl;
                exit(1);
            }
            return n1 < n2;
        }
    }
};

#endif /* Node_hpp */
