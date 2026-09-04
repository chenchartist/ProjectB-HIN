#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

using namespace std;


// -------------------------------------------------
// EDGE STRUCTURE
// -------------------------------------------------

struct Edge
{
    string source;
    string sourceType;
    string target;
    string targetType;
    string edgeType;
};


// -------------------------------------------------
// CHECK IF EDGE FOLLOWS HIN SCHEMA
// -------------------------------------------------

bool isValidEdge(const Edge &edge)
{
    if (edge.sourceType == "Author" &&
        edge.targetType == "Paper" &&
        edge.edgeType == "Write")
    {
        return true;
    }

    if (edge.sourceType == "Paper" &&
        edge.targetType == "Paper" &&
        edge.edgeType == "Cite")
    {
        return true;
    }

    if (edge.sourceType == "Paper" &&
        edge.targetType == "Venue" &&
        edge.edgeType == "Published_In")
    {
        return true;
    }

    if (edge.sourceType == "Author" &&
        edge.targetType == "Institution" &&
        edge.edgeType == "Affiliated_With")
    {
        return true;
    }

    return false;
}


int main()
{
    cout << "========================================\n";
    cout << " HIN COMMUNITY SEARCH SYSTEM\n";
    cout << " Modified CSR + K-Core + Meta-Path\n";
    cout << "========================================\n\n";


    // -------------------------------------------------
    // STAGE 1: LOAD DATASET
    // -------------------------------------------------

    ifstream file("data/dataset.csv");

    if (!file.is_open())
    {
        cout << "Error: Could not open dataset.csv\n";
        return 1;
    }

    vector<Edge> edges;

    set<string> uniqueNodes;

    map<string, int> nodeTypeCount;
    map<string, int> edgeTypeCount;

    string line;


    // Skip CSV header
    getline(file, line);


    while (getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }

        stringstream ss(line);

        Edge edge;

        getline(ss, edge.source, ',');
        getline(ss, edge.sourceType, ',');
        getline(ss, edge.target, ',');
        getline(ss, edge.targetType, ',');
        getline(ss, edge.edgeType, ',');

        edges.push_back(edge);

        uniqueNodes.insert(edge.source);
        uniqueNodes.insert(edge.target);

        edgeTypeCount[edge.edgeType]++;
    }

    file.close();


    // -------------------------------------------------
    // STAGE 2: CREATE NODE TYPE MAP
    // -------------------------------------------------

    map<string, string> nodeTypeMap;

    for (const Edge &edge : edges)
    {
        nodeTypeMap[edge.source] = edge.sourceType;
        nodeTypeMap[edge.target] = edge.targetType;
    }


    for (const auto &entry : nodeTypeMap)
    {
        nodeTypeCount[entry.second]++;
    }


    // -------------------------------------------------
    // DISPLAY DATASET SUMMARY
    // -------------------------------------------------

    cout << "DATASET SUMMARY\n";
    cout << "----------------------------------------\n";

    cout << "Total Nodes: "
         << uniqueNodes.size()
         << endl;

    cout << "Total Edges: "
         << edges.size()
         << endl;


    cout << "\nNode Types:\n";

    for (const auto &entry : nodeTypeCount)
    {
        cout << entry.first
             << ": "
             << entry.second
             << endl;
    }


    cout << "\nEdge Types:\n";

    for (const auto &entry : edgeTypeCount)
    {
        cout << entry.first
             << ": "
             << entry.second
             << endl;
    }


    // -------------------------------------------------
    // STAGE 3: NODE ID MAPPING
    // -------------------------------------------------

    map<string, int> nodeIDMap;

    map<int, string> idToNodeMap;

    map<int, string> nodeTypeByID;


    int nextID = 0;


    for (const auto &entry : nodeTypeMap)
    {
        string nodeName = entry.first;
        string nodeType = entry.second;

        nodeIDMap[nodeName] = nextID;

        idToNodeMap[nextID] = nodeName;

        nodeTypeByID[nextID] = nodeType;

        nextID++;
    }


    // -------------------------------------------------
    // DISPLAY NODE ID MAPPING
    // -------------------------------------------------

    cout << "\nNODE ID MAPPING\n";
    cout << "----------------------------------------\n";


    for (const auto &entry : idToNodeMap)
    {
        int id = entry.first;

        string nodeName = entry.second;

        cout << "ID "
             << id
             << " -> "
             << nodeName
             << " ("
             << nodeTypeByID[id]
             << ")"
             << endl;
    }


    // -------------------------------------------------
    // STAGE 4: HIN SCHEMA VALIDATION
    // -------------------------------------------------

    vector<Edge> validEdges;
    vector<Edge> invalidEdges;


    for (const Edge &edge : edges)
    {
        if (isValidEdge(edge))
        {
            validEdges.push_back(edge);
        }
        else
        {
            invalidEdges.push_back(edge);
        }
    }


    cout << "\nHIN SCHEMA VALIDATION\n";
    cout << "----------------------------------------\n";

    cout << "Valid Edges: "
         << validEdges.size()
         << endl;

    cout << "Invalid Edges: "
         << invalidEdges.size()
         << endl;


    if (!invalidEdges.empty())
    {
        cout << "\nInvalid Edge Details:\n";

        for (const Edge &edge : invalidEdges)
        {
            cout << edge.source
                 << " ("
                 << edge.sourceType
                 << ") -> "
                 << edge.target
                 << " ("
                 << edge.targetType
                 << ") ["
                 << edge.edgeType
                 << "]"
                 << endl;
        }
    }
    else
    {
        cout << "All edges follow the HIN schema.\n";
    }


    // -------------------------------------------------
    // STAGE 5: SORT VALID EDGES
    // Source ID -> Edge Type -> Target ID
    // -------------------------------------------------

    sort(validEdges.begin(),
         validEdges.end(),
         [&](const Edge &a, const Edge &b)
         {
             int sourceA = nodeIDMap[a.source];
             int sourceB = nodeIDMap[b.source];

             int targetA = nodeIDMap[a.target];
             int targetB = nodeIDMap[b.target];


             if (sourceA != sourceB)
             {
                 return sourceA < sourceB;
             }


             if (a.edgeType != b.edgeType)
             {
                 return a.edgeType < b.edgeType;
             }


             return targetA < targetB;
         });


    // -------------------------------------------------
    // DISPLAY SORTED VALID EDGES
    // -------------------------------------------------

    cout << "\nSORTED VALID EDGES\n";
    cout << "----------------------------------------\n";


    for (const Edge &edge : validEdges)
    {
        cout << nodeIDMap[edge.source]
             << " ("
             << edge.source
             << ")"
             << " -> "
             << nodeIDMap[edge.target]
             << " ("
             << edge.target
             << ")"
             << "  Type: "
             << edge.edgeType
             << endl;
    }


    // =================================================
    // STAGE 6: BUILD MODIFIED CSR
    // =================================================

    int totalNodes = nodeIDMap.size();

    vector<int> rowPointers(totalNodes + 1, 0);

    vector<int> columnIndices;

    vector<string> csrEdgeTypes;


    // Type Offset Index:
    // Node ID -> Edge Type -> (Start Position, End Position)

    map<int, map<string, pair<int, int>>> typeOffsetIndex;


    // -------------------------------------------------
    // BUILD ROW POINTERS
    // -------------------------------------------------

    for (const Edge &edge : validEdges)
    {
        int sourceID = nodeIDMap[edge.source];

        rowPointers[sourceID + 1]++;
    }


    // Convert edge counts into cumulative positions

    for (int i = 1; i <= totalNodes; i++)
    {
        rowPointers[i] += rowPointers[i - 1];
    }


    // -------------------------------------------------
    // BUILD COLUMN INDICES
    // -------------------------------------------------

    for (const Edge &edge : validEdges)
    {
        int targetID = nodeIDMap[edge.target];

        columnIndices.push_back(targetID);

        csrEdgeTypes.push_back(edge.edgeType);
    }


    // -------------------------------------------------
    // BUILD TYPE OFFSET INDEX
    // -------------------------------------------------

    for (int nodeID = 0; nodeID < totalNodes; nodeID++)
    {
        int start = rowPointers[nodeID];

        int end = rowPointers[nodeID + 1];


        int position = start;


        while (position < end)
        {
            string currentType = csrEdgeTypes[position];

            int typeStart = position;


            while (position < end &&
                   csrEdgeTypes[position] == currentType)
            {
                position++;
            }


            int typeEnd = position;


            typeOffsetIndex[nodeID][currentType] =
                make_pair(typeStart, typeEnd);
        }
    }


    // =================================================
    // DISPLAY MODIFIED CSR
    // =================================================

    cout << "\nMODIFIED CSR STRUCTURE\n";
    cout << "----------------------------------------\n";


    // -------------------------------------------------
    // ROW POINTERS
    // -------------------------------------------------

    cout << "\nRow_Pointers:\n";

    for (int value : rowPointers)
    {
        cout << value << " ";
    }

    cout << endl;


    // -------------------------------------------------
    // COLUMN INDICES
    // -------------------------------------------------

    cout << "\nColumn_Indices:\n";

    for (int value : columnIndices)
    {
        cout << value << " ";
    }

    cout << endl;


    // -------------------------------------------------
    // EDGE TYPES
    // -------------------------------------------------

    cout << "\nEdge_Types:\n";

    for (const string &type : csrEdgeTypes)
    {
        cout << type << " ";
    }

    cout << endl;


    // -------------------------------------------------
    // TYPE OFFSET INDEX
    // -------------------------------------------------

    cout << "\nTYPE OFFSET INDEX\n";
    cout << "----------------------------------------\n";


    for (int nodeID = 0; nodeID < totalNodes; nodeID++)
    {
        cout << "\nNode "
             << idToNodeMap[nodeID]
             << " [ID "
             << nodeID
             << "]"
             << endl;


        if (typeOffsetIndex[nodeID].empty())
        {
            cout << "  No outgoing edges\n";

            continue;
        }


        for (const auto &entry : typeOffsetIndex[nodeID])
        {
            string edgeType = entry.first;

            int start = entry.second.first;
            int end = entry.second.second;


            cout << "  "
                 << edgeType
                 << " -> ["
                 << start
                 << ", "
                 << end
                 << ")";


            // Also display actual neighbours

            cout << "  Neighbours: ";


            for (int i = start; i < end; i++)
            {
                int neighbourID = columnIndices[i];

                cout << idToNodeMap[neighbourID];

                if (i < end - 1)
                {
                    cout << ", ";
                }
            }


            cout << endl;
        }
    }


    // -------------------------------------------------
    // CSR SUMMARY
    // -------------------------------------------------

    cout << "\nCSR SUMMARY\n";
    cout << "----------------------------------------\n";

    cout << "Number of Nodes: "
         << totalNodes
         << endl;

    cout << "Number of Stored Edges: "
         << columnIndices.size()
         << endl;

    cout << "Row_Pointers Size: "
         << rowPointers.size()
         << endl;

    cout << "Column_Indices Size: "
         << columnIndices.size()
         << endl;

    cout << "Type_Offset_Index Nodes: "
         << typeOffsetIndex.size()
         << endl;


    // -------------------------------------------------
    // CURRENT IMPLEMENTATION STATUS
    // -------------------------------------------------

    cout << "\n========================================\n";

    cout << "IMPLEMENTATION STATUS\n";

    cout << "========================================\n";

    cout << "[DONE] Dataset Loading\n";
    cout << "[DONE] Node ID Mapping\n";
    cout << "[DONE] HIN Schema Validation\n";
    cout << "[DONE] Edge Sorting\n";
    cout << "[DONE] Modified CSR Construction\n";

    cout << "\nModified CSR built successfully.\n";

    cout << "========================================\n";


    return 0;
}