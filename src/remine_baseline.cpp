/*
Usage:
./bin/remine_baseline ./data_remine/nyt_deps_train.txt remine_extraction/ver2/nyt_6k_remine_2.txt remine_extraction/ver2/nyt_6k_remine_pos.txt remine_extraction/ver2/nyt_6k_remine_4.txt
*/

#include <fstream>
#include "utils/parameters.h"
#include "utils/utils.h"

int MIN_DIS = 0;
set<string> verb_tags = {"VB", "VBD", "VBG", "VBN", "VBP", "VBZ"};
void split(const string &s, char delim, vector<string>& result) {
    stringstream ss;
    ss.str(s);
    string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
}

void printSubtree(const vector<vector<int>>& parent, const vector<string> tags, set<int>& bgs, int index, int left, int right) {
    // cerr << index << " inserted !" << endl;
    // if (verb_tags.count(tags[index - 1])) {
    if (index <= right && index > left) {
        for (int i = 0; i < parent[index].size(); ++i) {
            printSubtree(parent, tags, bgs, parent[index][i], left, right);
        }
            //bgs.insert(parent[index][i]);
        bgs.insert(index);
    }

    
}

void process(const vector<int>& deps, const vector<string>& tags, const vector<string>& types, const vector<pair<int, int>>& entityMentions, FILE* out) {
	vector<vector<int>> children(deps.size() + 1);
    vector<vector<int>> parents(deps.size() + 1);
    int root;

    assert(deps.size() == types.size());
    for (int i = 0; i < deps.size(); ++ i) {
        int a = i + 1, b = deps[i];
        if (b == 0) {
        	children[a].push_back(a);
        }
        parents[b].push_back(a);
        int multi_root = 0;
        while (b != 0) {
            ++ multi_root;
        	children[a].push_back(b);
        	b = deps[b - 1];
            if (multi_root > deps.size()) return;
    	}	
    }

    for (auto& item : children) {
    	reverse(item.begin(), item.end());
    }
    
    /*
    for (int i = 0; i < children.size(); ++i) {
    	 cout << "node : " << i << " ";
    	for (const auto& t : children[i])
    		cout << t << " ";
        cout << "childrens: ";
        for (const auto& t : parents[i])
            cout << t << " ";
    	cout << endl;
    }
    */
    
    vector<vector<int>> out_nodes(entityMentions.size());
    vector<vector<string>> out_types(entityMentions.size());
    vector<string> segments;

    for (int i = 0; i < entityMentions.size(); ++i) {
        // cerr << entityMentions[i].first << " " << entityMentions[i].second << endl;
    	for (int index = entityMentions[i].first; index < entityMentions[i].second; ++index) {

    		if (deps[index] <= entityMentions[i].first || deps[index] > entityMentions[i].second) {
    			if (deps[index] == 0) {
                    /* Root node */
    				out_nodes[i].push_back(index + 1);
    			}
    			else {
    				out_nodes[i].push_back(deps[index]);
    			}
                out_types[i].push_back(types[index]);
    		}
    	}
    }

    // Shortest path version
    for (int j = 1; j < entityMentions.size(); ++j) {
        int distance = deps.size();
        int min_i = 0;
        int min_start = 0, min_end = 0, min_parent = 0;
        string start_type, end_type;
        set<int> bgs;
        for (int i = 0; i < j; ++i) {
            // Fix multi-out_nodes problem in this version
            for(int start_index = 0; start_index < out_nodes[i].size(); ++ start_index)
                for (int end_index = 0; end_index < out_nodes[j].size(); ++ end_index) {
                    if (out_types[i][start_index].find("nmod") != string::npos || out_types[i][start_index].find("dobj") != string::npos || 
                       out_types[j][end_index].find("nsubj") != string::npos )
                        continue;

            //for (int start : out_nodes[i]) 
            //    for (int end : out_nodes[j]) {
                    int start = out_nodes[i][start_index];
                    int end = out_nodes[j][end_index];
                    int min_depth = min(children[start].size(), children[end].size());
                    int parent = 0, k;

                    for (k = 0; k < min_depth; ++k, parent=k) {
                        if (children[start][k] != children[end][k]) {
                            break;
                        }
                    }

                    if (children[end].size() + children[start].size() + 2 - 2 * parent <= distance) {
                        distance = children[end].size() + children[start].size() + 2 - 2 * parent;
                        min_start = start;
                        min_end = end;
                        min_parent = parent;
                        min_i = i;
                        start_type = out_types[i][start_index];
                        end_type = out_types[j][end_index];
                    }
                }
        }

        // assert(min_parent != 0);
        if (min_parent == 0) continue;

        // cerr << min_i << " " << j << "\t" << min_start << "\t" << min_end << "\t" << min_parent << endl;
        // cerr << children[min_start].size() << "\t" << children[min_end].size() << endl;
        for (int st = min_parent; st < children[min_start].size(); ++st) {
            printSubtree(parents, tags, bgs, children[min_start][st], entityMentions[min_i].second, entityMentions[j].first);
        }

        printSubtree(parents, tags, bgs, min_start, entityMentions[min_i].second, entityMentions[j].first);
        if (min_start != min_end) {
            for (int st = min_parent; st < children[min_end].size(); ++st) {
                printSubtree(parents, tags, bgs, children[min_end][st], entityMentions[min_i].second, entityMentions[j].first);
            }

            printSubtree(parents, tags, bgs, min_end, entityMentions[min_i].second, entityMentions[j].first);
        }
        
        /*
        vector<int> erased;

        for (const auto& path : bgs) {
            if (path <= entityMentions[min_i].second || path > entityMentions[j].first )
                erased.push_back(path);
        }

        for (const auto& path : erased) {
            bgs.erase(path);
        }
        */

        // fprintf(out, "%d_%s %d_%s\t", min_i, start_type.c_str(), j, end_type.c_str());
        fprintf(out, "%d %d\t", min_i, j);

        for (const auto& t : bgs) {
            fprintf(out, "%d ", t);
        }

        fprintf(out, "<>");

    }

    // All pair version
    /*
    for (int i = 0; i < entityMentions.size(); ++i) {
    	for (int j = i + 1; j < entityMentions.size(); ++j) {
    		set<int> bgs;
            // cerr << i << " " << j << "\t";
    		if (out_nodes[i].size() == 1 && out_nodes[j].size() == 1) {
    			int start = out_nodes[i][0];
    			int end = out_nodes[j][0];
    			
    			int min_depth = min(children[start].size(), children[end].size());
    			int parent = 0, k;
    			// if access root,
                bool connect = true; 
                //cout << "min depth:" << min_depth << endl;
    			for (k = 0; k < min_depth; ++k, parent=k) {
    				if (children[start][k] != children[end][k]) {
                        if (verb_tags.count(tags[children[start][k] - 1]) && verb_tags.count(tags[children[end][k] - 1]))
                            connect = false;
                        //cout << children[end].size() + children[start].size() - 2*k << endl;
    					break;
    				}
    			}

                // cerr << "parent" << parent << endl;
                // cerr << children[end].size() << "\t" << children[start].size() << endl;
                if (children[end].size() + children[start].size() + 2 - 2*parent > MIN_DIS)
                    connect = false;

                if (!connect) continue;

                //cerr << "start" << endl;
    			//cerr << "start" << start << "end" << end <<" parent" << parent << endl;
                //cerr << children[start].size() << " " << children[end].size() << endl;

                // start from root
                if (parent == 0)
                    cerr << children[start][0] << children[end][0] << endl;
    			for (int st = parent; st < children[start].size(); ++st) {
                    printSubtree(parents, tags, bgs, children[start][st]);
                }

                printSubtree(parents, tags, bgs, start);
				for (int st = parent; st < children[end].size(); ++st) {
					// segments.back() += " " + to_string(children[end][st]);
                    printSubtree(parents, tags, bgs, children[end][st]);
                    //printSubtree(parents, bgs, end);
                }   
                printSubtree(parents, tags, bgs, end);
                
                vector<int> erased;

                for (const auto& path : bgs) {
                    if (path <= entityMentions[i].second || path > entityMentions[j].first )
                        erased.push_back(path);
                }

                for (const auto& path : erased) {
                    bgs.erase(path);
                }
                fprintf(out, "%d %d\t", i, j);
                for (const auto& t : bgs) {
                    fprintf(out, "%d ", t);
                }
    		}
            fprintf(out, "<>");
            // cout << "<>";
    	}
    }*/

    /*
    for (const auto& seg : segments) {
    	if (seg.length() > 0) {
    		cout << seg << endl;
    	}
    }
    */

}

