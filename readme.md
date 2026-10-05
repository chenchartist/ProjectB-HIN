# HIN Community Search System

This project is a C++ program for finding strong communities in a Heterogeneous Information Network (HIN).

The dataset contains:

- Authors
- Papers
- Institutions
- Fields of Study

The relationships include:

- `author --writes--> paper`
- `author --affiliated_with--> institution`
- `paper --cites--> paper`
- `paper --has_topic--> field_of_study`

The program uses meta-path search and k-core community search to find strongly connected groups.

## Dataset Path

The dataset must be placed here:

```text
HIN_Community_Search/data/mag/raw/
```

Required files:

```text
data/mag/raw/num-node-dict.csv
data/mag/raw/triplet-type-list.csv
data/mag/raw/relations/.../edge.csv
```

The dataset is ignored by Git because it is too large.

## Compile

Go to the C++ folder:

```bash
cd HIN_Community_Search
```

Compile the program:

```bash
g++ main.cpp -o main.exe
```

## Run

Run with default settings:

```bash
./main.exe
```

Run with custom settings:

```bash
./main.exe startType queryNode k metaPath p
```

Meaning:

```text
startType = node type to search from
queryNode = local node ID or auto
k = minimum degree for k-core
metaPath = relationship path
p = minimum community size
```

## Example Commands

Find author communities through papers:

```bash
./main.exe author auto 2 writes:F,writes:R 1
```

Find author communities through institutions:

```bash
./main.exe author auto 2 affiliated_with:F,affiliated_with:R 1
```

Find paper communities through citations:

```bash
./main.exe paper auto 2 cites:F,cites:R 1
```

Search using a specific author local ID:

```bash
./main.exe author 10 2 writes:F,writes:R 1
```

Use a stronger k-core requirement:

```bash
./main.exe author auto 5 writes:F,writes:R 1
```

## Meta-Path Examples

APA means:

```text
Author -> Paper -> Author
```

In this program, APA is written as:

```text
writes:F,writes:R
```

Other useful meta-paths:

```text
APA: Author -> Paper -> Author
writes:F,writes:R

AIA: Author -> Institution -> Author
affiliated_with:F,affiliated_with:R

PCP: Paper -> Paper -> Paper through citations
cites:F,cites:R

PFP: Paper -> Field of Study -> Paper
has_topic:F,has_topic:R
```

## Notes

A k-core community means every node in the final group has at least `k` connections inside the group.

The current main implementation is C++. The Python files are older reference/demo files and are not required to run the main project.
