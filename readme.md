# HIN Community Search

A C++ program that finds connected research communities using meta-paths and k-core search. It can find authors connected through shared papers, institutions, or research topics, and explain their shared connections.

A k-core community requires every member to have at least `k` connections within the community.

## Requirements

- A compiler supporting C++17, such as g++ or clang++.
- Git.
- Around 8 GB RAM recommended for the large OGB-MAG dataset.
- Python 3.8+ only for the optional topic helper.

## Download and Build

```bash
git clone https://github.com/chenchartist/ProjectB-HIN.git
cd ProjectB-HIN/HIN_Community_Search
g++ -O2 -std=c++17 main.cpp -o main.exe
```

Run all commands below from `HIN_Community_Search`.

## Datasets

- **DBLP:** Included in `data/dblp/`. Smaller dataset with readable names, suitable for demonstrations.
- **OGB-MAG:** Download separately and extract into `data/mag/`. Uncompress `.csv.gz` files before running.

Each dataset needs `raw/num-node-dict.csv`, `raw/triplet-type-list.csv`, and `raw/relations/<source>___<relation>___<target>/edge.csv`. Files in `mapping/` provide node labels.

## Run

On macOS/Linux:

```bash
./main.exe author 1607 2 "writes:F,has_topic:F,has_topic:R,writes:R" 1 0 data/dblp "field_of_study:57|1252|563"
```

On Windows PowerShell, use `.\main.exe` instead of `./main.exe`.

This example searches for an author community through selected research topics.

### Command Arguments

```text
./main.exe startType queryNode k metaPath p maxFanout datasetPath queryNodes
```

| Argument | Meaning |
|---|---|
| `startType` | Starting node type, such as `author` or `paper` |
| `queryNode` | Local node ID, or `auto` to select a node automatically |
| `k` | Minimum connections per member within the community |
| `metaPath` | Relationship sequence; `F` means forward, `R` means reverse |
| `p` | Minimum community size |
| `maxFanout` | Filters intermediate nodes with too many connections; `0` means unlimited |
| `datasetPath` | Dataset folder; defaults to `data/mag` |
| `queryNodes` | Optional restriction, such as `"field_of_study:57|1252|563"` |

### More Examples

```bash
# APA: authors connected through shared papers
./main.exe author auto 2 "writes:F,writes:R" 1 0 data/dblp

# AIA: authors connected through shared institutions
./main.exe author auto 2 "affiliated_with:F,affiliated_with:R" 1 100 data/dblp

# Optional: list an author's topics before choosing topic IDs
python3 scripts/author_topics.py 1607 data/dblp
```

## Results and Updates

The output includes community members, available node labels, shared attributes, runtime statistics, and validation checks. A query may return no community if its requirements are too strict.

After updating with `git pull`, rebuild the program using the compile command above.