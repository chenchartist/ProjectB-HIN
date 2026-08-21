from hin_kcore_search import HINKCoreSearch

# Create HIN
hin = HINKCoreSearch()

# Add hospital data
hin.add_edge("Patient_001", "Doctor_A", "treats", "Patient", "Doctor")
hin.add_edge("Patient_002", "Doctor_A", "treats", "Patient", "Doctor")
hin.add_edge("Patient_003", "Doctor_A", "treats", "Patient", "Doctor")
hin.add_edge("Patient_001", "Doctor_B", "treats", "Patient", "Doctor")
hin.add_edge("Patient_002", "Doctor_B", "treats", "Patient", "Doctor")

# Show stats
stats = hin.get_statistics()
print(f"Nodes: {stats['total_nodes']}")
print(f"Edges: {stats['total_edges']}")

# Find doctors treating 2+ patients
community = hin.find_kcore_community(k=2, edge_type="treats")
print(f"\nDoctors treating ≥2 patients: {community}")