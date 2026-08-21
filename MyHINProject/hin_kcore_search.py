"""
T006a – Typed K-Core Community Search using Modified CSR
Python Implementation for HIN (Heterogeneous Information Networks)
Optimized for DBLP and similar datasets
"""

from collections import defaultdict, deque
import time
from typing import Dict, List, Tuple, Set
import json


class HINKCoreSearch:
    """
    Heterogeneous Information Network K-Core Community Search using Modified CSR format.
    
    Supports:
    - Edge-type based filtering
    - Compact node ID mapping
    - Efficient degree computation via Type_Offset_Index
    - K-core peeling algorithm
    """
    
    def __init__(self):
        self.adjacency_list = defaultdict(list)
        self.node_to_id = {}
        self.id_to_node = {}
        self.node_type_map = {}
        self.next_node_id = 0
        self.valid_edges = []
        
        # CSR structures
        self.row_pointers = []
        self.column_indices = []
        self.type_offset_index = defaultdict(lambda: defaultdict(tuple))
        
        # K-core structures
        self.degree = {}
        self.removed = {}
        self.in_queue = {}
        self.queue = deque()
    
    def add_edge(self, source: str, target: str, edge_type: str, source_node_type: str = None, target_node_type: str = None):
        """
        Stage 2: Temporary Adjacency List Ingestion
        Add edge to the raw HIN data.
        """
        # Map nodes if not already mapped
        if source not in self.node_to_id:
            node_id = self.next_node_id
            self.node_to_id[source] = node_id
            self.id_to_node[node_id] = source
            self.node_type_map[node_id] = source_node_type or "Unknown"
            self.next_node_id += 1
        
        if target not in self.node_to_id:
            node_id = self.next_node_id
            self.node_to_id[target] = node_id
            self.id_to_node[node_id] = target
            self.node_type_map[node_id] = target_node_type or "Unknown"
            self.next_node_id += 1
        
        source_id = self.node_to_id[source]
        target_id = self.node_to_id[target]
        
        # Add to adjacency list
        self.adjacency_list[source_id].append((target_id, edge_type))
        self.valid_edges.append((source_id, target_id, edge_type))
    
    def build_csr(self):
        """
        Stage 5 & 6: Build Modified CSR Format
        Converts adjacency list to Compressed Sparse Row format with type offsets.
        """
        # Sort edges by (source, type, target)
        self.valid_edges.sort(key=lambda e: (e[0], e[2], e[1]))
        
        # Initialize CSR structures
        self.row_pointers = [0] * (self.next_node_id + 1)
        self.column_indices = []
        self.type_offset_index = defaultdict(lambda: defaultdict(tuple))
        
        current_node = -1
        position = 0
        
        for source, target, edge_type in self.valid_edges:
            # Start new node block
            if source != current_node:
                current_node = source
                self.row_pointers[source] = position
            
            # Track type offsets
            if edge_type not in self.type_offset_index[source]:
                # First edge of this type from this node
                self.type_offset_index[source][edge_type] = (position, position + 1)
            else:
                # Extend the end of existing type block
                start, _ = self.type_offset_index[source][edge_type]
                self.type_offset_index[source][edge_type] = (start, position + 1)
            
            self.column_indices.append(target)
            position += 1
        
        # Set final row pointer
        self.row_pointers[self.next_node_id] = position
    
    def compute_typed_degree(self, edge_type: str):
        """
        Stage 8: Compute Typed Degree
        Calculate degree of each node considering only specified edge type.
        """
        self.degree = {}
        
        for node_id in range(self.next_node_id):
            if edge_type in self.type_offset_index[node_id]:
                start, end = self.type_offset_index[node_id][edge_type]
                self.degree[node_id] = end - start
            else:
                self.degree[node_id] = 0
    
    def initialize_kcore_structures(self):
        """
        Stage 7: Initialize K-Core Structures
        Set up removed and in_queue tracking.
        """
        self.removed = {node_id: False for node_id in range(self.next_node_id)}
        self.in_queue = {node_id: False for node_id in range(self.next_node_id)}
        self.queue = deque()
    
    def find_kcore_community(self, k: int, edge_type: str) -> List[str]:
        """
        Main algorithm: K-Core Community Search with typed edges.
        
        Args:
            k: Minimum degree threshold
            edge_type: Only consider edges of this type
        
        Returns:
            List of node names in the k-core community
        """
        start_time = time.time()
        
        # Build CSR format
        print(f"[Step 1] Building Modified CSR format...")
        self.build_csr()
        print(f"  ✓ CSR built: {len(self.valid_edges)} edges, {self.next_node_id} nodes")
        
        # Compute typed degrees
        print(f"[Step 2] Computing typed degrees for edge_type='{edge_type}'...")
        self.compute_typed_degree(edge_type)
        print(f"  ✓ Degree computation complete")
        
        # Initialize structures
        print(f"[Step 3] Initializing k-core structures...")
        self.initialize_kcore_structures()
        
        # Stage 9: Identify initial weak nodes
        print(f"[Step 4] Identifying weak nodes (degree < {k})...")
        weak_count = 0
        for node_id in range(self.next_node_id):
            if self.degree.get(node_id, 0) < k:
                self.queue.append(node_id)
                self.in_queue[node_id] = True
                weak_count += 1
        print(f"  ✓ {weak_count} weak nodes added to queue")
        
        # Stage 10: K-core peeling
        print(f"[Step 5] Performing k-core peeling...")
        iterations = 0
        removed_count = 0
        
        while self.queue:
            u = self.queue.popleft()
            self.in_queue[u] = False
            
            if self.removed[u]:
                continue
            
            self.removed[u] = True
            removed_count += 1
            iterations += 1
            
            # Get typed neighbours
            if edge_type not in self.type_offset_index[u]:
                continue
            
            start, end = self.type_offset_index[u][edge_type]
            
            # Update neighbour degrees
            for pos in range(start, end):
                v = self.column_indices[pos]
                if not self.removed[v]:
                    self.degree[v] = self.degree[v] - 1
                    
                    if self.degree[v] < k and not self.in_queue[v]:
                        self.queue.append(v)
                        self.in_queue[v] = True
        
        print(f"  ✓ {iterations} iterations, {removed_count} nodes removed")
        
        # Stage 11: Extract final community
        print(f"[Step 6] Extracting final k-core community...")
        community_ids = []
        for node_id in range(self.next_node_id):
            if not self.removed[node_id]:
                community_ids.append(node_id)
        
        community_names = [self.id_to_node[nid] for nid in community_ids]
        
        elapsed = time.time() - start_time
        print(f"  ✓ Community size: {len(community_names)} nodes")
        print(f"  ✓ Total time: {elapsed:.4f}s\n")
        
        return community_names
    
    def get_statistics(self) -> Dict:
        """Return dataset statistics."""
        node_types = defaultdict(int)
        for node_type in self.node_type_map.values():
            node_types[node_type] += 1
        
        edge_types = defaultdict(int)
        for _, _, edge_type in self.valid_edges:
            edge_types[edge_type] += 1
        
        return {
            "total_nodes": self.next_node_id,
            "total_edges": len(self.valid_edges),
            "node_types": dict(node_types),
            "edge_types": dict(edge_types)
        }


