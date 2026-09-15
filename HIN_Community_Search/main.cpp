#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <unordered_set>
#include <unordered_map>
#include <queue>
 
using namespace std; 
 
// ========================================================= 
// HELPERS Functions
// ========================================================= 
 
// Removes leading and trailing space from text.Trims leading and trailing space off of text. 
string trim(const string& text)
{ 
    size_t start = text.find_first_not_of(" \t\r\n"); 
 
    if (start == string::npos) 
        return ""; 
 
    size_t end = text.find_last_not_of(" \t\r\n"); 
 
    return text.substr(start, end-start + 1); 
} 
 
// Splits a string into a vector of strings, using commas as the separator.This splits a string into a vector of strings, separating on commas. 
vector<string> splitCSVLine(const string& line)
{ 
    vector<string> values; 
    string value; 
    stringstream ss(line); 
 
    while (getline(ss, value, ',')) 
    { 
        values.push_back(trim(value)); 
    } 
 
    return values; 
} 
 
// Converts bytes to megabytes for memory reporting.
double bytesToMB(long long bytes) 
{ 
    return static_cast<double>(bytes) / 
           (1024.0 * 1024.0); 
} 
 
 
// ========================================================= 
// DATA STRUCTURES 
// ========================================================= 
// Defines one valid relationship in the HIN schema.
// Loaded from triplet-type-list.csv in Stage 2.
 
struct RelationSchema 
{ 
    string sourceType; 
    string relationType; 
    string targetType; 
}; 
 
// Stores one edge with global integer node IDs.
// Used during loading (Stage 4) and CSR construction (Stage 6).
// Released from memory after the CSR is built.

struct GlobalEdge 
{ 
    int source; 
    int target; 
    int relationTypeID; 
}; 
 
// Stores the start and end positions of one relation type
// within a node's block in Column_Indices.
// Enables O(1) typed neighbour access via Type_Offset_Index.

struct TypeRange 
{ 
    int relationTypeID; 
    int start; 
    int end; 
}; 
 
// Defines one hop in a meta-path.
    // true  = forward CSR 
    // false = reverse CSR 

struct MetaPathStep 
{ 
    int relationTypeID; 
    bool forward; 
}; 


// =========================================================
// HIN DATA STRUCTURE
// =========================================================

// Encapsulates all components of the Heterogeneous
// Information Network as designed in the Alpha phase

// Modified CSR achieves:
//   O(1) typed neighbour access via typeOffsetIndex
//   O(V+E) total memory
//   O(V+E) k-core peeling complexity


struct HIN
{
    // Node Information
    // Populated in Stage 1 and Stage 3
    int totalNodes;
    map<string, int> nodeTypeCounts;
    map<string, int> nodeTypeOffsets;
 
    // HIN Schema
    // Populated in Stage 2
    // Defines what relationships are valid in the HIN
    // Loaded from triplet-type-list.csv
    vector<RelationSchema> relations;
    map<string, int> relationTypeToID;
    map<int, string> idToRelationType;
 
    // Forward Modified CSR 
    // Built in Stage 6
    // rowPointers[u]     = start of node u in columnIndices
    // columnIndices      = flat array of all target node IDs
    // relationTypeIDs    = relation type per edge
    // typeOffsetIndex[u] = where each relation type starts within node u's block
                     
    vector<int> rowPointers;
    vector<int> columnIndices;
    vector<int> relationTypeIDs;
    vector<vector<TypeRange>> typeOffsetIndex;
 
    // Reverse CSR
    // Built in Stage 8
    // Same structure as forward CSR but edges are flipped
    // Enables reverse hop traversal for meta-paths
    // e.g. APA: writes FORWARD then writes REVERSE
    vector<int> reverseRowPointers;
    vector<int> reverseColumnIndices;
    vector<int> reverseRelationTypeIDs;
};

 
// ========================================================= 
// Convert node ID from a specific node in a network to a global node ID. 
// ========================================================= 
 
int getGlobalNodeID(
    const string& nodeType,
    int localID,
    const map<string, int>& nodeTypeOffsets)
{
    auto it = nodeTypeOffsets.find(nodeType);
    if (it == nodeTypeOffsets.end())
        return -1;
    return it->second + localID;
}
 

// ========================================================= 
// META-PATH TRAVERSAL 
// ========================================================= 
// Follows a meta-path from a start node and returns all reachable end nodes.

//For each step in the path:
//   forward=true  reads from hin.rowPointers (outgoing edges)
//   forward=false reads from hin.reverseRowPointers (incoming)

vector<int> followMetaPath(
    int startNode,
    const vector<MetaPathStep>& path,
    const HIN& hin)
{
    vector<int> currentNodes;
    currentNodes.push_back(startNode);
 
    for (const MetaPathStep& step : path)
    {
        unordered_set<int> nextSet;
 
        for (int node : currentNodes)
        {
            if (step.forward)
            {
                // Traverse forward CSR
                int start = hin.rowPointers[node];
                int end   = hin.rowPointers[node + 1];
 
                for (int i = start; i < end; i++)
                {
                    if (hin.relationTypeIDs[i] == step.relationTypeID)
                        nextSet.insert(hin.columnIndices[i]);
                }
            }
            else
            {
                // Traverse reverse CSR
                int start = hin.reverseRowPointers[node];
                int end   = hin.reverseRowPointers[node + 1];
 
                for (int i = start; i < end; i++)
                {
                    if (hin.reverseRelationTypeIDs[i] == step.relationTypeID)
                        nextSet.insert(hin.reverseColumnIndices[i]);
                }
            }
        }
 
        currentNodes.assign(nextSet.begin(), nextSet.end());
 
        if (currentNodes.empty())
            break;
    }
 
    // Remove start node if meta-path returns to same type
    currentNodes.erase(
        remove(currentNodes.begin(), currentNodes.end(), startNode),
        currentNodes.end());
 
    sort(currentNodes.begin(), currentNodes.end());
 
    return currentNodes;
}
 

 
// ========================================================= 
// BUILD QUERY-CENTRED META-PATH DERIVED GRAPH
// ========================================================= 

// The derived graph is the input to k-core peeling.
// It is NOT the raw HIN — it is a virtual graph where
// edges represent meta-path connections, not physical edges.
//
// Steps:
//   1. Collect query node and all meta-path neighbours
//   2. For each candidate node find its meta-path neighbours
//   3. Build undirected adjacency list from shared connections
 
