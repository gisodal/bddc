/*
 * Copyright 2017 Giso H. Dal, Radboud University Nijmegen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Do not include this file directly. Instead, include sylvan.h */
#include <sylvan_obj.hpp>
#include <stdio.h>
#include <stdlib.h>

using namespace sylvan;

void DotToPs(std::string source, std::string destination, bool synchronous = true){
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-result"

    std::string command = "dot -Tps " + source + " -o " + destination + " 2>/dev/null 1>&2";
    if(synchronous)
        command.append(" &");

    system(command.c_str());
    #pragma GCC diagnostic pop
}

//bool seen(std::set<BDD> &set, Bdd &node){
//    if(set.insert(node.GetBDD()).second)
//        return false;
//    else return true;
//}
//
//void WriteDot(File *file, Bdd bdd){
//
//    std::map< BDD,>
//    std::set<BDD> set;
//    std::queue<Bdd> queue;
//    queue.push(bdd);
//    while(!queue.empty()){
//        Bdd node = queue.front();
//        queue.pop();
//
//        if(seen(set,node))
//            continue;
//
//        if(node.isTerminal())
//            return true;
//
//        if
//        queue.push(node.Else());
//        queue.push(node.Then());
//        if(node::is_terminal(n))
//            level[0].push_back(n);
//        else level[n->l].push_back(n);
//
//        if(n->t) q.push(n->t);
//        if(n->e) q.push(n->e);
//    }
//
//
//    Cudd_Misc::Cudd_Queue q;
//    Cudd_Misc::Cudd_UniqueTable done;
//
//    fprintf(file, "    center = true;\n");
//    fprintf(file, "    edge [dir = none];\n");
//
//    fprintf(file, "\n    // =================== WEIGHTS ===================\n\n");
//    std::map< Cudd_DdHalfWord, std::vector<Cudd_DdNode*> > level;
//    done.insert(NULL);
//    if(n){
//        q.push(n);
//        while(!q.empty()){
//            Cudd_DdNode *n = q.front();
//            q.pop();
//
//            if(done.find(n) == done.end()){
//                done.insert(n);
//                literal_t l = n->index;
//                level[l].push_back(n);
//                if(!Cudd_IsTerminal(n)){
//                    q.push(Cudd_E(n));
//                    q.push(Cudd_T(n));
//                    if(g.defined() && g.is_probability(l))
//                        fprintf(file, "    %lu [label=\"W%u(%.2f)\"];\n", (size_t) n, l, g.get_probability(l));
//                }
//            }
//        }
//    }
//
//
//
//
//
//
//
//
//
//
//    std::queue<Bdd> q;
//    std::map<Bdd*,bool> done;
//        fprintf(file, "digraph %s {\n", "WPBDD");
//
//
//        fprintf(file, "    center = true;\n");
//        fprintf(file, "    edge [dir = none];\n");
//
//        std::map< unsigned int, std::vector<node*> > level;
//        if(n){
//            q.push(n);
//            while(!q.empty()){
//                node *n = q.front();
//                q.pop();
//
//                if(!done[n]){
//                    done[n] = true;
//                    if(node::is_terminal(n))
//                        level[0].push_back(n);
//                    else level[n->l].push_back(n);
//
//                    if(n->t) q.push(n->t);
//                    if(n->e) q.push(n->e);
//                }
//            }
//        }
//
//        #ifdef DEBUG
//        // ==== build numeric index for each node ====
//        // NOTE: index is assigned according to pre-order
//        std::unordered_map<node*,unsigned int> index;
//        node::stack s;
//        s.push(n);
//        unsigned int terminals = 0;
//        while(!s.empty()){
//            node *n = s.top();
//            s.pop();
//
//            if(n && index.find(n) == index.end()){
//                if(node::is_terminal(n)){
//                    index[n] = node::is_satisfiable(n);
//                    terminals++;
//                } else {
//                    index[n] = index.size()+1-terminals;
//                    s.push(n->e);
//                    s.push(n->t);
//                }
//            }
//        }
//        #endif
//
//
//
//        #ifdef VERBOSE
//        const bool add_literals = true;
//        #else
//        const bool add_literals = false;
//        #endif
//
//        fprintf(file, "\n    // ==================== NODES ====================\n\n");
//        for(auto it = level.begin(); it != level.end(); it++){
//            if(it->second.size() > 0){
//                if(it->first == 0){
//                    fprintf(file, "    {   // TERMINALS\n");
//                    fprintf(file, "        rank = same;\n");
//                    fprintf(file, "        node [shape=box];\n");
//                    if(add_literals)
//                        fprintf(file, "        \"TERMINALS\";\n");
//                    for(auto itn = it->second.begin(); itn != it->second.end(); itn++){
//                        node *n = *itn;
//                        if(node::is_terminal(n)){
//                            if(node::is_satisfiable(n)){
//                                #ifdef DEBUG
//                                //fprintf(file, "        %lu [label=\"T (%d:%x)\"", n, n->ref, n);
//                                fprintf(file, "        %lu [label=\"%u:%p\\nl:%d, ref:%u\\nT\"", (size_t) n, index[n], n, n->l, n->ref);
//                                #else
//                                fprintf(file, "        %lu [label=\"T\"", (size_t) n);
//                                #endif
//                                if(current && n == current)
//                                    fprintf(file, ",fontcolor=\"red\",color=\"red\"");
//                                fprintf(file, "];\n");
//                            } else {
//                                #ifdef DEBUG
//                                //fprintf(file, "        %lu [label=\"F (%d:%x)\"", n, n->ref, n);
//                                fprintf(file, "        %lu [label=\"%u:%p\\nl:%d, ref:%u\\nF\"", (size_t) n, index[n], n, n->l, n->ref);
//                                #else
//                                fprintf(file, "        %lu [label=\"F\"", (size_t) n);
//                                #endif
//                                if(current && n == current)
//                                    fprintf(file, ",fontcolor=\"red\",color=\"red\"");
//                                fprintf(file, "];\n");
//                            }
//                        }
//                    }
//                    fprintf(file, "    }\n");
//                } else {
//                    literal_t l = it->first;
//                    if(g.defined()){
//                        unsigned int variable = g.get_literal_to_variable()[l];
//                        unsigned int value = l - g.get_variable_to_literal()[variable];
//                        std::string node_name = g.get_bayesnet()->get_node_name(variable);
//                        std::string value_name = g.get_bayesnet()->get_node_value_name(variable,value);
//                        fprintf(file, "    {   // literal %d (%s=%s)\n", l, node_name.c_str(), value_name.c_str());
//                        fprintf(file, "        rank = same;\n");
//                        fprintf(file, "        node [ label = \"%s=%s\" ]\n", node_name.c_str(), value_name.c_str());
//                        if(add_literals)
//                            fprintf(file, "        literal_%d [shape = plaintext];\n", l);
//                        for(auto itn = it->second.begin(); itn != it->second.end(); itn++){
//                            node* n = *itn;
//                            fprintf(file, "        %lu", (size_t) n);
//                            #ifdef DEBUG
//                            //fprintf(file, " [label=\"%s=%s (%d:%x)\"]", node_name.c_str(), value_name.c_str(), n->ref, n);
//                            fprintf(file, " [label=\"%u:%p\\nl:%d, ref:%u\\n%s=%s\"]", index[n], n, n->l, n->ref, node_name.c_str(), value_name.c_str());
//                            #endif
//                            if(current && n == current)
//                                fprintf(file, " [fontcolor=\"red\",color=\"red\"]");
//                            fprintf(file, ";\n");
//                        }
//                        fprintf(file, "    }\n");
//                    } else {
//                        fprintf(file, "    {   // literal %d\n", l);
//                        fprintf(file, "        rank = same;\n");
//                        if(add_literals)
//                           fprintf(file, "        literal_%d [shape = plaintext];\n", l, l);
//                        for(auto itn = it->second.begin(); itn != it->second.end(); itn++){
//                            fprintf(file, "        %lu", (size_t) n);
//                            #ifdef DEBUG
//                            fprintf(file, " [label=\"%u\" (%d:%x)\"]", (size_t) n, l, n->ref, n);
//                            #endif
//                            if(current && n == current)
//                                fprintf(file, "[fontcolor=\"red\",color=\"red\"]");
//                            fprintf(file, ";\n");
//                        }
//                        fprintf(file, "    }\n");
//                    }
//
//                }
//            }
//        }
//        // print labels on left side
//        if(add_literals){
//            fprintf(file, "\n    // ==================== ORDER ====================\n\n");
//            fprintf(file, "    { \n");
//            fprintf(file, "        node [shape = plaintext];\n");
//            fprintf(file, "        edge [style = invis];\n");
//            fprintf(file, "        \"TERMINALS\" [style = invis];\n\n");
//            const ordering_t &ordering = manager->get_ordering(id);
//            for(unsigned int i = 0; i < ordering.size(); i++){
//                literal_t l = ordering[i];
//                unsigned int variable = g.get_literal_to_variable()[l];
//                unsigned int value = l - g.get_variable_to_literal()[variable];
//                std::string node_name = g.get_bayesnet()->get_node_name(variable);
//                std::string value_name = g.get_bayesnet()->get_node_value_name(variable,value);
//                if(level[l].size() > 0)
//                    fprintf(file, "        literal_%d [ label = \" Literal %d: %s=%s \\l\"];\n", l, l, node_name.c_str(), value_name.c_str());
//            }
//
//            for(unsigned int i = 0; i < ordering.size(); i++){
//                literal_t l = ordering[i];
//                if(level[l].size() > 0)
//                    fprintf(file, "        literal_%d ->\n", l);
//                // if(level[l].size() == 0){
//                // } else fprintf(file, "        \"(Literal %d)\" ->\n", l);
//            }
//            fprintf(file, "        \"TERMINALS\";\n");
//            fprintf(file, "    }\n");
//        }
//        fprintf(file, "\n    // ==================== EDGES ====================\n\n");
//        done.clear();
//        if(n){
//            q.push(n);
//            while(!q.empty()){
//                node *n = q.front();
//                q.pop();
//
//                if(!done[n]){
//                    done[n] = true;
//                    if(n->t){
//                        q.push(n->t);
//                        fprintf(file, "    %lu -> %lu", (size_t) n, (size_t) n->t);
//                        if(n->W){
//                            fprintf(file, " [fontsize=8,label=\"");
//                            for(auto it = n->W->begin(); it != n->W->end(); it++){
//                                weight_t w = *it;
//
//                                if(it != n->W->begin())
//                                    fprintf(file, " * ");
//
//                                if(g.defined()){
//                                    if(OPT_DETERMINISM && (w == 1 || w == 0))
//                                        fprintf(file, "W%u(%.2f)", w, w);
//                                    else
//                                        fprintf(file, "W%u(%.2f)", w, g.get_weight_to_probability()[w-(g.get_nr_literals()+1)]);
//                                }
//                                else
//                                    fprintf(file, "W%u", w);
//                            }
//                            fprintf(file, "\"]");
//                        }
//                        fprintf(file, ";\n");
//                    }
//                    if(n->e){
//                        q.push(n->e);
//                        fprintf(file, "    %lu -> %lu [style=dotted];\n", (size_t) n, (size_t) n->e);
//                    }
//                }
//            }
//        }
//
//        fprintf(file, "}\n");
//        fclose(file);
//    } else throw compiler_write_exception("Could not open dot file '%s'",files.get_filename_c(DOT,aux));
//}
//
//void Cudd_CustomDumpDot(BDD bdd){
//
//
//
//    Cudd_Misc::Cudd_Queue q;
//    Cudd_Misc::Cudd_UniqueTable done;
//
//    fprintf(file, "    center = true;\n");
//    fprintf(file, "    edge [dir = none];\n");
//
//    fprintf(file, "\n    // =================== WEIGHTS ===================\n\n");
//    std::map< Cudd_DdHalfWord, std::vector<Cudd_DdNode*> > level;
//    done.insert(NULL);
//    if(n){
//        q.push(n);
//        while(!q.empty()){
//            Cudd_DdNode *n = q.front();
//            q.pop();
//
//            if(done.find(n) == done.end()){
//                done.insert(n);
//                literal_t l = n->index;
//                level[l].push_back(n);
//                if(!Cudd_IsTerminal(n)){
//                    q.push(Cudd_E(n));
//                    q.push(Cudd_T(n));
//                    if(g.defined() && g.is_probability(l))
//                        fprintf(file, "    %lu [label=\"W%u(%.2f)\"];\n", (size_t) n, l, g.get_probability(l));
//                }
//            }
//        }
//    }
//
//    fprintf(file, "\n    // ==================== VARIABLES ====================\n\n");
//    for(auto it = level.begin(); it != level.end(); it++){
//        if(it->second.size() > 0){
//
//            Cudd_DdNode *n = *(it->second.begin());
//            if(!Cudd_IsTerminal(n)){
//                literal_t l = n->index;
//                if(g.defined() && !g.is_probability(l)){
//                    fprintf(file, "    {\n");
//                    fprintf(file, "        rank = same;\n");
//                    if(!g.is_probability(l)){
//                        unsigned int variable = g.get_literal_to_variable()[l];
//                        unsigned int value = l - g.get_variable_to_literal()[variable];
//                        std::string node_name = g.get_bayesnet()->get_node_name(variable);
//                        std::string value_name = g.get_bayesnet()->get_node_value_name(variable,value);
//                        fprintf(file, "        node [label=\"%s=%s\"];\n", node_name.c_str(), value_name.c_str());
//                    } else fprintf(file, "        node [label=\"%d\"];\n", l);
//
//                    for(auto itn = it->second.begin(); itn != it->second.end(); itn++){
//                        Cudd_DdNode *n = *itn;
//                        fprintf(file, "        %lu;\n", (size_t) n);
//                    }
//                    fprintf(file, "    }\n");
//                }
//            } else {
//                if(it->second.size() > 1){
//                    fprintf(file, "    {\n");
//                    fprintf(file, "        rank = same;\n");
//                }
//                for(auto itn = it->second.begin(); itn != it->second.end(); itn++){
//                    Cudd_DdNode *n = *itn;
//                    if(Cudd_IsTrue(n))
//                        fprintf(file, "        %lu [label=\"T\",shape=box];\n", (size_t) n);
//                    else fprintf(file, "        %lu [label=\"F\",shape=box];\n", (size_t) n);
//                }
//                if(it->second.size() > 1)
//                    fprintf(file, "    }\n");
//            }
//        }
//    }
//
//    fprintf(file, "\n    // ==================== EDGES ====================\n\n");
//    done.clear();
//    done.insert(NULL);
//    if(n){
//        q.push(n);
//        while(!q.empty()){
//            Cudd_DdNode *n = q.front();
//            q.pop();
//
//            if(done.find(n) == done.end()){
//                done.insert(n);
//                if(!Cudd_IsTerminal(n)){
//                    q.push(Cudd_E(n));
//                    q.push(Cudd_T(n));
//                    fprintf(file, "    %lu -> %lu [style=dotted];\n", (size_t) n, (size_t) Cudd_E(n));
//                    fprintf(file, "    %lu -> %lu;\n", (size_t) n, (size_t) Cudd_T(n));
//                }
//            }
//        }
//    }
//}
//
//void Bdd::Dot(std::string filename){
//    FILE *file = fopen(filename.c_str(), "w");
//    if(file){
//        fprintf(file, "digraph %s {\n", "sylvan-obdd");
//
//        WriteDot(File, (*this))
//
//        fprintf(file, "}\n");
//        fclose(file);
//    } else throw compiler_write_exception("Could not open dot file '%s'\n", files.get_filename_c(DOT));
//}
//

void Bdd::Dot(std::string filename){
    size_t position = filename.find_last_of(".");
    std::string extension = (std::string::npos == position)? std::string() : filename.substr(position+1);
    std::string base = (std::string::npos == position)? filename : filename.substr(0,position);

    if(extension != "dot"){
        fprintf(stderr, "filename '%s' is not a dot file\n",filename.c_str());
        return;
    }

    FILE *file = fopen(filename.c_str(), "w");
    if(file){
        PrintDot(file);
        DotToPs(filename, base + ".ps");
        fclose(file);
    } else fprintf(stderr,"Could not open dot file '%s'\n", filename.c_str());
}





