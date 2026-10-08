"""List an author's topics so you can pick query topics.

Usage (from the project folder):
    python scripts/author_topics.py 1607            # by local author ID
    python scripts/author_topics.py "Jiawei Han"    # by name
    python scripts/author_topics.py 1607 data/dblp 40

Columns: topic local ID, topic name, the author's papers on it,
and all papers on it in the dataset. Prefer topics with a smaller
"all papers" count: they give smaller, more specific communities.
"""
import sys
from collections import Counter, defaultdict

who = sys.argv[1] if len(sys.argv) > 1 else "0"
root = (sys.argv[2] if len(sys.argv) > 2 else "data/dblp").rstrip("/\\") + "/"
top = int(sys.argv[3]) if len(sys.argv) > 3 else 30

def names(path):
    out = {}
    for line in open(path, encoding="utf-8"):
        parts = line.rstrip("\n").split(",", 1)
        if len(parts) == 2 and parts[0].isdigit():
            out[int(parts[0])] = parts[1]
    return out

authors = names(root + "mapping/author_entidx2name.csv")
topics = names(root + "mapping/field_of_study_entidx2name.csv")

if who.isdigit():
    author = int(who)
else:
    matches = [a for a, n in authors.items() if n.lower() == who.lower()]
    if not matches:
        sys.exit("Author not found: " + who)
    author = matches[0]

papers = [int(p) for a, p in (l.strip().split(",") for l in open(root + "raw/relations/author___writes___paper/edge.csv")) if int(a) == author]
paper_topics = defaultdict(list)
topic_papers = Counter()
for line in open(root + "raw/relations/paper___has_topic___field_of_study/edge.csv"):
    p, t = map(int, line.strip().split(","))
    paper_topics[p].append(t)
    topic_papers[t] += 1

mine = Counter(t for p in papers for t in paper_topics[p])
print(f"{authors.get(author, author)} (local ID {author}): {len(papers)} papers, {len(mine)} topics\n")
print(f"{'ID':>6}  {'Topic':40s} {'Author papers':>13} {'All papers':>10}")
for t, n in mine.most_common(top):
    print(f"{t:>6}  {topics.get(t, t)[:40]:40s} {n:>13} {topic_papers[t]:>10}")