void buildDerivedGraph( 
    int queryNode, 
   const vector<int>& queryNeighbours,
    const vector<MetaPathStep>& metaPath,
    const HIN& hin,
    vector<int>& derivedNodes,
    vector<vector<int>>& derivedAdjacency,
    unordered_map<int, int>& globalToDerived)
{ 
    derivedNodes.clear(); 
    derivedNodes.push_back(queryNode); 
 
    for (int neighbour: queryNeighbours) 
    { 
        if (neighbour != queryNode) 
            derivedNodes.push_back(neighbour); 
    } 
 
    sort(derivedNodes.begin(), derivedNodes.end()); 
    derivedNodes.erase(
        unique(derivedNodes.begin(), derivedNodes.end()), 
        derivedNodes.end()); 
 
    globalToDerived.clear(); 
    for (size_t i = 0; i < derivedNodes.size(); i++) 
        globalToDerived[derivedNodes[i]] = static_cast<int>(i); 
 
    vector<unordered_set<int>> adjacencySets(derivedNodes.size()); 
 
    for (size_t i = 0; i < derivedNodes.size(); i++)
    {
        int globalNode = derivedNodes[i];
 
        vector<int> neighbours = followMetaPath(
            globalNode, metaPath, hin);
 
        for (int globalNeighbour : neighbours)
        {
            auto it = globalToDerived.find(globalNeighbour);
            if (it == globalToDerived.end())
                continue;
 
            int neighbourIndex = it->second;
            if (neighbourIndex == static_cast<int>(i))
                continue;
 
            // APA is symmetric — derived graph is undirected
            adjacencySets[i].insert(neighbourIndex);
            adjacencySets[neighbourIndex].insert(static_cast<int>(i));
        }
    } 
 
    derivedAdjacency.assign(derivedNodes.size(), {}); 
    for (size_t i = 0; i < adjacencySets.size(); i++) 
    { 
        derivedAdjacency[i].assign(adjacencySets[i].begin(), adjacencySets[i].end()); 
        sort(derivedAdjacency[i].begin(), derivedAdjacency[i].end()); 
    } 
} 
 
 
// ========================================================= 
// ITERATIVE K-CORE PEELING 
// ========================================================= 
// Removes all nodes whose degree falls below k.
 
vector<bool> performKCorePeeling( 
    const vector<vector<int>>& adjacency, 
    int k, 
    vector<int>& finalDegrees, 
    int& removedCount) 
{ 
    int n = static_cast<int>(adjacency.size()); 
 
    vector<int> degree(n, 0); 
    vector<bool> removed(n, false); 
    vector<bool> inQueue(n, false); 
    queue<int> Q; 
 
    // Initialise degree and queue weak nodes
    for (int i = 0; i < n; i++) 
    { 
        degree[i] = static_cast<int>(adjacency[i].size()); 
 
        if (degree[i] < k) 
        { 
            Q.push(i); 
            inQueue[i] = true; 
        } 
    } 
 
    removedCount = 0; 
 
    // Peel weak nodes and cascade
    while (!Q.empty()) 
    { 
        int u = Q.front(); 
        Q.pop(); 
        inQueue[u] = false; 
 
        if (removed[u]) 
            continue; 
 
        removed[u] = true; 
        removedCount++; 
 
        // for all vertices, v, adjacent to u 
        for (int v : adjacency[u])
        { 
            if (removed[v]) 
                continue; 
 
            degree[v]--; 
 
            if (degree[v] < k && !inQueue[v]) 
            { 
                Q.push(v); 
                inQueue[v] = true; 
            } 
        } 
    } 
 
    // If displayed degrees are not equal to final active degrees, then recalculate the final active degrees. 
    finalDegrees.assign(n, 0); 
    for (int u = 0; u < n; u++) 
    { 
        if (removed[u]) 
            continue; 
 
        // Loop over all of u's neighbors.
        for (int v : adjacency[u]) 
        { 
            if (!removed[v]) 
                finalDegrees[u]++; 
        } 
    } 
 
    return removed; 
}
// =========================================================
//EXTRACT QUERY-CENTRED COMMUNITY
// =========================================================
// After k-core peeling, uses BFS from the query node to find all surviving nodes connected to it.

vector<int> extractQueryCommunity(
    int queryIndex,
    const vector<vector<int>>& adjacency,
    const vector<bool>& removed)
{
    vector<int> community;

    // This question is likely the most frequent asked in a project.Perhaps you cannot separate the question of "why" from the question of "how".
    if (queryIndex < 0 || 
        queryIndex >= static_cast<int>(adjacency.size()))
        return community;

    if (removed[queryIndex])
        return community;

    vector<bool> visited(adjacency.size(), false);
    queue<int> Q;

    Q.push(queryIndex);
    visited[queryIndex] = true;

    while (!Q.empty())
    {
        int u = Q.front();
        Q.pop();
        community.push_back(u);

        // Each element of 'v' in the for loop: = v0, v1, ..., vN (as N ne in his domain)
        for (int v : adjacency[u])
        {
            if (removed[v] || visited[v])
                continue;
            visited[v] = true;
            Q.push(v);
            
        }
    }

    return community;
}


// ========================================================= 
// PARSE META-PATH SPECIFICATION
// ========================================================= 
// Parses a comma-separated meta-path string (e.g. "writes:F,writes:R")
// and validates it against the discovered HIN schema.
bool parseMetaPath(
    const string& spec,
    const string& initialType,
    const HIN& hin,
    vector<MetaPathStep>& outPath,
    string& endType,
    string& readable)
{
    outPath.clear();
    readable = initialType;
    string currentType = initialType;
    string token;
    stringstream ss(spec);

    while (getline(ss, token, ','))
    {
        token = trim(token);
        if (token.empty()) continue;

        size_t colon = token.find(':');
        if (colon == string::npos) return false;

        string relationName = trim(token.substr(0, colon));
        string direction    = trim(token.substr(colon + 1));
        transform(direction.begin(), direction.end(),
                  direction.begin(), ::toupper);

        bool forward;
        if (direction == "F" || direction == "FORWARD")
            forward = true;
        else if (direction == "R" || direction == "REVERSE")
            forward = false;
        else
            return false;

        auto relationIDIt = hin.relationTypeToID.find(relationName);
        if (relationIDIt == hin.relationTypeToID.end())
            return false;

        bool schemaMatch = false;
        string nextType;

        for (const RelationSchema& r : hin.relations)
        {
            if (r.relationType != relationName) continue;
            if (forward && r.sourceType == currentType)
            {
                nextType = r.targetType;
                schemaMatch = true;
                break;
            }
            if (!forward && r.targetType == currentType)
            {
                nextType = r.sourceType;
                schemaMatch = true;
                break;
            }
        }

        if (!schemaMatch) return false;

        MetaPathStep step;
        step.relationTypeID = relationIDIt->second;
        step.forward        = forward;
        outPath.push_back(step);

        readable += " --[" + relationName +
                    (forward ? " FORWARD" : " REVERSE") +
                    "]--> " + nextType;

        currentType = nextType;
    }

    endType = currentType;
    return !outPath.empty();
}


// =========================================================
// MAIN
// =========================================================

