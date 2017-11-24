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
#include <map>
#include <vector>
#include <set>
#include <queue>
#include <inttypes.h>
#include <stdint.h>

#include <sylvan_bdd.h>

using namespace sylvan;

#define TERMINAL_LEVEL INT32_MAX
#define TERMINAL_FALSE (INT32_MAX-1)
#define TERMINAL_TRUE  (INT32_MAX-2)


#define BDD_COMPLEMENT (0x8000000000000000LL)
#define BDD_STRIPMARK(node) ((uint64_t)(node & (~BDD_COMPLEMENT)))
#define BDD_HASMARK(node) ((int)(node & 0x4000000000000000 ? 1 : 0))

typedef std::map< uint32_t, std::vector<Bdd> > LevelMap;



void DotToPs(std::string source, std::string destination, bool synchronous = true){
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-result"

    std::string command = "dot -Tps " + source + " -o " + destination + " 2>/dev/null 1>&2";
    if(synchronous)
        command.append(" &");

    system(command.c_str());
    #pragma GCC diagnostic pop
}

bool seen(std::set<BDD> &set, Bdd &node){
    BDD rawnode = BDD_STRIPMARK(node.GetBDD());
    if(set.insert(rawnode).second)
        return false;
    else return true;
}

LevelMap DetermineLevelMap(Bdd &bdd){
    LevelMap level;

    std::queue<Bdd> queue;
    std::set<BDD> set;

    queue.push(bdd);
    while(!queue.empty()){
        Bdd node = queue.front();
        queue.pop();

        if(seen(set,node))
            continue;

        if(node.isTerminal())
            level[TERMINAL_LEVEL].push_back(node);
        else {
            level[node.TopVar()].push_back(node);
            queue.push(node.Else());
            queue.push(node.Then());
        }
    }

    return std::move(level);
}

void FprintNodes(FILE *file, LevelMap &level){
    fprintf(file, "\n    // ==================== NODES ====================\n\n");

    for(auto it = level.begin(); it != level.end(); it++){
        std::vector<Bdd> &bdds = it->second;

        fprintf(file, "    {\n");
        fprintf(file, "        rank = same;\n");
        if(bdds[0].isTerminal()){
            fprintf(file, "        node [ shape=box, style=filled ];\n");
            fprintf(file, "        terminals [shape = plaintext, style = invis];\n");
            for(auto itbdds = bdds.begin(); itbdds != bdds.end(); itbdds++){
                Bdd &bdd = *itbdds;
                fprintf(file, "        %" PRIu64 " [label=\"%s\"];\n",
                        (uint64_t) (bdd.isOne()?TERMINAL_TRUE:TERMINAL_FALSE),
                        (bdd.isOne()?"T":"F"));
            }
        } else {
            uint32_t variable = bdds[0].TopVar();
            fprintf(file, "        node [ label=\"%" PRIu32 "\" ];\n",variable);
            fprintf(file, "        variable_%" PRIu32 " [shape = plaintext, label=\"%" PRIu32 "\" ];\n", variable,variable);
            for(auto itbdds = bdds.begin(); itbdds != bdds.end(); itbdds++){
                Bdd &bdd = *itbdds;
                fprintf(file, "        %" PRIu64 ";\n", (uint64_t) BDD_STRIPMARK(bdd.GetBDD()));
            }
        }
        fprintf(file, "    }\n\n");
    }
}

void FprintOrder(FILE *file, LevelMap &level){
    fprintf(file, "    // ==================== ORDER ====================\n\n");
    fprintf(file, "    { \n");
    fprintf(file, "        edge [style = invis];\n");

    //for(auto it = level.begin(); it != level.end(); it++){
    //    uint32_t variable = it->first;
    //    if(!(it->second[0].isTerminal()))
    //        fprintf(file, "        variable_%" PRIu32 " [ label=\"%" PRIu32 "\" ];\n", variable, variable);
    //}

    for(auto it = level.begin(); it != level.end(); it++){
        uint32_t variable = it->first;
        if(!(it->second[0].isTerminal()))
            fprintf(file, "        variable_%" PRIu32 " ->\n", variable);
    }
    fprintf(file, "        terminals;\n");
    fprintf(file, "    }\n\n");
}

void FprintEdges(FILE *file, LevelMap &level){
    fprintf(file, "\n    // ==================== EDGES ====================\n\n");
    fprintf(file, "    {\n");
    for(auto it = level.begin(); it != level.end(); it++){
        std::vector<Bdd> &bdds = it->second;
        if(!bdds[0].isTerminal()){
            for(auto itbdds = bdds.begin(); itbdds != bdds.end(); itbdds++){
                Bdd &bdd = *itbdds;

                // then edge
                fprintf(file, "        %" PRIu64 " -> %" PRIu64 " [style=solid dir=both arrowtail=%s];\n",
                    BDD_STRIPMARK(bdd.GetBDD()),
                    (bdd.Then().isTerminal()?TERMINAL_FALSE:BDD_STRIPMARK(bdd.Then().GetBDD())),
                    (BDD_HASMARK(bdd.Then().GetBDD()) ? "dot" : "none"));

                // else edge
                fprintf(file, "        %" PRIu64 " -> %" PRIu64 " [style=dashed];\n",
                    BDD_STRIPMARK(bdd.GetBDD()),
                    (bdd.Else().isTerminal()?TERMINAL_FALSE:BDD_STRIPMARK(bdd.Then().GetBDD())));
            }
        }
    }
    fprintf(file, "    }\n\n");
}

void FprintDot(FILE *file, Bdd bdd){
    LevelMap level = DetermineLevelMap(bdd);

    BDD root = bdd.GetBDD();
    fprintf(file, "digraph \"silvan\" {\n");
    fprintf(file, "    graph [dpi = 300];\n");
    fprintf(file, "    center = true;\n");
    fprintf(file, "    edge [dir = forward];\n");

    FprintNodes(file, level);

    FprintOrder(file, level);

    FprintEdges(file, level);

    fprintf(file, "    entry [style=invis];\n");
    if(bdd.isTerminal())
        fprintf(file, "    entry -> %" PRIu64 " [style=solid dir=both arrowtail=%s];\n",
            (uint64_t) TERMINAL_FALSE, BDD_HASMARK(root) ? "dot" : "none");
    else fprintf(file, "    entry -> %" PRIu64 " [style=solid dir=both arrowtail=%s];\n",
            BDD_STRIPMARK(root), BDD_HASMARK(root) ? "dot" : "none");



    fprintf(file, "}\n");
}

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
        FprintDot(file, bdd);
        fclose(file);

        DotToPs(filename, base + ".ps");
    } else fprintf(stderr,"Could not open dot file '%s'\n", filename.c_str());
}

//void Bdd::Dot(std::string filename){
//    size_t position = filename.find_last_of(".");
//    std::string extension = (std::string::npos == position)? std::string() : filename.substr(position+1);
//    std::string base = (std::string::npos == position)? filename : filename.substr(0,position);
//
//    if(extension != "dot"){
//        fprintf(stderr, "filename '%s' is not a dot file\n",filename.c_str());
//        return;
//    }
//
//    FILE *file = fopen(filename.c_str(), "w");
//    if(file){
//        PrintDot(file);
//        DotToPs(filename, base + ".ps");
//        fclose(file);
//    } else fprintf(stderr,"Could not open dot file '%s'\n", filename.c_str());
//}





