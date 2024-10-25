//
//  Node.cpp
//  SINGER
//
//  Created by Yun Deng on 3/31/22.
//

#include "Node.hpp"

Node::Node(double t) {
    time = t;
}

void Node::set_index(int index) {
    this->index = index;
}

void Node::add_mutation(double pos) {
    mutation_sites[pos] = 1;
}
 
double Node::get_state(double pos) {
    move_iterator(pos);
    if (it->first == pos) {
        return it->second;
    } else {
        return 0;
    }
    return 0;
}

void Node::write_state(double pos, double s) {
    if (s == 0) {
        mutation_sites.erase(pos);
        if (it->first == pos) {
            it = mutation_sites.begin();
        }
        return;
    } else if (s == 1) {
        mutation_sites[pos] = s;
    }
    return;
}

void Node::read_mutation(string filename) {
    ifstream fin(filename);
    if (!fin.good()) {
        cerr << "input file not found" << endl;
        exit(1);
    }
    double x;
    while (fin >> x) {
        add_mutation(x);
    }
}

shared_ptr<Node> new_node(double t) {
    return make_shared<Node>(t);
}

void Node::move_iterator(double m) {
    if (it->first == m) {
        return;
    }
    double next_pos = next(it)->first;
    double prev_pos = prev(it)->first;
    if (it->first < m) {
        if (next_pos == m) {
            ++it;
            return;
        } else if (next_pos > m) {
            return;
        }
    } else if (it->first > m and prev_pos <= m) {
        --it;
        return;
    }
    if (abs(it->first - m) < 20) {
        while (it != mutation_sites.begin() and it->first > m) {
            --it;
        }
        while (next(it) != mutation_sites.end() and next(it)->first <= m) {
            ++it;
        }
    } else {
        it = mutation_sites.upper_bound(m);
        --it;
    }
    assert(it->first <= m and next(it)->first > m);
}

/*
Node::Node(double t) {
    time = t;
}

void Node::set_index(int index) {
    this->index = index;
}

void Node::add_mutation(double pos) {
    mutation_sites.insert(pos);
}

double Node::get_state(double pos) {
    if (mutation_sites.count(pos) > 0) {
        return 1;
    } else {
        return 0;
    }
}

void Node::write_state(double pos, double s) {
    if (s == 0) {
        mutation_sites.erase(pos);
        // ambiguous_sites.erase(pos);
        return;
    } else if (s == 1) {
        // ambiguous_sites.erase(pos);
        mutation_sites.insert(pos);
    }
    return;
}

void Node::read_mutation(string filename) {
    ifstream fin(filename);
    if (!fin.good()) {
        cerr << "input file not found" << endl;
        exit(1);
    }
    double x;
    while (fin >> x) {
        add_mutation(x);
    }
}

shared_ptr<Node> new_node(double t) {
    return make_shared<Node>(t);
}
*/