int main(int argc, char* argv[])
{
	FILE* depIn = tryOpen(argv[1], "r");
	vector<vector<int>> depPaths;
    vector<vector<string>> depTypes;
    char currentDep[100];
	while (getLine(depIn)) {
		vector<int> tmp;
        vector<string> tmp_type;
		depPaths.push_back(tmp);
        depTypes.push_back(tmp_type);
		stringstream sin(line);
		for(string temp; sin >> temp;) {
            strcpy(currentDep, temp.c_str());
            int idx = atoi(strtok (currentDep, "_"));
            int idx_dep = atoi(strtok (NULL, "_"));
            string xxx(strtok(NULL, "_"));
			depPaths.back().push_back(idx_dep);
            depTypes.back().push_back(xxx);
            // cout << depPaths.back().back() << " " << depTypes.back().back() << endl;
		}
	}
    fclose(depIn);

    vector<vector<string>> posPaths;
    FILE* posIn = tryOpen(argv[3], "r");
    while (getLine(posIn)) {
        vector<string> tmp;
        posPaths.push_back(tmp);
        stringstream sin(line);
        for (string temp; sin >> temp;) {
            posPaths.back().push_back(temp);
        }

    }

	FILE* emIn = tryOpen(argv[2], "r");

    MIN_DIS = 4;
	int docs = 0;
    FILE* out = tryOpen(argv[4], "w");
    // FILE* out_dep = tryOpen(argv[5], "w");
    cout << "dependencies readed" << endl;
	while (getLine(emIn)) {
        // cerr << docs << "DOC" << endl;
		stringstream sin(line);
		vector<pair<int ,int>> ems;
		for(string temp; sin >> temp;) {
			vector<string> segs;
			split(temp, '_', segs);
			assert(segs.size() == 2);
			ems.push_back(make_pair(stoi(segs[0]), stoi(segs[1])));
		}
        process(depPaths[docs], posPaths[docs], depTypes[docs], ems, out);
        fprintf(out, "\n");
        ++ docs;
        // break;
        //if (docs == 5)
		//  break;
	}
    fclose(emIn);
    fclose(out);

}