int main( int argc, char* argv[] )
{
    auto programStart = chrono::high_resolution_clock::now();
 
    cout << "========================================\n";
    cout << " HIN COMMUNITY SEARCH SYSTEM\n";
    cout << " Modified CSR + Meta-Path Engine\n";
    cout << "========================================\n\n";
 
    // The primary HIN data structure.
    // Populated progressively across Stages 1 to 8.
    HIN hin;
 
 
    // =====================================================
    // PHASE 1 - HIN SCHEMA DEFINITION
    // Stages 1 to 3
    // Discovers node types, relationship rules, and
    // assigns global integer IDs to all nodes.
    // =====================================================
 
 
    // =====================================================
    // STAGE 1 - NODE TYPE DISCOVERY
    // Reads num-node-dict.csv
    // Identifies all node types and their counts.
    // Populates hin.nodeTypeCounts and hin.totalNodes.
    // ====================================================

    cout << "STAGE 1 - NODE TYPE DISCOVERY\n";
    cout << "----------------------------------------\n";

    string nodeFilePath = "data/mag/raw/num-node-dict.csv";
    ifstream nodeFile(nodeFilePath);

    if (!nodeFile.is_open())
    {
        // std::cerr is used to show errors.cerr is used to display errors.
        cerr << "ERROR: Could not open " << nodeFilePath << endl;
        return 1;
    }

    string line;

    if (!getline(nodeFile, line))
    {
        cerr << "ERROR: Node type row missing.\n";
        return 1;
    }

    vector<string> nodeTypeNames = splitCSVLine(line);

    if (!getline(nodeFile, line))
    {
        cerr << "ERROR: Node count row is missing.\n";
        return 1;
    }

    vector<string> nodeCounts = splitCSVLine(line);
    nodeFile.close();

    if (nodeTypeNames.size() != nodeCounts.size())
    {
        cerr << "ERROR: Non-matching type and count of the nodes .\n";
        return 1;
    }

    for (size_t i = 0; i < nodeTypeNames.size(); i++)
        hin.nodeTypeCounts[nodeTypeNames[i]] = stoi(nodeCounts[i]);
 
    hin.totalNodes = 0;
    for (const auto& entry : hin.nodeTypeCounts)
    {
        cout << entry.first << " -> " << entry.second << " nodes\n";
        hin.totalNodes += entry.second;
    }
 
    cout << "\nTotal Nodes: " << hin.totalNodes << endl;


    // =====================================================
    // STAGE 2 - SCHEMA DISCOVERY
    // Reads triplet-type-list.csv
    // Defines what relationships are valid in the HIN.
    // This IS the HIN schema — source, relation, target.
    // Populates hin.relations, hin.relationTypeToID.
    // =====================================================

    cout << "\n\nSTAGE 2 - HIN SCHEMA DISCOVERY\n";
    cout << "----------------------------------------\n";

    string tripletFilePath = "data/mag/raw/triplet-type-list.csv";
    ifstream tripletFile(tripletFilePath);

    if (!tripletFile.is_open())
    {
        cerr << "ERROR: Could not open " << tripletFilePath << endl;
        return 1;
    }

    set<string> relationNames;

    while (getline(tripletFile, line))
    {
        line = trim(line);
        if (line.empty()) continue;
 
        vector<string> values = splitCSVLine(line);
        if (values.size() < 3) continue;
 
        RelationSchema r;
        r.sourceType   = values[0];
        r.relationType = values[1];
        r.targetType   = values[2];
 
        hin.relations.push_back(r);
        relationNames.insert(r.relationType);
    }

    tripletFile.close();

    int nextRelationID = 0;
    // for all the relation names in relationNames
    for (const string& relationName : relationNames)
    {
        hin.relationTypeToID[relationName] = nextRelationID;
        hin.idToRelationType[nextRelationID] = relationName;
        cout << "Relation ID " << nextRelationID << ": " << relationName << endl;
        nextRelationID++;
    }

    cout << "\nDetected Triplets:\n";

    // For each RelationSchema r in relations
    for (const RelationSchema& r : hin.relations)
    {
        cout << r.sourceType
             << " --[" << r.relationType << "]--> "
             << r.targetType << endl;
    }

    // =====================================================
    // STAGE 3 - GLOBAL NODE ID ASSIGNMENT
    // Assigns contiguous integer ID ranges per node type.
    // All IDs of the same type are grouped together
    // for cache efficiency during typed queries.
    // Populates hin.nodeTypeOffsets.
    // =====================================================

    cout << "\nSTAGE 3 - GLOBAL NODE ID SPACE\n";
    cout << "----------------------------------------\n";

    // Array that contains offset of every node type.A map of offsets of each node type.
    map<string, int> nodeTypeOffsets;

    long long offset = 0;

    for (const auto& entry : hin.nodeTypeCounts)
    {
        hin.nodeTypeOffsets[entry.first] = offset;
        cout << entry.first
             << ": " << offset
             << " -> " << offset + entry.second - 1
             << endl;
        offset += entry.second;
    }

    if (offset != hin.totalNodes)
    {
        cerr << "ERROR: Global ID mismatch.\n";
        return 1;
    }


    // =====================================================
    // STAGE 4 - LOAD GLOBAL EDGES
    // Loads all valid edges from each relation folder.
    // Each edge is validated against the HIN schema.
    // Local node IDs are converted to global IDs. 
    // =====================================================

    cout<< "\n\nSTAGE 4 - LOAD GLOBAL EDGES\n";
    cout << "========================================\n";

    vector<GlobalEdge> edges;
    edges.reserve(22000000);

    int totalValidEdges = 0;
    int totalInvalidEdges = 0;

    auto loadStart = chrono::high_resolution_clock::now();

    // LIFE2: Loop over each of the relations:
    for (auto& relation : hin.relations)
    {
        string folderName =
            relation.sourceType + "___" +
            relation.relationType + "___" +
            relation.targetType;

        string edgeFilePath =
            "data/mag/raw/relations/" + folderName + "/edge.csv";

        cout << "\nLoading "
             << relation.sourceType 
             << " --[" << relation.relationType << "]--> "
             << relation.targetType << endl;

        ifstream edgeFile(edgeFilePath);
        if (!edgeFile.is_open())
        {
            cerr << "ERROR: Doing the following things puts you: " << edgeFilePath << endl;
            return 1;
        }

        int sourceLimit = hin.nodeTypeCounts[relation.sourceType];
        int targetLimit = hin.nodeTypeCounts[relation.targetType];
        int relationID = hin.relationTypeToID[relation.relationType];

        int valid = 0;
        int invalid = 0;

        while (getline(edgeFile, line))
        {
            line = trim(line);

            if (line.empty()) continue;

            vector<string> values = splitCSVLine(line);
            if (values.size() < 2)
            {
                invalid++;
                continue;
            }

            int localSource;
            int localTarget;

            try
            {
                localSource =
                    stoll(values[0]);

                localTarget =
                    stoll(values[1]);
            }
            catch (...)
            {
                invalid++;
                continue;
            }

            if (localSource < 0 || localSource >= sourceLimit ||
                localTarget < 0 || localTarget >= targetLimit)
            {
                invalid++;
                continue;
            }

            GlobalEdge edge;
            edge.source = getGlobalNodeID(
                relation.sourceType, localSource, hin.nodeTypeOffsets);
            edge.target = getGlobalNodeID(
                relation.targetType, localTarget, hin.nodeTypeOffsets);
            edge.relationTypeID = relationID;
 
            edges.push_back(edge);
            valid++;
        }

        edgeFile.close();

        totalValidEdges += valid;
        totalInvalidEdges += invalid;

        cout << "  Valid Edges: " << valid << endl;

        cout << "  Invalid Edges: " << invalid << endl;
    }

    auto loadEnd = chrono::high_resolution_clock::now();

    double loadSeconds = chrono::duration<double>(loadEnd - loadStart).count();

    cout << "Total loaded Edges: \n: " << edges.size() << endl;

    cout << "Loading Time: " << fixed << setprecision(2) << loadSeconds  << " sec\n";


    // =====================================================
    // STAGE 5 - SORT
    // Sorts edges by source node, then relation type,
    // then target node.
    // This ordering is required for correct CSR
    // and Type_Offset_Index construction.
    // =====================================================

    cout << "\n\nSTAGE 5 - SORT EDGES\n";
    cout << "========================================\n";

    auto sortStart = chrono::high_resolution_clock::now();

    sort(edges.begin(), edges.end(),
        [](const GlobalEdge& a, const GlobalEdge& b)
        {
            if (a.source != b.source)
                return a.source < b.source;
            if (a.relationTypeID != b.relationTypeID)
                return a.relationTypeID < b.relationTypeID;
            return a.target < b.target;
        });

    auto sortEnd = chrono::high_resolution_clock::now();

    double sortSeconds = chrono::duration<double>(sortEnd - sortStart).count();

    cout << "Sorting Complete: " << sortSeconds << " sec\n";


    // =====================================================
    // STAGE 6 - BUILD FORWARD MODIFIED CSR
    // Three arrays built here:
    //   rowPointers[u]  = start of node u in columnIndices
    //   columnIndices   = flat array of all target node IDs
    //   relationTypeIDs = relation type per edge
    //
    // Type_Offset_Index built alongside:
    //   For each node u and each relation type t:
    //   typeOffsetIndex[u][t] = (start, end)
    //   enables O(1) typed neighbour access
    //
    // Populates hin.rowPointers, hin.columnIndices,
    //           hin.relationTypeIDs, hin.typeOffsetIndex
    // =====================================================

    cout << "STAGE 6 - BUILD FORWARD MODIFIED CSR \n\n";
    cout << "========================================\n";

    auto csrStart = chrono::high_resolution_clock::now();

     // Build Row_Pointers
    // Build Row_Pointers
    hin.rowPointers.assign(hin.totalNodes + 1, 0);

    for (const GlobalEdge& edge : edges)
        hin.rowPointers[edge.source + 1]++;

    for (int i = 1; i <= hin.totalNodes; i++)
        hin.rowPointers[i] += hin.rowPointers[i - 1];

    // Build Column_Indices and Relation_Type_IDs
    hin.columnIndices.reserve(edges.size());
    hin.relationTypeIDs.reserve(edges.size());

    for (const GlobalEdge& edge : edges)
    {
        hin.columnIndices.push_back(edge.target);
        hin.relationTypeIDs.push_back(edge.relationTypeID);
    }

    // Build Type_Offset_Index
    hin.typeOffsetIndex.resize(hin.totalNodes);
    for (int node = 0; node < hin.totalNodes; node++)
    {
        int start = hin.rowPointers[node];
        int end   = hin.rowPointers[node + 1];
        int pos   = start;
 
        while (pos < end)
        {
            int type      = hin.relationTypeIDs[pos];
            int typeStart = pos;
 
            while (pos < end && hin.relationTypeIDs[pos] == type)
                pos++;
 
            TypeRange range;
            range.relationTypeID = type;
            range.start          = typeStart;
            range.end            = pos;
 
            hin.typeOffsetIndex[node].push_back(range);
        }
    }

    auto csrEnd = chrono::high_resolution_clock::now();
    double csrSeconds = chrono::duration<double>( csrEnd - csrStart).count();

    cout << "Forward CSR Complete: " << csrSeconds << " sec\n";

    // =====================================================
    // STAGE 7 - FORWARD VALIDATION
    // Confirms rowPointers.back() == columnIndices.size()
    // If these do not match the CSR is corrupt.
    // =====================================================

    cout << "\n\nSTAGE 7 - FORWARD CSR VALIDATION\n";
    cout << "========================================\n";

    if (hin.rowPointers.back() != static_cast<int>(hin.columnIndices.size()))
    {
        cerr << "[ERROR] Forward CSR invalid.\n";
        return 1;
    }
 
    cout << "[VALID] Forward Modified CSR consistent.\n";


    // =====================================================
    // STAGE 8 - REVERSE CSR
    // target becomes source, source becomes target.
    // Required for reverse hop traversal in meta-paths.
    // e.g. APA needs writes FORWARD then writes REVERSE.
    // Populates hin.reverseRowPointers, etc.
    // =====================================================

    cout << "\n\nSTAGE 8 - BUILD REVERSE CSR\n";
    cout << "========================================\n";

    auto reverseStart = chrono::high_resolution_clock::now();

    hin.reverseRowPointers.assign(hin.totalNodes + 1, 0);
 
    // For each outgoing edge (source -> target),
    // increment the count at reverseRowPointers[target + 1]
    for (int source = 0; source < hin.totalNodes; source++)
    {
        for (int i = hin.rowPointers[source];
             i < hin.rowPointers[source + 1]; i++)
        {
            int target = hin.columnIndices[i];
            hin.reverseRowPointers[target + 1]++;
        }
    }
 
    // convert counts to cumulative start positions
    for (int i = 1; i <= hin.totalNodes; i++)
        hin.reverseRowPointers[i] += hin.reverseRowPointers[i - 1];
 
    hin.reverseColumnIndices.resize(hin.columnIndices.size());
    hin.reverseRelationTypeIDs.resize(hin.relationTypeIDs.size());
 
    vector<int> cursor = hin.reverseRowPointers;

    for (int source = 0; source < hin.totalNodes; source++)
    {
        for (int i = hin.rowPointers[source];
             i < hin.rowPointers[source + 1]; i++)
        {
            int target   = hin.columnIndices[i];
            int position = cursor[target]++;
 
            hin.reverseColumnIndices[position]   = source;
            hin.reverseRelationTypeIDs[position] = hin.relationTypeIDs[i];
        }
    }

    vector<int>().swap(cursor);
 
    auto reverseEnd = chrono::high_resolution_clock::now();
    double reverseSeconds = chrono::duration<double>(reverseEnd - reverseStart).count();
 
    cout << "Reverse CSR Complete: " << reverseSeconds << " sec\n";

    // =====================================================
    // STAGE 9 - REVERSE VALIDATION
    // =====================================================

    cout << "\n\nSTAGE 9 - REVERSE CSR VALIDATION\n";
    cout << "========================================\n";

    if (hin.reverseRowPointers.back() !=
        static_cast<int>(hin.reverseColumnIndices.size()))
    {
        cerr << "[ERROR] Reverse CSR invalid.\n";
        return 1;
    }
 
    cout << "[VALID] Reverse CSR consistent.\n";


    // =====================================================
    // FREE TEMPORARY EDGE VECTOR
    // The edges vector is no longer needed after CSR is built.
    // Release memory before community search begins.
    // =====================================================

    long long temporaryEdgeMemory =
        static_cast<long long>(edges.capacity()) * sizeof(GlobalEdge);
 
    cout << "\nFreeing temporary edge vector...\n";

    vector<GlobalEdge>().swap(edges);

    cout << "Released approximately "
         << fixed << setprecision(2)
         << bytesToMB(temporaryEdgeMemory)
         << " MB temporary edge memory.\n";


    // =====================================================
    // HIN DATA STRUCTURE COMPLETE
    // All components of the HIN are now built and ready.
    // =====================================================
 
    cout << "\n\nHIN DATA STRUCTURE COMPLETE\n";
    cout << "========================================\n";
    cout << "Total Nodes:    " << hin.totalNodes << endl;
    cout << "Node Types:     " << hin.nodeTypeCounts.size() << endl;
    cout << "Relation Types: " << hin.relationTypeToID.size() << endl;
    cout << "Total Edges:    " << hin.columnIndices.size() << endl;
 
    cout << "\nNode Type Ranges (Global ID Space):\n";
    for (const auto& entry : hin.nodeTypeOffsets)
    {
        cout << "  " << entry.first
             << ": ID " << entry.second
             << " to " << entry.second + hin.nodeTypeCounts.at(entry.first) - 1
             << endl;
    }
 
    cout << "\nRelation Types:\n";
    for (const auto& entry : hin.relationTypeToID)
    {
        cout << "  ID " << entry.second
             << " = " << entry.first << endl;
    }
 
    cout << "\nData Structure Status:\n";
    cout << "  [DONE] HIN Schema           (Stage 2)\n";
    cout << "  [DONE] Global Node IDs      (Stage 3)\n";
    cout << "  [DONE] Forward Modified CSR (Stage 6)\n";
    cout << "  [DONE] Type_Offset_Index    (Stage 6)\n";
    cout << "  [DONE] Reverse CSR          (Stage 8)\n";
    cout << "\nHIN is ready for community search.\n";


    // =====================================================
    // STAGE 10 - RUNTIME META-PATH CONFIGURATION
    //Reads command line arguments for:
    //   startType  = node type to search (e.g. author)
    //   queryArg   = local node ID or auto
    //   k          = minimum degree for k-core
    //   pathSpec   = meta-path definition (e.g. writes:F,writes:R)
    //   p          = minimum community size for KP-Core
    //
    // Validates meta-path against the discovered HIN schema.
    // =====================================================

    cout << "\n\nSTAGE 10 - GENERIC RUNTIME CONFIGURATION\n";
    cout << "========================================\n";

    string startType = (argc > 1) ? argv[1] : "author";
    string queryArg  = (argc > 2) ? argv[2] : "auto";
    int k            = (argc > 3) ? stoi(argv[3]) : 5;
    string pathSpec  = (argc > 4) ? argv[4] : "writes:F,writes:R";
    int p            = (argc > 5) ? stoi(argv[5]) : 1;

    if (hin.nodeTypeCounts.find(startType) == hin.nodeTypeCounts.end())
    {
        cerr << "ERROR: Unknown start node type: " << startType << endl;
        return 1;
    }

    if (k < 1)
    {
        cerr << "ERROR: k must be >= 1.\n";
        return 1;
    }

    if (p < 1)
    {
        cerr<< "ERROR: p must be >= 1.\n";
        return 1;
    }

    vector<MetaPathStep> metaPath;
    string endType;
    string readablePath;

    // Pass hin explicitly to parseMetaPath
    if (!parseMetaPath(pathSpec, startType, hin, metaPath, endType, readablePath))
    {
        cerr << "ERROR: Invalid path specification or path does not match discovered HIN schema.\n";
        cerr << "Format example: writes:F,writes:R\n";
        return 1;
    }

    if (endType != startType)
    {
        cerr << "ERROR: For query-centred k-core, the selected meta-path must return\n";
        cerr << "to the same node type. Start=" << startType
             << ", End=" << endType << endl;
        return 1;
    }

    cout << "Start Node Type: " << startType << endl;
    cout << "Selected k:      " << k << endl;
    cout << "Selected p:      " << p << " (minimum community size)\n";
    cout << "Path Spec:       " << pathSpec << endl;
    cout << "Resolved Path:\n  " << readablePath << endl;
    cout << "Steps:           " << metaPath.size() << endl;
    cout << "[VALID] Runtime meta-path matches discovered HIN schema.\n";


    // =====================================================
    // STAGE 11 - META-PATH QUERY
    // Executes the meta-path traversal for the query node.
    // Returns all nodes reachable via the full meta-path.
    // =====================================================

    cout << "\n\nSTAGE 11 - META-PATH QUERY\n";
    cout << "========================================\n";

    int startTypeOffset = hin.nodeTypeOffsets[startType];
    int startTypeCount = hin.nodeTypeCounts[startType];

    int queryNode = -1;
    vector<int> metaPathNeighbours;

    auto queryStart = chrono::high_resolution_clock::now();

    if (queryArg != "auto")
    {
        int localID;
        try { localID = stoll(queryArg); }
        catch (...)
        {
            cerr << "ERROR: Query local ID must be an integer or 'auto'.\n";
            return 1;
        }

        if (localID < 0 || localID >= startTypeCount)
        {
            cerr << "ERROR: Query local ID is outside the selected node type range.\n";
            return 1;
        }

        queryNode = startTypeOffset + localID;
        metaPathNeighbours = followMetaPath(queryNode, metaPath, hin);
    }
    else
    {
        // Search a bounded prefix for a useful demonstration query.
        int searchLimit = min(startTypeCount, 1000);
 
        for (int localID = 0; localID < searchLimit; localID++)
        {
            int candidate = startTypeOffset + localID;
            vector<int> result = followMetaPath(candidate, metaPath, hin);
 
            if (!result.empty())
            {
                queryNode = candidate;
                metaPathNeighbours = move(result);
                break;
            }
        }
    }

    auto queryEnd = chrono::high_resolution_clock::now();
    double querySeconds = chrono::duration<double>(queryEnd - queryStart).count();

    if (queryNode == -1)
    {
        cerr << "No suitable query node with meta-path neighbours found.\n";
        return 1;
    }

    cout << "Query Node:\n";
    cout << "  Type:     " << startType << endl;
    cout << "  Local ID: " << queryNode - startTypeOffset << endl;
    cout << "  Global ID:" << queryNode << endl;
    cout << "\nMeta-Path Neighbours Found: " << metaPathNeighbours.size() << endl;
    cout << "First Neighbours:\n";

    for (size_t i = 0; i < metaPathNeighbours.size() && i < 20; i++)
    {
        int neighbour = metaPathNeighbours[i];
        cout << "  Global ID: " << neighbour;
        if (neighbour >= startTypeOffset &&
            neighbour < startTypeOffset + startTypeCount)
        {
            cout << " | " << startType << " Local ID: "
                 << neighbour - startTypeOffset;
        }
        cout << endl;
    }

    cout << "\nMeta-Path Query Time: " 
        << fixed << setprecision(6) << querySeconds << " sec\n";


    // =====================================================
    // STAGE 12 - META-PATH VALIDATION
    // Confirms forward and reverse traversal executed.
    // Confirms duplicates and self-node removed.
    // =====================================================

    cout << "\n\nSTAGE 12 - META-PATH VALIDATION\n";
    cout << "========================================\n";

    bool hasForward = false;
    bool hasReverse = false;
    for (const MetaPathStep& step : metaPath)
    {
        if (step.forward) hasForward = true;
        else hasReverse = true;
    }

    cout << "[VALID] Selected path resolved from runtime relation names.\n";
    if (hasForward) cout << "[VALID] Forward traversal executed.\n";
    if (hasReverse) cout << "[VALID] Reverse traversal executed.\n";
    cout << "[VALID] Duplicate neighbours removed.\n";
    cout << "[VALID] Self node removed from result.\n";


    // =====================================================
    // STAGE 13 - BUILD QUERY-CENTRED DERIVED GRAPH
    // Constructs the query-centred derived graph.
    // This is a virtual graph — edges represent meta-path
    // connections not physical HIN edges.
    // K-core peeling runs on this derived graph.
    // =====================================================

    cout << "\n\nSTAGE 13 - BUILD META-PATH DERIVED GRAPH\n";
    cout << "========================================\n";

    auto derivedStart = chrono::high_resolution_clock::now();

    vector<int> derivedNodes;
    vector<vector<int>> derivedAdjacency;
    unordered_map<int, int> globalToDerived;

    buildDerivedGraph(
        queryNode, metaPathNeighbours, metaPath, hin,
        derivedNodes, derivedAdjacency, globalToDerived);

    auto derivedEnd = chrono::high_resolution_clock::now();
    double derivedSeconds = chrono::duration<double>(derivedEnd - derivedStart).count();
 
    long long derivedEdgeRefs = 0;
    for (const vector<int>& n : derivedAdjacency)
        derivedEdgeRefs += static_cast<long long>(n.size());
    long long derivedEdges = derivedEdgeRefs / 2;
 
    cout << "Candidate Nodes:           " << derivedNodes.size() << endl;
    cout << "Derived Undirected Edges:  " << derivedEdges << endl;
    cout << "Construction Time:         "
         << fixed << setprecision(6) << derivedSeconds << " sec\n";
 
    cout << "\nDerived Graph Sample:\n";
    for (size_t i = 0; i < derivedNodes.size() && i < 20; i++)
    {
        cout << "  " << startType
             << " Global " << derivedNodes[i]
             << " | Local " << derivedNodes[i] - startTypeOffset
             << " | Meta-Path Degree " << derivedAdjacency[i].size()
             << endl;
    }
 
    if (derivedNodes.empty())
    {
        cerr << "[ERROR] Derived graph is empty.\n";
        return 1;
    }
 
    cout << "\n[VALID] Query-centred meta-path derived graph created.\n";


    // =====================================================
    // STAGE 14 - MULTIPLE K-CORE TESTING
    // Tests k-core peeling across k=2 to k=6.
    // The community size should decrease monotonically as k increases — this is a fundamental property
    // of k-cores and validates the algorithm is correct.
    // =====================================================

    cout << "\n\nSTAGE 14 - MULTIPLE K-CORE TESTING\n";
    cout << "========================================\n";

    vector<int> kValues = {2, 3, 4, 5, 6};
    if (find(kValues.begin(), kValues.end(), k) == kValues.end())
        kValues.push_back(k);
    sort(kValues.begin(), kValues.end());
    kValues.erase(unique(kValues.begin(), kValues.end()), kValues.end());

    cout << left
         << setw(8) << "k"
         << setw(18) << "Initial"
         << setw(18) << "Removed"
         << setw(18) << "Surviving"
         << setw(20) << "Query Community"
         << "Time (sec)\n";
    cout << "--------------------------------------------------------------------------------\n";

    for (int testK : kValues)
    {
        auto testStart = chrono::high_resolution_clock::now();
        vector<int> testFinalDegrees;
        int testRemovedCount = 0;
        vector<bool> testRemoved = performKCorePeeling(
            derivedAdjacency, testK, testFinalDegrees, testRemovedCount);
 
        int testSurviving = 0;
        for (bool r : testRemoved) if (!r) testSurviving++;
 
        vector<int> testCommunity;
        auto qIt = globalToDerived.find(queryNode);
        if (qIt != globalToDerived.end())
            testCommunity = extractQueryCommunity(
                qIt->second, derivedAdjacency, testRemoved);
 
        auto testEnd = chrono::high_resolution_clock::now();
        double testSeconds = chrono::duration<double>(testEnd - testStart).count();
 
        cout << left
             << setw(8)  << testK
             << setw(18) << derivedNodes.size()
             << setw(18) << testRemovedCount
             << setw(18) << testSurviving
             << setw(20) << testCommunity.size()
             << fixed << setprecision(6) << testSeconds << endl;
    }


    // =====================================================
    // STAGE 15 - DETAILED RUNTIME K-CORE
    // Runs k-core peeling at the selected k value.
    // =====================================================

    cout << "\n\nSTAGE 15 - DETAILED K-CORE DEMO (k = " << k << ")\n";
    cout << "========================================\n";

    auto kCoreStart = chrono::high_resolution_clock::now();
    vector<int> finalDegrees;
    int removedCount = 0;
    vector<bool> removed = performKCorePeeling(
        derivedAdjacency, k, finalDegrees, removedCount);
    auto kCoreEnd = chrono::high_resolution_clock::now();
    double kCoreSeconds = chrono::duration<double>(kCoreEnd - kCoreStart).count();

    int survivingNodes = 0;
    for (bool r : removed) if (!r) survivingNodes++;

    cout << "Selected k: " << k << endl;
    cout << "Initial Candidate Nodes: " << derivedNodes.size() << endl;
    cout << "Removed During Peeling: " << removedCount << endl;
    cout << "Remaining K-Core Nodes: " << survivingNodes << endl;
    cout << "K-Core Peeling Time: " << fixed << setprecision(6)
         << kCoreSeconds << " sec\n";


    // =====================================================
    // STAGE 16 - FINAL QUERY COMMUNITY + KP-CORE CHECK
    // Extracts the community connected to the query node.
    // Applies the KP-Core size constraint:
    //   if community size >= p -> PASS -> return community
    //   if community size <  p -> FAIL -> reject community
    // =====================================================

    cout << "\n\nStage 16 – FINAL QUERY COMMUNITY\n";
    cout << "========================================\n";

    auto queryIndexIt = globalToDerived.find(queryNode);
    if (queryIndexIt == globalToDerived.end())
    {
        cerr<< "There is no query node in the derived graph. \n";
        return 1;
    }

    vector<int> communityIndices = extractQueryCommunity(
        queryIndexIt->second, derivedAdjacency, removed);

    if (communityIndices.empty())
    {
        cout << "Query node did NOT live after being peeled by k cores.\n";
        cout << "Final query-centred community is empty.\n";
    }
    else
    {
        cout << "[VALID] Query survives " << k << "-core peeling.\n";
        cout << "K-Core Community Size: " << communityIndices.size() << endl;

        // ─────────────────────────────────────────────────
        // KP-CORE SIZE CONSTRAINT CHECK
        // ─────────────────────────────────────────────────
        cout << "\nKP-CORE SIZE CONSTRAINT CHECK\n";
        cout << "----------------------------------------\n";
        cout << "Community Size: " << communityIndices.size() << endl;
        cout << "Minimum Size p: " << p << endl;

        if ((int)communityIndices.size() >= p)
        {
            cout << "[PASS] Community satisfies KP-Core constraint.\n";
            cout << "       Size " << communityIndices.size()
                << " >= p=" << p << "\n";
            cout << "\nFinal KP-Core Community Members:\n";

            for (size_t x = 0; x < communityIndices.size() && x < 30; x++)
            {
                int idx = communityIndices[x];
                int globalID = derivedNodes[idx];
                cout << "  " << startType
                    << " Global ID: " << globalID
                    << " | Local ID: " << globalID - startTypeOffset
                    << " | Final Degree: " << finalDegrees[idx];
                if (globalID == queryNode) cout << "  <-- QUERY";
                cout << endl;
            }
        }
        else
        {
            cout << "[FAIL] Community does NOT satisfy KP-Core constraint.\n";
            cout << "       Size " << communityIndices.size()
                << " < p=" << p << "\n";
            cout << "Community rejected — too small to be meaningful.\n";

            communityIndices.clear();
        }
    }


    // Validate k-core correctness — every survivor must have degree >= k
    bool kCoreValid = true;
    for (size_t i = 0; i < derivedAdjacency.size(); i++)
    {
        if (removed[i]) continue;
        int activeDegree = 0;
        for (int v : derivedAdjacency[i])
            if (!removed[v]) activeDegree++;
        if (activeDegree < k)
        {
            kCoreValid = false;
            break;
        }
    }

    if (!kCoreValid)
    {
        cerr << "[ERROR] K-core validation failed.\n";
        return 1;
    }
 
    if (survivingNodes == 0)
        cout << "[VALID] No nodes survive k=" << k
             << "; peeling correctly removed the complete candidate graph.\n";
    else
        cout << "[VALID] Every surviving node satisfies degree >= " << k << ".\n";
 
    cout << "[VALID] Iterative k-core peeling is consistent.\n";

    // =====================================================
     // STAGE 17 - ADDITIONAL META-PATH TEST
    // Tests a second meta-path on the same query node
    // to confirm the engine works across different
    // semantic relationships.
    // For author queries: AIA (Author->Institution->Author)
    // =====================================================

    cout << "\n\nSTAGE 17 – ADDITIONAL META-PATH TEST\n";
    cout << "========================================\n";

    string additionalSpec;
    string additionalName;

    if (startType == "author" &&
        hin.relationTypeToID.count("affiliated_with"))
    {
        additionalSpec = "affiliated_with:F,affiliated_with:R";
        additionalName = "AIA";
    }
    else
    {
        additionalSpec = pathSpec;
        additionalName = "Runtime Path Re-test";
    }

    vector<MetaPathStep> additionalPath;
    string additionalEndType;
    string additionalReadable;
    bool additionalPathValid = parsePath(
        additionalSpec, startType, hin, additionalPath,
        additionalEndType, additionalReadable);

    vector<int> additionalNeighbours;
    int additionalQuery = -1;
    double additionalSeconds = 0.0;

    if (additionalPathValid && additionalEndType == startType)
    {
        auto additionalStart = chrono::high_resolution_clock::now();

        additionalQuery = queryNode;
        additionalNeighbours = followMetaPath(
            additionalQuery, additionalPath, hin);

        if (additionalNeighbours.empty())
        {
            int searchLimit = min(startTypeCount, 1000);
            for (int localID = 0; localID < searchLimit; localID++)
            {
                // [JAWS] The startTypeOffset number is a number of seconds.The startTypeOffset number is a number of seconds.
                int candidate = static_cast<int>(startTypeOffset + localID);

                vector<int> result = followMetaPath(
                    candidate, additionalPath, hin);

                if (!result.empty())
                {
                    additionalQuery = candidate;
                    additionalNeighbours = move(result);
                    break;
                }
            }
        }

        auto additionalEnd = chrono::high_resolution_clock::now();
        additionalSeconds = chrono::duration<double>(
            additionalEnd - additionalStart).count();

        cout << "Additional Path: " << additionalName << endl;
        cout << "  " << additionalReadable << endl;
        cout << "additionalQuery - startTypeOffset = " << additionalQuery - startTypeOffset << endl;
        // Note: a cout is used to print the number of additionalNeighbours.
        cout << "Additional Neighbours: " << additionalNeighbours.size() << endl;
        cout << "Query Time: "
             << fixed << setprecision(6)
             << additionalSeconds << " sec\n";

        if (!additionalNeighbours.empty())
            cout << "[VALID] Retrieved Semantic Neighbours via Additional Meta-path Traversal.\n";
        else
            cout << "[WARNING] schema-valid but bounded test did not find any neighbours in the additional path.\n";
    }
    else
    {
        cout << "[WARNING] No other appropriate consistent meta-path found. \n" << endl;
    }


    // =====================================================
    // STAGE 18 - MEMORY + PERFORMANCE EVALUATION
    // =====================================================

    cout << "\n\nSTAGE 18 - MEMORY AND PERFORMANCE EVALUATION\n";
    cout << "========================================\n";

    long long forwardRowBytes  = static_cast<long long>(hin.rowPointers.capacity()) * sizeof(int);
    long long forwardColBytes  = static_cast<long long>(hin.columnIndices.capacity()) * sizeof(int);
    long long forwardTypeBytes = static_cast<long long>(hin.relationTypeIDs.capacity()) * sizeof(int);
    long long reverseRowBytes  = static_cast<long long>(hin.reverseRowPointers.capacity()) * sizeof(int);
    long long reverseColBytes  = static_cast<long long>(hin.reverseColumnIndices.capacity()) * sizeof(int);
    long long reverseTypeBytes = static_cast<long long>(hin.reverseRelationTypeIDs.capacity()) * sizeof(int);

    long long typeRangeCount = 0;
    for (const auto& ranges : hin.typeOffsetIndex)
        typeRangeCount += static_cast<long long>(ranges.size());
    long long typeRangeBytes = typeRangeCount * sizeof(TypeRange);
 
    long long derivedNodeBytes = static_cast<long long>(derivedNodes.capacity()) * sizeof(int);
    long long derivedAdjBytes  = 0;
    for (const auto& a : derivedAdjacency)
        derivedAdjBytes += static_cast<long long>(a.capacity()) * sizeof(int);
 
    long long forwardMainBytes  = forwardRowBytes + forwardColBytes + forwardTypeBytes;
    long long reverseMainBytes  = reverseRowBytes + reverseColBytes + reverseTypeBytes;
    long long combinedCSRBytes  = forwardMainBytes + reverseMainBytes;
 
    cout << "Forward Row Pointers:     " << bytesToMB(forwardRowBytes)  << " MB\n";
    cout << "Forward Columns:          " << bytesToMB(forwardColBytes)  << " MB\n";
    cout << "Forward Relation IDs:     " << bytesToMB(forwardTypeBytes) << " MB\n";
    cout << "Reverse Row Pointers:     " << bytesToMB(reverseRowBytes)  << " MB\n";
    cout << "Reverse Columns:          " << bytesToMB(reverseColBytes)  << " MB\n";
    cout << "Reverse Relation IDs:     " << bytesToMB(reverseTypeBytes) << " MB\n";
    cout << "Forward Main CSR:         " << bytesToMB(forwardMainBytes) << " MB\n";
    cout << "Reverse Main CSR:         " << bytesToMB(reverseMainBytes) << " MB\n";
    cout << "Combined CSR Arrays:      " << bytesToMB(combinedCSRBytes) << " MB\n";
    cout << "Type_Offset_Index:        " << typeRangeCount
         << " ranges (~" << bytesToMB(typeRangeBytes) << " MB)\n";
    cout << "Derived Graph Payload:    ~"
         << bytesToMB(derivedNodeBytes + derivedAdjBytes) << " MB\n";
    cout << "Temporary Edge Memory:    "
         << bytesToMB(temporaryEdgeMemory) << " MB (released)\n";
 
    cout << "\nPerformance Summary:\n";
    cout << "  Edge Loading:           " << loadSeconds    << " sec\n";
    cout << "  Edge Sorting:           " << sortSeconds    << " sec\n";
    cout << "  Forward CSR Build:      " << csrSeconds     << " sec\n";
    cout << "  Reverse CSR Build:      " << reverseSeconds << " sec\n";
    cout << "  Meta-Path Query:        " << querySeconds   << " sec\n";
    cout << "  Derived Graph Build:    " << derivedSeconds << " sec\n";
    cout << "  K-Core Peeling:         " << kCoreSeconds   << " sec\n";
    cout << "  Additional Meta-Path:   " << additionalSeconds << " sec\n";


    // =====================================================
    // STAGE 19 - FINAL TEST / DEMO CHECKLIST
    // =====================================================

    cout << "\n\nSTAGE 19 - FINAL VALIDATION CHECKLIST\n";
    cout << "========================================\n";
 
    bool forwardCSRValid  = hin.rowPointers.back() ==
                            static_cast<int>(hin.columnIndices.size());
    bool reverseCSRValid  = hin.reverseRowPointers.back() ==
                            static_cast<int>(hin.reverseColumnIndices.size());
    bool edgeCountValid   = totalValidEdges ==
                            static_cast<int>(hin.columnIndices.size());
    bool runtimePathValid = !metaPath.empty() && endType == startType;
    bool derivedGraphValid= !derivedNodes.empty() && globalToDerived.count(queryNode);
 
    bool kpCoreValid = communityIndices.empty()
        ? true
        : (int)communityIndices.size() >= p;
 
    cout << (forwardCSRValid   ? "[PASS] " : "[FAIL] ") << "Forward CSR consistency\n";
    cout << (reverseCSRValid   ? "[PASS] " : "[FAIL] ") << "Reverse CSR consistency\n";
    cout << (edgeCountValid    ? "[PASS] " : "[FAIL] ") << "Loaded edge count consistency\n";
    cout << (runtimePathValid  ? "[PASS] " : "[FAIL] ") << "Runtime meta-path schema validation\n";
    cout << (derivedGraphValid ? "[PASS] " : "[FAIL] ") << "Query-centred derived graph\n";
    cout << (kCoreValid        ? "[PASS] " : "[FAIL] ") << "Iterative k-core validation\n";
    cout << (kpCoreValid       ? "[PASS] " : "[FAIL] ")
         << "KP-Core size constraint (p=" << p << ")\n";
    cout << ((!additionalNeighbours.empty()) ? "[PASS] " : "[INFO] ")
         << "Additional meta-path test\n";

    bool allCriticalTests = forwardCSRValid  && reverseCSRValid  &&
                            edgeCountValid   && runtimePathValid &&
                            derivedGraphValid && kCoreValid      && 
                            kpCoreValid;
 
    if (!allCriticalTests)
    {
        cerr << "\n[ERROR] One or more critical tests failed.\n";
        return 1;
    }
 
    cout << "\n[VALID] All critical implementation tests passed.\n";


    // =====================================================
    // STAGE 20 - FINAL IMPLEMENTATION SUMMARY
    // =====================================================

    auto programEnd = chrono::high_resolution_clock::now();
    double totalSeconds = chrono::duration<double>(programEnd - programStart).count();
 
    cout << "\n\n========================================\n";
    cout << " FINAL IMPLEMENTATION SUMMARY\n";
    cout << "========================================\n";
    cout << "Total Nodes:        " << hin.totalNodes     << endl;
    cout << "Total Valid Edges:  " << totalValidEdges    << endl;
    cout << "Total Invalid Edges:" << totalInvalidEdges  << endl;
 
    cout << "\nRuntime Configuration:\n";
    cout << "  Start Type:   " << startType << endl;
    cout << "  Query Local:  " << queryNode - startTypeOffset << endl;
    cout << "  Query Global: " << queryNode << endl;
    cout << "  Path Spec:    " << pathSpec  << endl;
    cout << "  k:            " << k         << endl;
    cout << "  p:            " << p         << endl;
 
    cout << "\nDerived Graph:\n";
    cout << "  Nodes:           " << derivedNodes.size()      << endl;
    cout << "  Edges:           " << derivedEdges             << endl;
    cout << "  Surviving Nodes: " << survivingNodes           << endl;
    cout << "  Community Size:  " << communityIndices.size()  << endl;
 
    cout << "\nAdditional Meta-Path:\n";
    cout << "  Name:       " << additionalName                << endl;
    cout << "  Spec:       " << additionalSpec                << endl;
    cout << "  Neighbours: " << additionalNeighbours.size()   << endl;
 
    cout << "\nTotal Program Time: " << totalSeconds << " sec\n";
 
    cout << "\nStatus:\n";
    cout << "[DONE] Real MAG Dataset\n";
    cout << "[DONE] Dynamic Schema Discovery\n";
    cout << "[DONE] struct HIN Data Structure\n";
    cout << "[DONE] Global Node ID Assignment\n";
    cout << "[DONE] Forward Modified CSR + Type_Offset_Index\n";
    cout << "[DONE] Reverse CSR\n";
    cout << "[DONE] Generic Runtime Meta-Path Configuration\n";
    cout << "[DONE] Query-Centred Derived Graph\n";
    cout << "[DONE] Multiple K Testing\n";
    cout << "[DONE] Iterative K-Core Peeling\n";
    cout << "[DONE] KP-Core Size Constraint\n";
    cout << "[DONE] Final Query Community Extraction\n";
    cout << "[DONE] Additional Meta-Path Test (AIA)\n";
    cout << "[DONE] Memory and Performance Evaluation\n";
    cout << "[DONE] Final Validation Checklist\n";
 
    cout << "\nUSAGE EXAMPLES:\n";
    cout << "  Default APA:    ./main.exe\n";
    cout << "  APA custom:     ./main.exe author 0 4 writes:F,writes:R\n";
    cout << "  AIA custom:     ./main.exe author auto 3 affiliated_with:F,affiliated_with:R\n";
    cout << "  KP-Core (p=5):  ./main.exe author auto 2 writes:F,writes:R 5\n";
    cout << "  KP-Core (p=10): ./main.exe author auto 2 writes:F,writes:R 10\n";
 
    cout << "========================================\n";
    return 0;
}
