
//Student Name: Obadiah Goodnews Chukwu
//School Project

//Project: Smart City Route Management System (Menu-driven, XAI-enabled)
//NOTE:This distance are approximately for this project/demostration. 


#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <stack>
#include <set>
#include <algorithm>
#include <iomanip>
#include <limits>

using namespace std;

//-- Core Data Structures --

// Edge in adjacency list (to = neighbour index, w = distance/cost)
struct Edge {
    int to;
    int w;
};

using Graph = vector<vector<Edge>>;

// Keep a canonical undirected key (min(u,v), max(u,v)) to avoid duplicates
struct RouteKey {
    int a, b;
    bool operator<(const RouteKey& other) const {
        if (a != other.a) return a < other.a;
        return b < other.b;
    }
};

// Record an edit operation for undo/redo
enum class OpType { Add, Remove, Update };

struct Op {
    OpType type;
    int u, v;
    int oldW; // for Remove/Update
    int newW; // for Add/Update
};

//-- Global-ish Helpers --

static void rule(char ch = '-', int n = 70) {
    for (int i = 0; i < n; ++i) cout << ch;
    cout << "\n";
}

// Find edge index v in g[u]; return -1 if missing
static int findEdgeIndex(const Graph& g, int u, int v) {
    for (int i = 0; i < (int)g[u].size(); ++i) {
        if (g[u][i].to == v) return i;
    }
    return -1;
}

// Add undirected edge; if exists, overwrite weight
static void addOrUpdateUndirected(Graph& g, int u, int v, int w) {
    int iu = findEdgeIndex(g, u, v);
    if (iu == -1) g[u].push_back({ v, w }); else g[u][iu].w = w;

    int iv = findEdgeIndex(g, v, u);
    if (iv == -1) g[v].push_back({ u, w }); else g[v][iv].w = w;
}

// Remove undirected edge if present; return previous weight or -1 if absent
static int removeUndirected(Graph& g, int u, int v) {
    int prev = -1;
    int iu = findEdgeIndex(g, u, v);
    if (iu != -1) { prev = g[u][iu].w; g[u].erase(g[u].begin() + iu); }
    int iv = findEdgeIndex(g, v, u);
    if (iv != -1) g[v].erase(g[v].begin() + iv);
    return prev;
}

//-- Pretty Printers --

static void printCities(const vector<string>& city) {
    cout << "Intersections (indices):\n";
    for (int i = 0; i < (int)city.size(); ++i)
        cout << "  " << setw(2) << i << " = " << city[i] << "\n";
}

static void printAdjList(const Graph& g, const vector<string>& city) {
    cout << "Adjacency List (intersection -> (neighbour, cost)):\n";
    for (int u = 0; u < (int)g.size(); ++u) {
        cout << setw(16) << city[u] << " : ";
        for (int i = 0; i < (int)g[u].size(); ++i) {
            const Edge& e = g[u][i];
            cout << "(" << city[e.to] << ", " << e.w << ")";
            if (i + 1 < (int)g[u].size()) cout << ", ";
        }
        cout << "\n";
    }
}

static vector<vector<int>> toAdjMatrix(const Graph& g) {
    int n = (int)g.size();
    vector<vector<int>> M(n, vector<int>(n, 0));
    for (int i = 0; i < n; ++i) {
        M[i][i] = 0;
        for (auto e : g[i]) M[i][e.to] = e.w;
    }
    return M;
}

static void printAdjMatrix(const vector<vector<int>>& M, const vector<string>& city) {
    int n = (int)M.size();
    cout << setw(16) << "";
    for (int j = 0; j < n; ++j) cout << setw(12) << city[j].substr(0, 10);
    cout << "\n";
    for (int i = 0; i < n; ++i) {
        cout << setw(16) << city[i].substr(0, 10);
        for (int j = 0; j < n; ++j) cout << setw(12) << M[i][j];
        cout << "\n";
    }
}

// Build a unique undirected route list for sorting/printing
struct RouteView { int u, v, w; };

static vector<RouteView> collectUniqueRoutes(const Graph& g) {
    set<RouteKey> seen;
    vector<RouteView> out;
    for (int u = 0; u < (int)g.size(); ++u) {
        for (auto e : g[u]) {
            int a = min(u, e.to), b = max(u, e.to);
            if (!seen.count({ a,b })) {
                seen.insert({ a,b });
                // weight is symmetric; read from current e
                out.push_back({ a, b, e.w });
            }
        }
    }
    return out;
}

