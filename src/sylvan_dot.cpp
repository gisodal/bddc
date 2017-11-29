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
#define TERMINAL_FALSE (INT64_MAX-1)
#define TERMINAL_TRUE  (INT64_MAX-2)

typedef std::map< uint32_t, std::vector<Bdd> > LevelMap;

BDD GetId(Bdd bdd){
    if(bdd.isTerminal()){
        if(bdd.isOne())
            return TERMINAL_TRUE;
        else return TERMINAL_FALSE;
    } else return bdd.GetBDD();
}

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
    BDD rawnode = node.GetBDD();
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

void FprintNodes(FILE *file, const LevelMap &level, const std::vector< std::string > &names){
    fprintf(file, "\n    // ==================== NODES ====================\n\n");

    for(auto it = level.begin(); it != level.end(); it++){
        const std::vector<Bdd> &bdds = it->second;

        fprintf(file, "    {\n");
        fprintf(file, "        rank = same;\n");
        if(bdds[0].isTerminal()){
            fprintf(file, "        node [ shape=box, style=filled ];\n");
            fprintf(file, "        terminals [shape = plaintext, style = invis];\n");
            for(auto itbdds = bdds.begin(); itbdds != bdds.end(); itbdds++){
                const Bdd &bdd = *itbdds;
                fprintf(file, "        %" PRIu64 " [label=\"%s\"];\n",
                        GetId(bdd),
                        (bdd.isOne()?"T":"F"));
            }
        } else {
            uint32_t variable = bdds[0].TopVar();
            if(names.empty())
                fprintf(file, "        node [ label=\"%" PRIu32 "\" ];\n",variable);
            else fprintf(file, "        node [ label=\"%s\" ];\n", names[variable].c_str());

            fprintf(file, "        variable_%" PRIu32 " [shape = plaintext, label=\"%" PRIu32 "\" ];\n", variable,variable);
            for(auto itbdds = bdds.begin(); itbdds != bdds.end(); itbdds++){
                const Bdd &bdd = *itbdds;
                fprintf(file, "        %" PRIu64 ";\n", (uint64_t) bdd.GetBDD());
            }
        }
        fprintf(file, "    }\n\n");
    }
}

void FprintOrder(FILE *file, LevelMap &level){
    fprintf(file, "    // ==================== ORDER ====================\n\n");
    fprintf(file, "    { \n");
    fprintf(file, "        edge [style = invis];\n");

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
                fprintf(file, "        %" PRIu64 " -> %" PRIu64 " [style=solid dir=both arrowtail=none];\n", // arrowtail="dot" | "none"
                    GetId(bdd),
                    GetId(bdd.Then()));

                // else edge
                fprintf(file, "        %" PRIu64 " -> %" PRIu64 " [style=dashed];\n",
                    GetId(bdd),
                    GetId(bdd.Else()));
            }
        }
    }
    fprintf(file, "    }\n\n");
}

void FprintDot(FILE *file, Bdd bdd, const std::vector< std::string > &names){
    LevelMap level = DetermineLevelMap(bdd);

    fprintf(file, "digraph \"bddc\" {\n");
    // fprintf(file, "    graph [dpi = 300];\n");
    fprintf(file, "    center = true;\n");
    fprintf(file, "    edge [dir = forward];\n");

    FprintNodes(file, level, names);

    FprintOrder(file, level);

    FprintEdges(file, level);

    fprintf(file, "    entry [style=invis];\n");
    fprintf(file, "    entry -> %" PRIu64 " [style=solid dir=both arrowtail=%s];\n", GetId(bdd),"none");
    fprintf(file, "}\n");
}

void Bdd::Dot(std::string filename, const bool kCreatePs) const {
    std::vector< std::string > names;
    Dot(filename, names, kCreatePs);
}

void Bdd::Dot(std::string filename, const std::vector< std::string > &names, const bool kCreatePs) const {
    size_t position = filename.find_last_of(".");
    std::string extension = (std::string::npos == position)? std::string() : filename.substr(position+1);
    std::string base = (std::string::npos == position)? filename : filename.substr(0,position);

    if(extension != "dot"){
        fprintf(stderr, "filename '%s' is not a dot file\n",filename.c_str());
        return;
    }

    FILE *file = fopen(filename.c_str(), "w");
    if(file){
        FprintDot(file, bdd, names);
        fclose(file);

        if(kCreatePs)
            DotToPs(filename, base + ".ps");
    } else fprintf(stderr,"Could not open dot file '%s'\n", filename.c_str());
}

void Bdd::DotWithComplement(std::string filename, const bool kCreatePs) const {
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
        fclose(file);

        if(kCreatePs)
            DotToPs(filename, base + ".ps");
    } else fprintf(stderr,"Could not open dot file '%s'\n", filename.c_str());
}


