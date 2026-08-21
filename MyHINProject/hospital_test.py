from hin_kcore_search import HINKCoreSearch
import csv

# Load hospital data
hin = HINKCoreSearch()

with open('hospital_data.csv') as f:
    reader = csv.DictReader(f)
    for row in reader:
        hin.add_edge(
            row['source'],
            row['target'],
            row['edge_type'],
            row['source_type'],
            row['target_type']
        )

# Show stats
print("="*60)
print("HOSPITAL DATASET LOADED")
print("="*60)
stats = hin.get_statistics()
print(f"Total Nodes: {stats['total_nodes']}")
print(f"Total Edges: {stats['total_edges']}")
print(f"Node Types: {stats['node_types']}")
print(f"Edge Types: {stats['edge_types']}")
print()

# Find doctors treating 2+ patients
print("="*60)
print("FINDING TREATMENT COMMUNITIES (k=2)")
print("="*60)
community = hin.find_kcore_community(k=2, edge_type="treated_by")
print(f"\nDoctors treating ≥2 patients: {community}")
print(f"Community size: {len(community)}")