// Functors for sorting
struct ByDistance {
    bool operator()(const RouteView& A, const RouteView& B) const {
        if (A.w != B.w) return A.w < B.w;
        if (A.u != B.u) return A.u < B.u;
        return A.v < B.v;
    }
};
struct ByCityNames {
    const vector<string>& names;
    explicit ByCityNames(const vector<string>& n) : names(n) {}
    bool operator()(const RouteView& A, const RouteView& B) const {
        string a1 = names[A.u], a2 = names[A.v];
        string b1 = names[B.u], b2 = names[B.v];
        if (a1 != b1) return a1 < b1;
        if (a2 != b2) return a2 < b2;
        return A.w < B.w;
    }
};

static void printRoutesSorted(const Graph& g, const vector<string>& city, bool byDist) {
    auto routes = collectUniqueRoutes(g);
    if (byDist) {
        sort(routes.begin(), routes.end(), ByDistance{});
        cout << "Routes sorted by distance (ascending):\n";
    }
    else {
        sort(routes.begin(), routes.end(), ByCityNames{ city });
        cout << "Routes sorted by city names (A..Z):\n";
    }
    for (auto r : routes) {
        cout << "  " << city[r.u] << " <-> " << city[r.v] << " : " << r.w << "\n";
    }
}

//-- AI-based predictions --

// Simple congestion multiplier based on hour (0..23). Transparent, rule-based.
// XAI: In explaination, during peak hours costs are scaled up to mimic congestion.
static double congestionMultiplier(int hour) {
    // 7-9 and 16-18 are peak: 30% extra cost; nights cheaper.
    if ((hour >= 7 && hour <= 9) || (hour >= 16 && hour <= 18)) return 1.30;
    if (hour >= 22 || hour <= 5) return 0.90;
    return 1.00;
}

//-- Dijkstra (XAI) --

struct PQItem { long long d; int u; };
struct MinCmp { bool operator()(const PQItem& a, const PQItem& b) const { return a.d > b.d; } };

// Returns (visitedCount, totalDistance) and prints XAI trace and path.
static pair<int, long long> dijkstraXAI(
    const Graph& g,
    int s, int t,
    const vector<string>& city,
    bool useCongestion,
    int hour // only used if useCongestion==true
) {
    int n = (int)g.size();
    const long long INF = (numeric_limits<long long>::max)() / 4;

    vector<long long> dist(n, INF);
    vector<int> parent(n, -1);
    priority_queue<PQItem, vector<PQItem>, MinCmp> pq;

    double mult = useCongestion ? congestionMultiplier(hour) : 1.0;

    // XAI: Initialize distances; source = 0; others = INF
    cout << "XAI: Initialize distances: dist[" << city[s] << "]=0, others=INF.\n";
    dist[s] = 0;
    pq.push({ 0, s });

    int popped = 0;

    while (!pq.empty()) {
        PQItem cur = pq.top(); pq.pop();
        long long d = cur.d; int u = cur.u;
        if (d != dist[u]) continue;  // stale
        ++popped;

        // XAI: Explain why u is selected now
        cout << "XAI: Selecting next node '" << city[u]
            << "' because it currently has the smallest known total cost " << d << ".\n";

            if (u == t) {
                cout << "XAI: Destination '" << city[t] << "' popped—its distance is now final.\n";
                break;
            }

            for (auto e : g[u]) {
                // Apply optional congestion scaling transparently
                long long w = (long long)((double)e.w * mult + 0.5);
                long long nd = d + w;

                // XAI: Show relaxation attempt
                cout << "XAI: Considering edge " << city[u] << " -> " << city[e.to]
                    << " with base cost " << e.w;
                if (useCongestion) cout << " (scaled to " << w << " at hour " << hour << ")";
                cout << ". Candidate total = " << nd << ". ";

                if (nd < dist[e.to]) {
                    cout << "Improves best known dist[" << city[e.to] << "] = "
                        << (dist[e.to] == INF ? string("INF") : to_string(dist[e.to]))
                        << " -> " << nd << " (parent = " << city[u] << ").\n";
                    dist[e.to] = nd;
                    parent[e.to] = u;
                    pq.push({ nd, e.to });
                }
                else {
                    cout << "No improvement.\n";
                }
            }
    }

    if (dist[t] == INF) {
        cout << "XAI: No route exists between " << city[s] << " and " << city[t] << ".\n";
        return { popped, -1 };
    }

    // Reconstruct path
    vector<int> path;
    for (int cur = t; cur != -1; cur = parent[cur]) path.push_back(cur);
    reverse(path.begin(), path.end());

    cout << "Shortest path: ";
    for (size_t i = 0; i < path.size(); ++i) {
        cout << city[path[i]];
        if (i + 1 < path.size()) cout << " -> ";
    }
    cout << "\nTotal cost: " << dist[t] << "\n";
    cout << "XAI: Nodes popped from queue (finalized): " << popped << ".\n";
    return { popped, dist[t] };
}

