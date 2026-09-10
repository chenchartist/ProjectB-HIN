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
// HELPERS 
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
 
double bytesToMB(long long bytes) 
{ 
    return static_cast<double>(bytes) / 
           (1024.0 * 1024.0); 
} 
 
 
// ========================================================= 
// DATA STRUCTURES 
// ========================================================= 
 
struct RelationSchema 
{ 
    string sourceType; 
    string relationType; 
    string targetType; 
}; 
 
struct GlobalEdge 
{ 
    int source; 
    int target; 
    int relationTypeID; 
}; 
 
struct TypeRange 
{ 
    int relationTypeID; 
    long long start; 
    long long end; 
}; 
 
struct MetaPathStep 
{ 
    int relationTypeID; 
 
    // true  = forward CSR 
    // false = reverse CSR 
    bool forward; 
}; 
 
 
// ========================================================= 
// A node ID from a specific node in a network to a global node ID. 
// ========================================================= 
 
long long getGlobalNodeID( 
    const string& nodeType, 
    long long localID, 
    const map<string, long long>& nodeTypeOffsets)
{ 
    auto it = nodeTypeOffsets.find(nodeType); 
 
    if (it == nodeTypeOffsets.end()) 
        return -1; 
 
    return it->second + localID; 
} 
 
 
// ========================================================= 
// META-PATH TRAVERSAL 
// ========================================================= 
 
vector<int> followMetaPath( 
    int startNode, 
    const vector<MetaPathStep>& path, 
    const vector<long long>& rowPointers,
    const vector<int>& columnIndices, 
    const vector<int>& relationTypeIDs, 
    const vector<long long>& reverseRowPointers,
    const vector<int>& reverseColumnIndices, 
    const vector<int>& reverseRelationTypeIDs) 
{ 
    vector<int> currentNodes; 
    currentNodes.push_back(startNode); 
 
    for (const auto step: path) 
    { 
        unordered_set<int> nextSet; 
 
        for (int node : currentNodes) 
        { 
            if (step.forward) 
            { 
                long long start = 
                    rowPointers[node]; 
 
                long long end = 
                    rowPointers[node + 1]; 
 
                for (long long i = start; 
                     i < end; 
                     i++) 
                { 
                    if (relationTypeIDs[i] == 
                        step.relationTypeID) 
                    { 
                        nextSet.insert( 
                            columnIndices[i]); 
                    } 
                } 
            } 
            else 
            { 
                long long start = 
                    reverseRowPointers[node]; 
 
                long long end = 
                    reverseRowPointers[node + 1]; 
 
                for (long long i = start; 
                     i < end; 
                     i++) 
                { 
                    if (reverseRelationTypeIDs[i] == 
                        step.relationTypeID) 
                    { 
                        nextSet.insert( 
                            reverseColumnIndices[i]); 
                    } 
                } 
            } 
        } 
 
        currentNodes.assign( 
            nextSet.begin(), 
            nextSet.end()); 
 
        if (currentNodes.empty()) 
            break; 
    } 
 
    // if the metapath is the same type as the start node, remove the start node from the graph. 
    currentNodes.erase( 
        remove( 
            currentNodes.begin(), 
            currentNodes.end(), 
            startNode), 
        currentNodes.end()); 
 
    sort( 
        currentNodes.begin(), 
        currentNodes.end()); 
 
    return currentNodes; 
} 
 
 
 
// ========================================================= 
// We build query-centred meta-path derived graph.We construct query-centred Meta-path derived graph. 
// ========================================================= 
 