def generate_synthetic_dblp(num_authors=500, num_papers=800, num_venues=50):
    """
    Generate synthetic DBLP-like dataset for testing.
    
    HIN Structure:
    - Author nodes
    - Paper nodes  
    - Venue nodes
    
    Edge types:
    - Author-writes-Paper
    - Paper-published-in-Venue
    """
    import random
    
    print("=" * 70)
    print("GENERATING SYNTHETIC DBLP DATASET")
    print("=" * 70)
    
    hin = HINKCoreSearch()
    
    # Create authors
    authors = [f"Author_{i}" for i in range(num_authors)]
    
    # Create papers and connect to authors
    print(f"\n[1/3] Creating {num_papers} papers and Author-writes-Paper edges...")
    papers = []
    author_paper_edges = 0
    
    for p in range(num_papers):
        paper_name = f"Paper_{p}"
        papers.append(paper_name)
        
        # Each paper has 1-3 authors
        num_authors_for_paper = random.randint(1, 3)
        paper_authors = random.sample(authors, num_authors_for_paper)
        
        for author in paper_authors:
            hin.add_edge(author, paper_name, "writes", "Author", "Paper")
            author_paper_edges += 1
    
    print(f"✓ Added {author_paper_edges} Author-writes-Paper edges")
    
    # Create venues and connect papers
    print(f"\n[2/3] Creating {num_venues} venues and Paper-published-in-Venue edges...")
    venues = [f"Venue_{i}" for i in range(num_venues)]
    paper_venue_edges = 0
    
    for paper in papers:
        venue = random.choice(venues)
        hin.add_edge(paper, venue, "published_in", "Paper", "Venue")
        paper_venue_edges += 1
    
    print(f"✓ Added {paper_venue_edges} Paper-published-in-Venue edges")
    
    # Add some reverse edges for realism (Venue-has-Paper)
    print(f"\n[3/3] Creating reverse edges for bidirectional relationships...")
    reverse_edges = 0
    for paper in papers:
        # Find which venue this paper was published in
        for source_id, target_id, etype in hin.valid_edges:
            if etype == "published_in" and hin.id_to_node[source_id] == paper:
                venue_name = hin.id_to_node[target_id]
                hin.add_edge(venue_name, paper, "has_paper", "Venue", "Paper")
                reverse_edges += 1
                break
    
    print(f"✓ Added {reverse_edges} Venue-has-Paper edges")
    
    stats = hin.get_statistics()
    print(f"\n" + "=" * 70)
    print("DATASET STATISTICS")
    print("=" * 70)
    print(f"Total nodes: {stats['total_nodes']}")
    print(f"Total edges: {stats['total_edges']}")
    print(f"Node types: {stats['node_types']}")
    print(f"Edge types: {stats['edge_types']}")
    print("=" * 70)
    
    return hin