//-- Search Helpers --

static bool routeExists(const Graph& g, int u, int v, int& weight) {
    int i = findEdgeIndex(g, u, v);
    if (i == -1) return false;
    weight = g[u][i].w;
    return true;
}

//-- Menu Operations --

static void addCity(vector<string>& city, unordered_map<string, int>& id, const string& name) {
    if (id.count(name)) return;
    int idx = (int)city.size();
    city.push_back(name);
    id[name] = idx;
}

static int getCityIndex(const unordered_map<string, int>& id, const string& name) {
    auto it = id.find(name);
    return (it == id.end()) ? -1 : it->second;
}

//-- main --

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    vector<string> city;
    unordered_map<string, int> id;
    Graph g;

    // Seed a small demo network
    auto seed = [&]() {
        city.clear(); id.clear();
        vector<string> names = {
            "ParkStation", "Braamfontein", "NorthGate", "EastGate", "WestRand"
        };
        for (auto& s : names) addCity(city, id, s);
        g.assign((int)city.size(), {});
        // Undirected demo routes (costs in minutes or km)
        addOrUpdateUndirected(g, id["ParkStation"], id["Braamfontein"], 7);
        addOrUpdateUndirected(g, id["ParkStation"], id["NorthGate"], 10);
        addOrUpdateUndirected(g, id["ParkStation"], id["EastGate"], 8);
        addOrUpdateUndirected(g, id["NorthGate"], id["WestRand"], 12);
        addOrUpdateUndirected(g, id["EastRand"], id["WestRand"], 9);
        addOrUpdateUndirected(g, id["NorthGate"], id["WestRand"], 11);
        };
    seed();

    stack<Op> undoSt, redoSt; // undo/redo operation stacks

    while (true) {
        rule('=');
        cout << "SMART CITY ROUTE MANAGEMENT (XAI)\n";
        rule();
        printCities(city);
        rule();
        cout << "MENU: (Case Sensitive)\n"
            << "1. Add a route\n"
            << "2. Remove a route\n"
            << "3. Update a route\n"
            << "4. View all routes (Adjacency List)\n"
            << "5. View adjacency matrix\n"
            << "6. Find shortest route (Dijkstra)\n"
            << "7. Sort routes (1=by distance, 2=by city names)\n"
            << "8. Search for a route\n"
            << "9. Undo last change\n"
            << "10. Redo\n"
            << "11. Reset\n"
            << "12. Exit\n"
            << "Choose: ";
        int ch; if (!(cin >> ch)) break;
        rule();

        if (ch == 1) {
            string A, B; int w;
            cout << "Add route A B weight (names, integer weight): ";
            cin >> A >> B >> w;
            int u = getCityIndex(id, A), v = getCityIndex(id, B);
            if (u == -1 || v == -1 || u == v) {
                cout << "Invalid city names.\n"; continue;
            }
            int prev = -1;
            int iuv = findEdgeIndex(g, u, v);
            if (iuv != -1) prev = g[u][iuv].w;
            addOrUpdateUndirected(g, u, v, w);
            cout << "Route " << A << " <-> " << B << " set to " << w << ".\n";
            // Record op (if it was new, oldW = -1; else update)
            if (prev == -1) undoSt.push(Op{ OpType::Add, u, v, -1, w });
            else undoSt.push(Op{ OpType::Update, u, v, prev, w });
            while (!redoSt.empty()) redoSt.pop();

            // XAI: why we accept/overwrite
            cout << "XAI: Route stored as undirected; if existed, its weight was overwritten to maintain a single, current cost.\n";

        }
        else if (ch == 2) {
            string A, B;
            cout << "Remove route A B (names): ";
            cin >> A >> B;
            int u = getCityIndex(id, A), v = getCityIndex(id, B);
            if (u == -1 || v == -1 || u == v) { cout << "Invalid names.\n"; continue; }
            int prev = removeUndirected(g, u, v);
            if (prev == -1) cout << "No such route.\n";
            else {
                cout << "Removed " << A << " <-> " << B << ".\n";
                undoSt.push(Op{ OpType::Remove, u, v, prev, -1 });
                while (!redoSt.empty()) redoSt.pop();
                // XAI: why we allow removal
                cout << "XAI: Removing this edge updates the reachable structure and will change shortest paths accordingly.\n";
            }

        }
        else if (ch == 3) {
            string A, B; int w;
            cout << "Update route A B to new weight: ";
            cin >> A >> B >> w;
            int u = getCityIndex(id, A), v = getCityIndex(id, B);
            if (u == -1 || v == -1 || u == v) { cout << "Invalid names.\n"; continue; }
            int old;
            if (!routeExists(g, u, v, old)) { cout << "Route does not exist.\n"; continue; }
            addOrUpdateUndirected(g, u, v, w);
            cout << "Updated " << A << " <-> " << B << " to " << w << ".\n";
            undoSt.push(Op{ OpType::Update, u, v, old, w });
            while (!redoSt.empty()) redoSt.pop();
            // XAI: why update matters
            cout << "XAI: Updating the cost directly affects future optimizations, enabling routes that are now shorter to be preferred.\n";

        }
        else if (ch == 4) {
            printAdjList(g, city);

        }
        else if (ch == 5) {
            auto M = toAdjMatrix(g);
            printAdjMatrix(M, city);

        }
        else if (ch == 6) {
            string S, T;
            cout << "Enter source and target names: ";
            cin >> S >> T;
            int s = getCityIndex(id, S), t = getCityIndex(id, T);
            if (s == -1 || t == -1 || s == t) { cout << "Invalid names.\n"; continue; }

            char useAIch; int hour = 12;
            cout << "Use congestion factor (y/n)? ";
            cin >> useAIch;
            bool useAI = (useAIch == 'y' || useAIch == 'Y');
            if (useAI) {
                cout << "Enter hour of day (0-23): ";
                cin >> hour;
                double mult = congestionMultiplier(hour);
                cout << "XAI: Congestion multiplier at hour " << hour << " = " << mult
                    << " (peak hours increase cost; night slightly reduces).\n";
            }

            auto res = dijkstraXAI(g, s, t, city, useAI, hour);
            if (res.second >= 0) {
                cout << "Explanation: Shortest path chosen because its total cost "
                    << "is minimal among all alternatives discovered via relaxations.\n";
            }

        }
        else if (ch == 7) {
            int mode;
            cout << "Sort mode (1=distance, 2=city names): ";
            cin >> mode;
            printRoutesSorted(g, city, mode == 1);
            // XAI: sorting rationale
            cout << "XAI: Routes were sorted using a custom comparator ";
            if (mode == 1) cout << "(ascending distance).\n";
            else cout << "(lexicographic order of city names; distance as tiebreak).\n";

        }
        else if (ch == 8) {
            string A, B;
            cout << "Search route A B (names): ";
            cin >> A >> B;
            int u = getCityIndex(id, A), v = getCityIndex(id, B);
            if (u == -1 || v == -1 || u == v) { cout << "Invalid names.\n"; continue; }
            int w;
            if (routeExists(g, u, v, w)) {
                cout << "Found: " << A << " <-> " << B << " with cost " << w << ".\n";
                cout << "XAI: Existence confirmed by scanning adjacency of " << A
                    << " and matching " << B << " as a neighbour.\n";
            }
            else {
                cout << "No such route.\n";
                cout << "XAI: Search failed because no neighbour entry matched the requested pair.\n";
            }

        }
        else if (ch == 9) {
            if (undoSt.empty()) { cout << "Nothing to undo.\n"; continue; }
            Op op = undoSt.top(); undoSt.pop();
            if (op.type == OpType::Add) {
                // Undo add -> remove
                removeUndirected(g, op.u, op.v);
                cout << "Undo: removed route.\n";
            }
            else if (op.type == OpType::Remove) {
                // Undo remove -> re-add oldW
                addOrUpdateUndirected(g, op.u, op.v, op.oldW);
                cout << "Undo: restored route with previous cost " << op.oldW << ".\n";
            }
            else { // Update
                addOrUpdateUndirected(g, op.u, op.v, op.oldW);
                cout << "Undo: reverted update to cost " << op.oldW << ".\n";
            }
            redoSt.push(op);
            cout << "XAI: Undo replays the inverse edit to restore the previous graph state.\n";

        }
        else if (ch == 10) {
            if (redoSt.empty()) { cout << "Nothing to redo.\n"; continue; }
            Op op = redoSt.top(); redoSt.pop();
            if (op.type == OpType::Add) {
                addOrUpdateUndirected(g, op.u, op.v, op.newW);
                cout << "Redo: re-added route with cost " << op.newW << ".\n";
            }
            else if (op.type == OpType::Remove) {
                removeUndirected(g, op.u, op.v);
                cout << "Redo: removed route again.\n";
            }
            else { // Update
                addOrUpdateUndirected(g, op.u, op.v, op.newW);
                cout << "Redo: re-applied update to cost " << op.newW << ".\n";
            }
            undoSt.push(op);
            cout << "XAI: Redo reapplies the previously undone edit to advance state forward.\n";

        }
        else if (ch == 11) {
            seed(); while (!undoSt.empty()) undoSt.pop(); while (!redoSt.empty()) redoSt.pop();
            cout << "Network reset.\n";

        }
        else if (ch == 12) {
            cout << "Goodbye.\n";
            break;

        }
        else {
            cout << "Invalid choice.\n";
        }
    }
    return 0;
}

