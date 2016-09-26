/*
Aho-Corasick keyword tree + automaton implementation for Python.
Copyright (C) 2016 Timo Petmanson ( tpetmanson@gmail.com )

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include "automaton.h"
#include "match.h"
#include "node.h"

#include <iostream>
#include <algorithm>
#include <deque>

BEGIN_NAMESPACE(ac)

Automaton::Automaton() : uptodate(false) {
    root = std::make_shared<Node>(0, -1);
    nodes.push_back(root);
}

void Automaton::add(const StringVector& pattern, const std::string& value) {
    NodePtr node = root;
    NodePtr outnode;
    const std::string* elem;
    for (int depth=0 ; depth<pattern.size() ; ++depth) {
        elem = &pattern[depth];
        alphabet.insert(*elem);
        outnode = node->get_outnode(*elem);
        if (outnode) {
            node = outnode;
        } else {
            NodePtr newnode = std::make_shared<Node>(nodes.size(), depth);
            node->set_outnode(*elem, newnode);
            nodes.push_back(newnode);
            node = newnode;
        }
    }
    node->set_value(value);
    node->add_match(node);
    uptodate = false;
}

NodePtr Automaton::find_node(const StringVector& prefix) const {
    NodePtr node = root;
    NodePtr outnode;
    for (int idx=0 ; idx<prefix.size() ; ++idx) {
        outnode = node->get_outnode(prefix[idx]);
        if (outnode) {
            node = outnode;
        } else {
            return NodePtr();
        }
    }
    return node;
}

bool Automaton::has_pattern(const StringVector& pattern) const {
    NodePtr node = find_node(pattern);
    return node && node->get_value() != "";
}

bool Automaton::has_prefix(const StringVector& prefix) const {
    NodePtr node = find_node(prefix);
    return !node;
}

std::string Automaton::get_value(const StringVector& pattern) const {
    NodePtr node = find_node(pattern);
    return node ? node->get_value() : std::string("");
}

NodePtr Automaton::goto_node(const int node_id, const std::string& elem) {
    NodePtr node = this->nodes[node_id];
    auto iter = node->outs.find(elem);
    if (iter != node->outs.end()) {
        return iter->second;
    } else if (node_id == 0) {
        return this->root;
    }
    return std::shared_ptr<Node>(NULL);
}

void Automaton::update_automaton() {
    IntVector fail_table(nodes.size(), 0);
    NodePtr root = this->root;
    std::deque<int> Q;
    for (auto iter = root->outs.begin() ; iter != root->outs.end() ; ++iter) {
        Q.push_back(iter->second->node_id);
    }
    while (Q.size() > 0) {
        int node_id = Q[0]; Q.pop_front();
        NodePtr node = nodes[node_id];
        #ifdef AC_DEBUG
            std::cout << "processing node " << node_id << " value " << node->value << "\n";
        #endif
        for (auto iter=node->outs.begin() ; iter != node->outs.end() ; ++iter) {
            auto dest_node = iter->second;
            #ifdef AC_DEBUG
                std::cout << "    dest node id " << dest_node->node_id << " with key " << iter->first << "\n";
            #endif
            Q.push_back(dest_node->node_id);
            int fail_node_id = fail_table[node_id];
            while (goto_node(fail_node_id, iter->first) == NULL) {
                fail_node_id = fail_table[fail_node_id];
            }
            fail_table[dest_node->node_id] = goto_node(fail_node_id, iter->first)->node_id;
            // copy the matches
            NodePtr fail_node = nodes[fail_table[dest_node->node_id]];
            dest_node->matches.reserve(dest_node->matches.size() + fail_node->matches.size());
            std::copy(fail_node->matches.begin(), fail_node->matches.end(), std::back_inserter(dest_node->matches));
        }
    }
    this->fail_table = fail_table;
    this->uptodate = true;
}

MatchVector Automaton::get_matches(const StringVector& text, const bool exclude_overlaps) {
    MatchVector matches;
    if (!this->uptodate) {
        this->update_automaton();
    }
    int node_id = this->root->node_id;
    for (int idx=0 ; idx<text.size() ; ++idx) {
        while (goto_node(node_id, text[idx]) == NULL) {
            node_id = this->fail_table[node_id]; // follow fail
        }
        NodePtr node = goto_node(node_id, text[idx]);
        node_id = node->node_id;
        #ifdef AC_DEBUG
            std::cout << "matching pos " << idx << " " << text[idx] << " with node " << node->node_id << " value " << node->value << "\n";
        #endif
        if (node->value != "") {
            for (NodePtr resnode : node->matches) {
                matches.push_back(Match(idx - resnode->depth, idx+1, resnode->value));
            }
        }
    }
    if (exclude_overlaps) {
        return remove_overlaps(matches);
    }
    return matches;
}

std::string Automaton::str() const {
    return root->str();
}

END_NAMESPACE