def test_kcore_search():
    """Run complete test on synthetic DBLP dataset."""
    
    print("\n" + "=" * 70)
    print("HIN K-CORE COMMUNITY SEARCH - DBLP DATASET TEST")
    print("=" * 70)
    
    # Generate synthetic DBLP
    hin = generate_synthetic_dblp(num_authors=500, num_papers=800, num_venues=50)
    
    # Test different k values and edge types
    test_cases = [
        {"k": 2, "edge_type": "writes", "description": "Small communities (Author-Paper)"},
        {"k": 3, "edge_type": "writes", "description": "Medium communities (Author-Paper)"},
        {"k": 1, "edge_type": "has_paper", "description": "Venue communities"},
    ]
    
    results = []
    
    for test in test_cases:
        print("\n" + "-" * 70)
        print(f"TEST: {test['description']}")
        print(f"Parameters: k={test['k']}, edge_type='{test['edge_type']}'")
        print("-" * 70)
        
        community = hin.find_kcore_community(test['k'], test['edge_type'])
        
        # Analyze community composition
        node_type_dist = defaultdict(int)
        for node_name in community:
            node_id = hin.node_to_id[node_name]
            node_type = hin.node_type_map[node_id]
            node_type_dist[node_type] += 1
        
        result = {
            "k": test['k'],
            "edge_type": test['edge_type'],
            "community_size": len(community),
            "composition": dict(node_type_dist),
            "sample_nodes": community[:10]  # First 10 nodes
        }
        results.append(result)
        
        print(f"Community composition: {dict(node_type_dist)}")
        print(f"Sample nodes: {community[:5]}")
    
    return results, hin


def export_results(results, hin):
    """Export results to JSON and create summary."""
    
    print("\n" + "=" * 70)
    print("RESULTS SUMMARY")
    print("=" * 70)
    
    summary = {
        "dataset": {
            "total_nodes": hin.get_statistics()['total_nodes'],
            "total_edges": hin.get_statistics()['total_edges'],
            "node_types": hin.get_statistics()['node_types'],
            "edge_types": hin.get_statistics()['edge_types']
        },
        "kcore_results": results
    }
    
    # Print formatted results
    for i, result in enumerate(results, 1):
        print(f"\nTest {i}: k={result['k']}, edge_type='{result['edge_type']}'")
        print(f"  Community size: {result['community_size']} nodes")
        print(f"  Composition: {result['composition']}")
        print(f"  Density: {result['community_size'] / summary['dataset']['total_nodes'] * 100:.2f}% of total")
    
    print("\n" + "=" * 70)
    
    # Save to file
    with open('/home/claude/kcore_results.json', 'w') as f:
        json.dump(summary, f, indent=2)
    
    print(f"✓ Results saved to /home/claude/kcore_results.json")
    
    return summary


if __name__ == "__main__":
    results, hin = test_kcore_search()
    summary = export_results(results, hin)