/*
    DOCUMENTATION
    -------------
    Project overview:
      This program represents and also manages small a city road network, 
      supports editing the network (add/remove/update), viewing adjacency list & matrix, 
      sorting/searching routes. It computes shortest routes using Dijkstra’s algorithm,
      adds an optional, transparent “congestion multiplier” (simple rule-based AI) to 
      scale costs by time of day. Explains decisions at runtime (XAI): prints why a node is selected, 
      why a distance improves, and how congestion affects cost.

    Approach I chose
      - Graph as an adjacency list (vector<vector<Edge>>) because it is space-
        efficient and neighbors are fast to iterate for Dijkstra.
      - City names map (unordered_map<string,int>) to translate names <-> indices.
      - Edits (add/remove/update) are undirected and recorded in two stacks
        (undo/redo) for state control.
      - A set<(min(u,v),max(u,v))> avoids duplicate undirected listings.
      - Dijkstra (priority queue) finds shortest routes; path via parent[].

    Data structures / algorithms used
      - vector / adjacency list (graph core)
      - unordered_map (name lookup)
      - stack (undo/redo)
      - set (unique undirected route enumeration)
      - queue (min-heap via custom comparator)
      - sorting with custom functors (ByDistance, ByCityNames)
      - searching via adjacency scan

    XAI (Explainable AI) implementation
      - During Dijkstra:
        * Prints why a node is picked (“currently smallest known total cost”).
        * Prints each relaxation attempt, the candidate total, and whether it improved dist[v].
      - Congestion explanation:
        * Hour-based multiplier (peak 7–9 & 16–18: ×1.30; night 22–5: ×0.90; otherwise ×1.00).
        * The program prints the exact multiplier and the scaled weight so the effect can be traced.
      - Edit operations: After add/update/remove, the program states why an action is valid and how it changes optimization.

    AI Integration
      - A simple, transparent congestion multiplier (time-of-day) scales edge
        costs. The program clearly print the chosen multiplier and how it affects total
        cost. This is rule-based and explainable.

    Input/Output & validation
      - Menu expects valid city names from the seeded set (ParkStation, Braamfontein, NorthGate, EastGate, WestRand),
      and are case sensitive.
      - Prevents self-loops and invalid names; overwriting a route weight is explicit and explained.
      - Weights are treated as kilometres (integers); Dijkstra uses long long internally after scaling.

    Testing performed
      - Verified printing of adjacency list/matrix after each edit.
      - Checked add→undo→redo cycles for consistency of weights and symmetry.
      - Ran Dijkstra on several pairs with and without congestion (e.g. ParkStation - WestRand at hours 8 and 12) and validated the XAI trace.
      - Sorted routes by distance and by city names; checked for duplicates (none due to RouteKey set).
      - Edge cases: removing non-existent edge; searching missing route; identical source/target are disallowed.


    Academic integrity & GenAI/XAI reflection
      - I used GenAI as a coding assistant for brainstorming structure and wording of XAI messages.
      - My changes:
        * Adapted units to kilometres; simplified comparators; added symmetric update semantics; expanded XAI prints.
        * Wrote/edited comments and menu copy to reflect my understanding (data structures, big-O, and correctness).
      - What worked: accelerating boilerplate (menus/printing) and reminding me of clean abstractions (RouteKey, Op).
      - What didn’t: AI suggestions for I/O sometimes too effusive; I had to rewrite to give it a shorter, better/understandable meaning.
      - I verified all logic by compiling, running multiple scenarios, and cross-checking Dijkstra behavior.
      - All reasoning and final code were reviewed and tested by me; the responsibility for correctness is mine. 
*/