void buildDerivedGraph( 
    int queryNode, 
    const vector<int>& queryNeighbours, 
    const vector<MetaPathStep>& metaPath, 
    const vector<long long>& rowPointers,
    const vector<int>& columnIndices, 
    const vector<int>& relationTypeIDs, 
    const vector<long long>& reverseRowPointers,
    const vector<int>& reverseColumnIndices, 
    const vector<int>& reverseRelationTypeIDs, 
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
    derivedNodes.erase(unique(derivedNodes.begin(), derivedNodes.end()), derivedNodes.end()); 
 
    globalToDerived.clear(); 
    for (size_t i = 0; i < derivedNodes.size(); i++) 
        globalToDerived[derivedNodes[i]] = static_cast<int>(i); 
 
    vector<unordered_set<int>> adjacencySets(derivedNodes.size()); 
 
    for (size_t i = 0; i < derivedNodes.size(); i++) 
    { 
        int globalNode = derivedNodes[i]; 
 
        vector<int> neighbours = followMetaPath( 
            globalNode, 
            metaPath, 
            rowPointers, 
            columnIndices, 
            relationTypeIDs, 
            reverseRowPointers, 
            reverseColumnIndices, 
            reverseRelationTypeIDs); 
 
        // Each neighbour in neighbours do the following: 
        for (int globalNeighbour : neighbours)
        { 
            auto it = globalToDerived.find(globalNeighbour); 
            if (it == globalToDerived.end()) 
                continue; 
 
            int neighbourIndex = it->second; 
            if (neighbourIndex == static_cast<int>(i)) 
                continue; 
 
            // The derived graph is assumed to be undirected if the graph is symmetric, as in the case of a graph from APA. 
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
// Locating the k-core community for a query.
// =========================================================

vector<int> extractQueryCommunity(
    int queryIndex,
    const vector<vector<int>>& adjacency,
    const vector<bool>& removed)
{
    vector<int> community;

    // This question is likely the most frequent asked in a project.Perhaps you cannot separate the question of "why" from the question of "how".
    if (queryIndex < 0 || queryIndex >= static_cast<int>(adjacency.size()))
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
            if (removed[v])
                continue;

            if (!visited[v])
            {
                visited[v] = true;
                Q.push(v);
            }
        }
    }

    return community;
}


// =========================================================
// MAIN
// =========================================================

int main( int argc, char* argv[] )
{
    auto programStart =
        chrono::high_resolution_clock::now();

    cout << "========================================\n";
    cout <<" HIN COMMUNITY SEARCH SYSTEM\n ";
    printf(" Modified CSR + Meta-Path Engine\n");
    cout << "========================================\n\n";


    // =====================================================
    // STAGE 1 - NODE TYPE DISCOVERY
    // =====================================================

    cout << "STAGE 1 - NODE TYPE DISCOVERY\n";
    cout << "----------------------------------------\n";

    string nodeFilePath =
        "data/mag/raw/num-node-dict.csv";

    ifstream nodeFile(nodeFilePath);

    if (!nodeFile.is_open())
    {
        // std::cerr is used to show errors.cerr is used to display errors.
        cerr << "ERROR: Could not open "
             << nodeFilePath
             << endl;

        return 1;
    }

    string line;

    if (!getline(nodeFile, line))
    {
        // Using cout instead of cerr: cout << "ERROR: Node type row missing. \n";
        cout << "ERROR: Node type row missing.\n";
        return 1;
    }

    vector<string> nodeTypeNames =
        splitCSVLine(line);

    if (!getline(nodeFile, line))
    {
        cout << "ERROR: Node count row is missing.\n";
        return 1;
    }

    vector<string> nodeCounts =
        splitCSVLine(line);

    nodeFile.close();

    if (nodeTypeNames.size() !=
        nodeCounts.size())
    {
        cout << "ERROR: Non-matching type and count of the nodes .\n";
        return 1;
    }

    map< string, long long > nodeTypeCounts;

    for (size_t i = 0;
         i < nodeTypeNames.size();
         i++)
    {
        nodeTypeCounts[nodeTypeNames[i]] =
            stoll(nodeCounts[i]);
    }

    long long totalNodes = 0;

    // For each of the entries in: nodeTypeCounts
    for (const auto& entry : nodeTypeCounts)
    {
        cout << entry.first
             << " -> "
             << entry.second
             << " nodes\n";

        totalNodes +=
            entry.second;
    }

    cout << "\nTotal Nodes: "
         << totalNodes
         << endl;


    // =====================================================
    // STAGE 2 - SCHEMA DISCOVERY
    // =====================================================

    cout << "\n\nSTAGE 2 - HIN SCHEMA DISCOVERY\n";
    cout << "----------------------------------------\n";

    string tripletFilePath =
        "data/mag/raw/triplet-type-list.csv";

    ifstream tripletFile(
        tripletFilePath);

    if (!tripletFile.is_open())
    {
        // std::cerr is used to show errors.cerr is used to display errors.
        cerr << "ERROR: Could not open "
             << tripletFilePath
             << endl;

        return 1;
    }

    vector<RelationSchema> relations;
    set<string> relationNames;

    while (getline(tripletFile, line))
    {
        line = trim(line);

        if (line.empty())
            continue;

        vector<string> values =
            splitCSVLine(line);

        if (values.size() < 3)
            continue;

        RelationSchema r;

        r.sourceType =
            values[0];

        r.relationType =
            values[1];

        r.targetType =
            values[2];

        relations.push_back(r);

        relationNames.insert(
            r.relationType);
    }

    tripletFile.close();

    map<string, int> relationTypeToID;
    map<int, string> idToRelationType;

    int nextRelationID = 0;

    // for all the relation names in relationNames
    for (const string& relationName : relationNames)
    {
        relationTypeToID[
            relationName] =
            nextRelationID;

        idToRelationType[
            nextRelationID] =
            relationName;

        cout << "Relation ID "
             << nextRelationID
             << ": "
             << relationName
             << endl;

        nextRelationID++;
    }

    cout << "\nDetected Triplets:\n";

    // For each RelationSchema r in relations
    for (const RelationSchema& r : relations)
    {
        cout << r.sourceType
             << " --["
             << r.relationType
             << "]--> "
             << r.targetType
             << endl;
    }

    // =====================================================
    // To incorporate an ability to globally determine the node Ids.To be able to identify nodeIDs around the world.
    // =====================================================

    cout << "    STAGE 3 - GLOBAL NODE ID SPACE\n";
    cout << "----------------------------------------\n";

    // Array that contains offset of every node type.A map of offsets of each node type.
    map<string, long long> nodeTypeOffsets;

    long long offset = 0;

    for (auto& entry :
         nodeTypeCounts)
    {
        nodeTypeOffsets[
            entry.first] =
            offset;

        cout << entry.first
             << ": "
             << offset
             << " -> "
             << offset +
                    entry.second - 1
             << endl;

        offset +=
            entry.second;
    }

    if (offset != totalNodes)
    {
        printf("ERROR: GlobalID mismatch.\n");
        return 1;
    }


    // =====================================================
    // Apply a global edge to stage.Place a "globs" edge on the stage.
    // =====================================================

    cout<< "\n\nSTAGE 4 - LOAD GLOBAL EDGES";
    cout << "========================================\n";

    vector<GlobalEdge> edges;
    edges.reserve(22000000);

    long long totalValidEdges = 0;
    long long totalInvalidEdges = 0;

    auto loadStart =
        chrono::high_resolution_clock::now();

    // LIFE2: Loop over each of the relations:
    for (auto& relation :
         relations)
    {
        string folderName =
            relation.sourceType +
            "___" +
            relation.relationType +
            "___" +
            relation.targetType;

        string edgeFilePath =
            "data/mag/raw/relations/" +
            folderName +
            "/edge.csv";

        cout << "\nLoading "
             << relation.sourceType
             << " --["
             << relation.relationType
             << "]--> "
             << relation.targetType
             << endl;

        ifstream edgeFile(
            edgeFilePath);

        if (!edgeFile.is_open())
        {
            cout << "ERROR: Doing the following things puts you: "
                 << edgeFilePath
                 << endl;

            return 1;
        }

        long long sourceLimit =
            nodeTypeCounts[
                relation.sourceType];

        long long targetLimit =
            nodeTypeCounts[
                relation.targetType];

        int relationID =
            relationTypeToID[
                relation.relationType];

        long long valid = 0;
        long long invalid = 0;

        while (getline(edgeFile, line))
        {
            line = trim(line);

            if (line.empty())
                continue;

            vector<string> values =
                splitCSVLine(line);

            if (values.size() < 2)
            {
                invalid++;
                continue;
            }

            long long localSource;
            long long localTarget;

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

            if (localSource < 0 ||
                localSource >= sourceLimit ||
                localTarget < 0 ||
                localTarget >= targetLimit)
            {
                invalid++;
                continue;
            }

            GlobalEdge edge;

            edge.source =
                static_cast<int>(
                    getGlobalNodeID(
                        relation.sourceType,
                        localSource,
                        nodeTypeOffsets));

            edge.target =
                static_cast<int>(
                    getGlobalNodeID(
                        relation.targetType,
                        localTarget,
                        nodeTypeOffsets));

            edge.relationTypeID =
                relationID;

            edges.push_back(edge);

            valid++;
        }

        edgeFile.close();

        totalValidEdges += valid;
        totalInvalidEdges += invalid;

        cout << "  Valid Edges: "
             << valid
             << endl;

        cout << "  Invalid Edges: "
             << invalid
             << endl;
    }

    auto loadEnd =
        chrono::high_resolution_clock::now();

    double loadSeconds =
        chrono::duration<double>(
            loadEnd - loadStart).count();

    cout << "Total loaded Edges: \n: "
         << edges.size()
         << endl;

    cout << "Loading Time: "
         << fixed
         << setprecision(2)
         << loadSeconds
         << " sec\n";


    // =====================================================
    // STAGE 5 - SORT
    // =====================================================

    cout << "\n\nSTAGE 5 - SORT EDGES\n";
    cout << "========================================\n";

    auto sortStart =
        chrono::high_resolution_clock::now();

    sort(
        edges.begin(),
        edges.end(),

        [](const GlobalEdge& a,
           const GlobalEdge& b)
        {
            if (a.source != b.source)
                return a.source < b.source;

            if (a.relationTypeID !=
                b.relationTypeID)
            {
                return a.relationTypeID <
                       b.relationTypeID;
            }

            return a.target <
                   b.target;
        });

    auto sortEnd =
        chrono::high_resolution_clock::now();

    double sortSeconds =
        chrono::duration<double>(
            sortEnd - sortStart).count();

    cout << "Sorting Complete: "
         << sortSeconds
         << " sec\n";


    // =====================================================
    // Rewrite the formula as a forward formula, and obtain t in terms of r.
    // =====================================================

    cout << "STAGE 6 - BUILD FORWARD MODIFIED CSR \n\n";
    cout << "========================================\n";

    auto csrStart =
        chrono::high_resolution_clock::now();

    vector<long long> rowPointers(
        totalNodes + 1,
        0);

    for (GlobalEdge& edge :
         edges)
    {
        rowPointers[
            static_cast<size_t>(
                edge.source) + 1]++;
    }

    for (long long i = 1;
         i <= totalNodes;
         i++)
    {
        rowPointers[i] +=
            rowPointers[i - 1];
    }

    vector<int> columnIndices;
    vector<int> relationTypeIDs;

    columnIndices.reserve(
        edges.size());

    relationTypeIDs.reserve(
        edges.size());

    for (GlobalEdge& edge :
         edges)
    {
        columnIndices.push_back(
            edge.target);

        relationTypeIDs.push_back(
            edge.relationTypeID);
    }

    vector<vector<TypeRange>>
        typeOffsetIndex(
            totalNodes);

    for (long long node = 0;
         node < totalNodes;
         node++)
    {
        long long start =
            rowPointers[node];

        long long end =
            rowPointers[node + 1];

        long long pos =
            start;

        while (pos < end)
        {
            int type =
                relationTypeIDs[pos];

            long long typeStart =
                pos;

            while (pos < end &&
                   relationTypeIDs[pos] ==
                       type)
            {
                pos++;
            }

            TypeRange range;

            range.relationTypeID =
                type;

            range.start =
                typeStart;

            range.end =
                pos;

            typeOffsetIndex[node].
                push_back(range);
        }
    }

    auto csrEnd =
        chrono::high_resolution_clock::now();

    double csrSeconds =
        chrono::duration<double>(
            csrEnd - csrStart).count();

    cout << "Forward CSR Complete: "
         << csrSeconds
         << " sec\n";

    // =====================================================
    // STAGE 7 - FORWARD VALIDATION
    // =====================================================

    cout << "\n\nSTAGE 7 - FORWARD CSR VALIDATION\n";
    cout << "========================================\n";

    if (rowPointers.back() !=
        static_cast<long long>(
            columnIndices.size()))
    {
        cout << "[ERROR] Forward CSR invalid.\n";
        return 1;
    }

    cout << "[VALID] Forward Modified CSR consistent.\n";


    // =====================================================
    // STAGE 8 - REVERSE CSR
    // =====================================================

    cout << "\n\nSTAGE 8 - BUILD REVERSE CSR\n";
    cout << "========================================\n";

    auto reverseStart =
        chrono::high_resolution_clock::now();

    vector<long long> reverseRowPointers(
        totalNodes + 1,
        0);

    for (long long source = 0;
         source < totalNodes;
         source++)
    {
        for (long long i =
                 rowPointers[source];
             i <
                 rowPointers[source + 1];
             i++)
        {
            int target =
                columnIndices[i];

            reverseRowPointers[
                static_cast<size_t>(
                    target) + 1]++;
        }
    }

    for (long long i = 1;
         i <= totalNodes;
         i++)
    {
        reverseRowPointers[i] +=
            reverseRowPointers[i - 1];
    }

    vector<int> reverseColumnIndices(
        columnIndices.size());

    vector<int> reverseRelationTypeIDs(
        relationTypeIDs.size());

    vector<long long> cursor =
        reverseRowPointers;

    for (long long source = 0;
         source < totalNodes;
         source++)
    {
        for (long long i =
                 rowPointers[source];
             i <
                 rowPointers[source + 1];
             i++)
        {
            int target =
                columnIndices[i];

            long long position =
                cursor[target]++;

            reverseColumnIndices[
                position] =
                static_cast<int>(
                    source);

            reverseRelationTypeIDs[
                position] =
                relationTypeIDs[i];
        }
    }

    vector<long long>().swap(cursor);

    auto reverseEnd =
        chrono::high_resolution_clock::now();

    double reverseSeconds =
        chrono::duration<double>(
            reverseEnd -
            reverseStart).count();

    cout << "Reverse CSR Complete: "
         << reverseSeconds
         << " sec\n";


    // =====================================================
    // STAGE 9 - REVERSE VALIDATION
    // =====================================================

    cout << "\n\nSTAGE 9 - REVERSE CSR VALIDATION\n";
    cout << "========================================\n";

    if (reverseRowPointers.back() !=
        static_cast<long long>(
            reverseColumnIndices.size()))
    {
        cout << "[ERROR] Reverse CSR invalid.\n";
        return 1;
    }

    cout << "[VALID] Reverse CSR consistent.\n";


    // =====================================================
    // FREE TEMPORARY EDGE VECTOR
    // =====================================================

    long long temporaryEdgeMemory =
        static_cast<long long>(
            edges.capacity()) *
        sizeof(GlobalEdge);

    cout << "\nFreeing temporary edge vector...\n";

    vector<GlobalEdge>().swap(edges);

    cout << "Released approximately "
         << fixed
         << setprecision(2)
         << bytesToMB(
                temporaryEdgeMemory)
         << " MB temporary edge memory.\n";


    // =====================================================
    // STAGE 10 - GENERIC RUNTIME META-PATH CONFIGURATION
    // =====================================================

    cout << "\n\nSTAGE 10 - GENERIC RUNTIME CONFIGURATION\n";
    cout << "========================================\n";

    // Runtime usage (all optional):
    //   ./main.exe <start_type> <local_query_id|auto> <k> <path_spec>
    // Example APA:
    //   ./main.exe author 0 5 writes:F,writes:R
    // Example AIA:
    //   ./main.exe author auto 3 affiliated_with:F,affiliated_with:R
    //
    // The algorithm does not contain Author/Paper-specific traversal logic.
    // Relation names and directions are resolved against the discovered schema.

    string startType = (argc > 1) ? argv[1] : "author";
    string queryArg = (argc > 2) ? argv[2] : "auto";
    int k = (argc > 3) ? stoi(argv[3]) : 5;
    string pathSpec = (argc > 4) ? argv[4] : "writes:F,writes:R";

    if (nodeTypeCounts.find(startType) == nodeTypeCounts.end())
    {
        cout << "ERROR: Unknown start node type: " << startType << endl;
        return 1;
    }

    if (k < 1)
    {
        cout << "ERROR: k must be >= 1.\n";
        return 1;
    }

    auto parsePath = [&](const string& spec,
                         const string& initialType,
                         vector<MetaPathStep>& outPath,
                         string& endType,
                         string& readable) -> bool
    {
        outPath.clear();
        readable = initialType;
        string currentType = initialType;
        string token;
        stringstream ss(spec);

        while (getline(ss, token, ','))
        {
            token = trim(token);
            if (token.empty())
                continue;

            size_t colon = token.find(':');
            if (colon == string::npos)
                return false;

            string relationName = trim(token.substr(0, colon));
            string direction = trim(token.substr(colon + 1));
            transform(direction.begin(), direction.end(), direction.begin(), ::toupper);

            bool forward;
            if (direction == "F" || direction == "FORWARD")
                forward = true;
            else if (direction == "R" || direction == "REVERSE")
                forward = false;
            else
                return false;

            auto relationIDIt = relationTypeToID.find(relationName);
            if (relationIDIt == relationTypeToID.end())
                return false;

            bool schemaMatch = false;
            string nextType;

            for (const RelationSchema& r : relations)
            {
                if (r.relationType != relationName)
                    continue;

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

            if (!schemaMatch)
                return false;

            MetaPathStep step;
            step.relationTypeID = relationIDIt->second;
            step.forward = forward;
            outPath.push_back(step);

            readable += " --[" + relationName +
                        (forward ? " FORWARD" : " REVERSE") +
                        "]--> " + nextType;

            currentType = nextType;
        }

        endType = currentType;
        return !outPath.empty();
    };

    vector<MetaPathStep> metaPath;
    string endType;
    string readablePath;

    if (!parsePath(pathSpec, startType, metaPath, endType, readablePath))
    {
        cout << "ERROR: Invalid path specification or path does not match discovered HIN schema.\n";
        cout << "Format example: writes:F,writes:R\n";
        return 1;
    }

    if (endType != startType)
    {
        cout << "ERROR: For query-centred k-core, the selected meta-path must return\n";
        cout << "to the same node type. Start=" << startType
             << ", End=" << endType << endl;
        return 1;
    }

    cout << "Start Node Type: " << startType << endl;
    cout << "Selected k: " << k << endl;
    cout << "Path Specification: " << pathSpec << endl;
    cout << "Resolved Meta-Path:\n  " << readablePath << endl;
    cout << "Number of Steps: " << metaPath.size() << endl;
    cout << "[VALID] Runtime meta-path matches discovered HIN schema.\n";


    // =====================================================
    // STAGE 11 - META-PATH QUERY
    // =====================================================

    cout << "\n\nSTAGE 11 - META-PATH QUERY\n";
    cout << "========================================\n";

    long long startTypeOffset = nodeTypeOffsets[startType];
    long long startTypeCount = nodeTypeCounts[startType];

    int queryNode = -1;
    vector<int> metaPathNeighbours;

    auto queryStart = chrono::high_resolution_clock::now();

    if (queryArg != "auto")
    {
        long long localID;
        try
        {
            localID = stoll(queryArg);
        }
        catch (...)
        {
            cout << "ERROR: Query local ID must be an integer or 'auto'.\n";
            return 1;
        }

        if (localID < 0 || localID >= startTypeCount)
        {
            cout << "ERROR: Query local ID is outside the selected node type range.\n";
            return 1;
        }

        queryNode = static_cast<int>(startTypeOffset + localID);
        metaPathNeighbours = followMetaPath(
            queryNode, metaPath,
            rowPointers, columnIndices, relationTypeIDs,
            reverseRowPointers, reverseColumnIndices, reverseRelationTypeIDs);
    }
    else
    {
        // Search a bounded prefix for a useful demonstration query.
        long long searchLimit = min<long long>(startTypeCount, 1000);

        for (long long localID = 0; localID < searchLimit; localID++)
        {
            int candidate = static_cast<int>(startTypeOffset + localID);
            vector<int> result = followMetaPath(
                candidate, metaPath,
                rowPointers, columnIndices, relationTypeIDs,
                reverseRowPointers, reverseColumnIndices, reverseRelationTypeIDs);

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
        cout << "No suitable query node with meta-path neighbours found.\n";
        return 1;
    }

    cout << "Query Node:\n";
    cout << "  Type: " << startType << endl;
    cout << "  Local ID: " << queryNode - startTypeOffset << endl;
    cout << "  Global ID: " << queryNode << endl;
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

    cout << "\nMeta-Path Query Time: " << fixed << setprecision(6)
         << querySeconds << " sec\n";


    // =====================================================
    // STAGE 12 - META-PATH VALIDATION
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
    // =====================================================

    cout << "\n\nSTAGE 13 - BUILD META-PATH DERIVED GRAPH\n";
    cout << "========================================\n";

    auto derivedStart = chrono::high_resolution_clock::now();

    vector<int> derivedNodes;
    vector<vector<int>> derivedAdjacency;
    unordered_map<int, int> globalToDerived;

    buildDerivedGraph(
        queryNode, metaPathNeighbours, metaPath,
        rowPointers, columnIndices, relationTypeIDs,
        reverseRowPointers, reverseColumnIndices, reverseRelationTypeIDs,
        derivedNodes, derivedAdjacency, globalToDerived);

    auto derivedEnd = chrono::high_resolution_clock::now();
    double derivedSeconds = chrono::duration<double>(derivedEnd - derivedStart).count();

    long long derivedEdgeReferences = 0;
    for (const vector<int>& neighbours : derivedAdjacency)
        derivedEdgeReferences += static_cast<long long>(neighbours.size());
    long long derivedEdges = derivedEdgeReferences / 2;

    cout << "Candidate Nodes: " << derivedNodes.size() << endl;
    cout << "Derived Undirected Edges: " << derivedEdges << endl;
    cout << "Derived Graph Construction Time: " << fixed << setprecision(6)
         << derivedSeconds << " sec\n";

    cout << "\nDerived Graph Sample:\n";
    for (size_t i = 0; i < derivedNodes.size() && i < 20; i++)
    {
        cout << "  " << startType << " Global " << derivedNodes[i]
             << " | Local " << derivedNodes[i] - startTypeOffset
             << " | Meta-Path Degree " << derivedAdjacency[i].size() << endl;
    }

    if (derivedNodes.empty())
    {
        cout << "[ERROR] Derived graph is empty.\n";
        return 1;
    }
    cout << "\n[VALID] Query-centred meta-path derived graph created.\n";


    // =====================================================
    // STAGE 14 - MULTIPLE K-CORE TESTING
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
            testCommunity = extractQueryCommunity(qIt->second, derivedAdjacency, testRemoved);

        auto testEnd = chrono::high_resolution_clock::now();
        double testSeconds = chrono::duration<double>(testEnd - testStart).count();

        cout << left << setw(8) << testK
             << setw(18) << derivedNodes.size()
             << setw(18) << testRemovedCount
             << setw(18) << testSurviving
             << setw(20) << testCommunity.size()
             << fixed << setprecision(6) << testSeconds << endl;
    }


    // =====================================================
    // STAGE 15 - DETAILED RUNTIME K-CORE
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
    // At STAGE 16, the final query community + validation will be in a sample of the community.
    // =====================================================

    cout << "\n\nStage 16 – FINAL QUERY COMMUNITY\n";
    cout << "========================================\n";

    auto queryIndexIt = globalToDerived.find(queryNode);
    if (queryIndexIt == globalToDerived.end())
    {
        cout<< "There is no query node in the derived graph. \n";
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
        std::cout << "Final Community Size: " << communityIndices.size() << std::endl;
        cout << "\nCommunity Members:\n";

        for (size_t x = 0; x < communityIndices.size() && x < 30; x++)
        {
            int idx = communityIndices[x];
            int globalID = derivedNodes[idx];
            cout << "  " << startType << " Global ID: " << globalID
                 << " Unique ID: " << globalID - startTypeOffset
                 << " | Final Degree: " << finalDegrees[idx];
            if (globalID == queryNode) cout << "  <-- QUERY";
            cout << endl;
        }
    }

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
        cout << "[ERROR] K-core validation failed. \n";
        return 1;
    }

    if (survivingNodes == 0)
        cout << "[VALID] No nodes survive k=" << k
             << "; Please see that the candidate graph (in its entirety) was removed in an acceptable way.\n";
    else
        cout << "[VALID] Every surviving node satisfies degree >= " << k << ".\n";
    cout << "[VALID] Iterative k-core peeling is consistent." << endl;


    // =====================================================
    // In this stage, you will learn about other meta-path tests.During this phase you will learn about other types of meta-path testing.
    // =====================================================

    cout << "\n\nSTAGE 17 – ADDITIONAL META-PATH TEST\n";
    cout << "========================================\n";

    string additionalSpec;
    string additionalName;

    // Illinois is aware that there are author queries, which is why they include a second relationship for MAG.
    // Author -> Institution -> Author.
    if (startType == "author" &&
        relationTypeToID.find("affiliated_with") != relationTypeToID.end())
    {
        additionalSpec = "affiliated_with:F,affiliated_with:R";
        additionalName = "AIA";
    }
    else
    {
        // Use the selected paths for Engine consistency test – generic fallback.
        additionalSpec = pathSpec;
        additionalName = "Runtime Path Re-test";
    }

    vector<MetaPathStep> additionalPath;
    string additionalEndType;
    string additionalReadable;
    bool additionalPathValid = parsePath(
        additionalSpec, startType, additionalPath,
        additionalEndType, additionalReadable);

    vector<int> additionalNeighbours;
    int additionalQuery = -1;
    double additionalSeconds = 0.0;

    if (additionalPathValid && additionalEndType == startType)
    {
        auto additionalStart = chrono::high_resolution_clock::now();

        // Try the specified query, first. If no neighbour is found for a sector for the second time, put X in the box.If there is no neighbour for a sector for the second time, then mark X in the box.
        // We need a query which can be shown in a bounded prefix with the help of // semantic path.We need a query that can be proven in a bounded prefix on the basis of // semantic path.
        additionalQuery = queryNode;
        additionalNeighbours = followMetaPath(
            additionalQuery, additionalPath,
            rowPointers, columnIndices, relationTypeIDs,
            reverseRowPointers, reverseColumnIndices, reverseRelationTypeIDs);

        if (additionalNeighbours.empty())
        {
            long long searchLimit = min<long long>(startTypeCount, 1000);

            // In case startTypeCount is more than 1000, use min<long long>(startTypeCount, 1000) to set searchLimit.
            // While searchLimit is not reached, we go through the localIDs starting with 0.
            for (long long localID = 0; localID < searchLimit; localID++)
            {
                // [JAWS] The startTypeOffset number is a number of seconds.The startTypeOffset number is a number of seconds.
                int candidate = static_cast<int>(startTypeOffset + localID);

                vector<int> result = followMetaPath(
                    candidate, additionalPath,
                    rowPointers, columnIndices, relationTypeIDs,
                    reverseRowPointers, reverseColumnIndices, reverseRelationTypeIDs);

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

    long long forwardRowBytes = static_cast<long long>(rowPointers.capacity()) * sizeof(long long);
    long long forwardColumnBytes = static_cast<long long>(columnIndices.capacity()) * sizeof(int);
    long long forwardTypeBytes = static_cast<long long>(relationTypeIDs.capacity()) * sizeof(int);
    long long reverseRowBytes = static_cast<long long>(reverseRowPointers.capacity()) * sizeof(long long);
    long long reverseColumnBytes = static_cast<long long>(reverseColumnIndices.capacity()) * sizeof(int);
    long long reverseTypeBytes = static_cast<long long>(reverseRelationTypeIDs.capacity()) * sizeof(int);

    long long typeRangeCount = 0;
    for (const auto& ranges : typeOffsetIndex)
        typeRangeCount += static_cast<long long>(ranges.size());
    long long typeRangePayloadBytes = typeRangeCount * sizeof(TypeRange);

    long long derivedNodeBytes = static_cast<long long>(derivedNodes.capacity()) * sizeof(int);
    long long derivedAdjacencyPayloadBytes = 0;
    for (const auto& a : derivedAdjacency)
        derivedAdjacencyPayloadBytes += static_cast<long long>(a.capacity()) * sizeof(int);

    long long forwardMainBytes = forwardRowBytes + forwardColumnBytes + forwardTypeBytes;
    long long reverseMainBytes = reverseRowBytes + reverseColumnBytes + reverseTypeBytes;
    long long combinedCSRBytes = forwardMainBytes + reverseMainBytes;

    cout << "Forward Row Pointers: " << bytesToMB(forwardRowBytes) << " MB\n";
    cout << "Forward Columns: " << bytesToMB(forwardColumnBytes) << " MB\n";
    cout << "Forward Relation IDs: " << bytesToMB(forwardTypeBytes) << " MB\n";
    cout << "Reverse Row Pointers: " << bytesToMB(reverseRowBytes) << " MB\n";
    cout << "Reverse Columns: " << bytesToMB(reverseColumnBytes) << " MB\n";
    cout << "Reverse Relation IDs: " << bytesToMB(reverseTypeBytes) << " MB\n";
    cout << "Forward Main CSR Arrays: " << bytesToMB(forwardMainBytes) << " MB\n";
    cout << "Reverse Main CSR Arrays: " << bytesToMB(reverseMainBytes) << " MB\n";
    cout << "Combined Main CSR Arrays: " << bytesToMB(combinedCSRBytes) << " MB\n";
    cout << "Type_Offset_Index Ranges: " << typeRangeCount
         << " (~" << bytesToMB(typeRangePayloadBytes) << " MB payload)\n";
    cout << "Derived Graph Payload: ~"
         << bytesToMB(derivedNodeBytes + derivedAdjacencyPayloadBytes) << " MB\n";
    cout << "Temporary Edge Vector Released Earlier: "
         << bytesToMB(temporaryEdgeMemory) << " MB\n";

    cout << "\nPerformance Summary:\n";
    cout << "  Edge Loading: " << loadSeconds << " sec\n";
    cout << "  Edge Sorting: " << sortSeconds << " sec\n";
    cout << "  Forward CSR: " << csrSeconds << " sec\n";
    cout << "  Reverse CSR: " << reverseSeconds << " sec\n";
    cout << "  Selected Meta-Path Query: " << querySeconds << " sec\n";
    cout << "  Derived Graph: " << derivedSeconds << " sec\n";
    cout << "  K-Core Peeling: " << kCoreSeconds << " sec\n";
    cout << "  Additional Meta-Path Test: " << additionalSeconds << " sec\n";


    // =====================================================
    // STAGE 19 - FINAL TEST / DEMO CHECKLIST
    // =====================================================

    cout << "\n\nSTAGE 19 - FINAL TEST AND DEMO CHECKLIST\n";
    cout << "========================================\n";

    bool forwardCSRValid = rowPointers.back() == static_cast<long long>(columnIndices.size());
    bool reverseCSRValid = reverseRowPointers.back() == static_cast<long long>(reverseColumnIndices.size());
    bool edgeCountValid = totalValidEdges == static_cast<long long>(columnIndices.size());
    bool runtimePathValid = !metaPath.empty() && endType == startType;
    bool derivedGraphValid = !derivedNodes.empty() && globalToDerived.count(queryNode);

    cout << (forwardCSRValid ? "[PASS] " : "[FAIL] ") << "Forward CSR consistency\n";
    cout << (reverseCSRValid ? "[PASS] " : "[FAIL] ") << "Reverse CSR consistency\n";
    cout << (edgeCountValid ? "[PASS] " : "[FAIL] ") << "Loaded edge count consistency\n";
    cout << (runtimePathValid ? "[PASS] " : "[FAIL] ") << "Runtime meta-path schema validation\n";
    cout << (derivedGraphValid ? "[PASS] " : "[FAIL] ") << "Query-centred derived graph\n";
    cout << (kCoreValid ? "[PASS] " : "[FAIL] ") << "Iterative k-core validation\n";
    cout << ((!additionalNeighbours.empty()) ? "[PASS] " : "[INFO] ")
         << "Additional meta-path test\n";

    bool allCriticalTests = forwardCSRValid && reverseCSRValid &&
                            edgeCountValid && runtimePathValid &&
                            derivedGraphValid && kCoreValid;

    if (!allCriticalTests)
    {
        cout << "\n[ERROR] One or more critical final tests failed.\n";
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
    cout << "Total Nodes: " << totalNodes << endl;
    cout << "Total Valid Edges: " << totalValidEdges << endl;
    cout << "Total Invalid Edges: " << totalInvalidEdges << endl;

    cout << "\nRuntime Configuration:\n";
    cout << "  Start Type: " << startType << endl;
    cout << "  Query Local ID: " << queryNode - startTypeOffset << endl;
    cout << "  Query Global ID: " << queryNode << endl;
    cout << "  Path Spec: " << pathSpec << endl;
    cout << "  k: " << k << endl;

    cout << "\nDerived Graph:\n";
    cout << "  Nodes: " << derivedNodes.size() << endl;
    cout << "  Edges: " << derivedEdges << endl;
    cout << "  Surviving Nodes: " << survivingNodes << endl;
    cout << "  Query Community Size: " << communityIndices.size() << endl;

    cout << "\nAdditional Meta-Path:\n";
    cout << "  Name: " << additionalName << endl;
    cout << "  Spec: " << additionalSpec << endl;
    cout << "  Neighbours: " << additionalNeighbours.size() << endl;

    cout << "\nTotal Program Time: " << totalSeconds << " sec\n";

    cout << "\nStatus:\n";
    cout << "[DONE] Real MAG Dataset\n";
    cout << "[DONE] Dynamic Schema Discovery\n";
    cout << "[DONE] Runtime Relation Mapping\n";
    cout << "[DONE] Global Node IDs\n";
    cout << "[DONE] Forward Modified CSR + Type_Offset_Index\n";
    cout << "[DONE] Reverse CSR\n";
    cout << "[DONE] Generic Runtime Meta-Path Configuration\n";
    cout << "[DONE] Query-Centred Derived Graph\n";
    cout << "[DONE] Multiple k Testing\n";
    cout << "[DONE] Iterative K-Core Peeling\n";
    cout << "[DONE] Final Query Community Extraction\n";
    cout << "[DONE] Additional Meta-Path Test\n";
    cout << "[DONE] Memory Evaluation\n";
    cout << "[DONE] Performance Evaluation\n";
    cout << "[DONE] Final Validation / Demo Checklist\n";

    cout << "\nUSAGE EXAMPLES:\n";
    cout << "  Default APA: ./main.exe\n";
    cout << "  APA custom:  ./main.exe author 0 4 writes:F,writes:R\n";
    cout << "  AIA custom:  ./main.exe author auto 3 affiliated_with:F,affiliated_with:R\n";

    cout << "========================================\n";
    return 0;
}
