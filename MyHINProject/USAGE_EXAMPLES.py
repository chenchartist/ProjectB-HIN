"""
Quick Start Examples: HIN K-Core Community Search
Ready-to-use templates for your project
"""

from hin_kcore_search import HINKCoreSearch
from collections import defaultdict
import json


# ============================================================================
# EXAMPLE 1: Basic Usage - Load Edges from CSV
# ============================================================================

def example_1_load_from_csv():
    """Load HIN from CSV file and find communities."""
    
    print("\n" + "="*70)
    print("EXAMPLE 1: Loading HIN from CSV")
    print("="*70)
    
    hin = HINKCoreSearch()
    
    # Example CSV format:
    # source,target,edge_type,source_type,target_type
    # Author_1,Paper_A,writes,Author,Paper
    # Author_1,Paper_B,writes,Author,Paper
    # Paper_A,Venue_X,published_in,Paper,Venue
    
    edges = [
        ("Author_1", "Paper_A", "writes", "Author", "Paper"),
        ("Author_1", "Paper_B", "writes", "Author", "Paper"),
        ("Author_2", "Paper_A", "writes", "Author", "Paper"),
        ("Author_2", "Paper_C", "writes", "Author", "Paper"),
        ("Author_3", "Paper_B", "writes", "Author", "Paper"),
        ("Author_3", "Paper_C", "writes", "Author", "Paper"),
        ("Paper_A", "Venue_X", "published_in", "Paper", "Venue"),
        ("Paper_B", "Venue_Y", "published_in", "Paper", "Venue"),
        ("Paper_C", "Venue_X", "published_in", "Paper", "Venue"),
    ]
    
    print(f"Loading {len(edges)} edges...")
    for source, target, etype, stype, ttype in edges:
        hin.add_edge(source, target, etype, stype, ttype)
    
    stats = hin.get_statistics()
    print(f"✓ Loaded {stats['total_nodes']} nodes, {stats['total_edges']} edges")
    print(f"  Node types: {stats['node_types']}")
    print(f"  Edge types: {stats['edge_types']}")
    
    # Find authors who wrote ≥2 papers
    community = hin.find_kcore_community(k=2, edge_type="writes")
    print(f"\nAuthors who wrote ≥2 papers: {community}")
    
    return hin


# ============================================================================
# EXAMPLE 2: Hospital HIN (Your Project!)
# ============================================================================

def example_2_hospital_hin():
    """Template for hospital heterogeneous information network."""
    
    print("\n" + "="*70)
    print("EXAMPLE 2: Hospital HIN - Doctor-Patient Communities")
    print("="*70)
    
    hin = HINKCoreSearch()
    
    # Hospital data: Doctors treating patients
    hospital_data = [
        # Doctor treats Patient
        ("Dr_Smith", "Patient_001", "treats", "Doctor", "Patient"),
        ("Dr_Smith", "Patient_002", "treats", "Doctor", "Patient"),
        ("Dr_Smith", "Patient_003", "treats", "Doctor", "Patient"),
        ("Dr_Johnson", "Patient_002", "treats", "Doctor", "Patient"),
        ("Dr_Johnson", "Patient_003", "treats", "Doctor", "Patient"),
        ("Dr_Johnson", "Patient_004", "treats", "Doctor", "Patient"),
        ("Dr_Lee", "Patient_001", "treats", "Doctor", "Patient"),
        ("Dr_Lee", "Patient_004", "treats", "Doctor", "Patient"),
        ("Dr_Lee", "Patient_005", "treats", "Doctor", "Patient"),
        
        # Patient diagnosed with Condition
        ("Patient_001", "Condition_Diabetes", "diagnosed_with", "Patient", "Condition"),
        ("Patient_002", "Condition_Hypertension", "diagnosed_with", "Patient", "Condition"),
        ("Patient_003", "Condition_Diabetes", "diagnosed_with", "Patient", "Condition"),
        ("Patient_004", "Condition_Arthritis", "diagnosed_with", "Patient", "Condition"),
        ("Patient_005", "Condition_Hypertension", "diagnosed_with", "Patient", "Condition"),
        
        # Doctor specializes in Department
        ("Dr_Smith", "Department_Endocrinology", "works_in", "Doctor", "Department"),
        ("Dr_Johnson", "Department_Cardiology", "works_in", "Doctor", "Department"),
        ("Dr_Lee", "Department_Rheumatology", "works_in", "Doctor", "Department"),
    ]
    
    print(f"Loading {len(hospital_data)} hospital relationships...")
    for source, target, etype, stype, ttype in hospital_data:
        hin.add_edge(source, target, etype, stype, ttype)
    
    stats = hin.get_statistics()
    print(f"✓ Loaded {stats['total_nodes']} entities")
    print(f"  Doctors, Patients, Conditions, Departments")
    print(f"  Relationships: {stats['edge_types']}\n")
    
    # Query 1: Doctors treating multiple patients
    print("Query 1: Doctors treating ≥2 patients (collaborative care)")
    doctors = hin.find_kcore_community(k=2, edge_type="treats")
    print(f"Result: {doctors}")
    
    print("\n(In real hospital data, this finds high-volume treatment teams)")
    
    return hin


# ============================================================================
# EXAMPLE 3: Analyzing Community Structure
# ============================================================================

def example_3_community_analysis():
    """Detailed analysis of discovered community."""
    
    print("\n" + "="*70)
    print("EXAMPLE 3: Deep Dive - Community Composition Analysis")
    print("="*70)
    
    hin = HINKCoreSearch()
    
    # Create a larger network
    edges = [
        # Tier 1: Highly productive authors
        ("A1", "P1", "writes", "Author", "Paper"),
        ("A1", "P2", "writes", "Author", "Paper"),
        ("A1", "P3", "writes", "Author", "Paper"),
        ("A1", "P4", "writes", "Author", "Paper"),
        
        ("A2", "P1", "writes", "Author", "Paper"),
        ("A2", "P2", "writes", "Author", "Paper"),
        ("A2", "P5", "writes", "Author", "Paper"),
        ("A2", "P6", "writes", "Author", "Paper"),
        
        # Tier 2: Moderately productive
        ("A3", "P3", "writes", "Author", "Paper"),
        ("A3", "P4", "writes", "Author", "Paper"),
        ("A3", "P7", "writes", "Author", "Paper"),
        
        ("A4", "P5", "writes", "Author", "Paper"),
        ("A4", "P6", "writes", "Author", "Paper"),
        
        # Tier 3: Low productivity (will be filtered out)
        ("A5", "P8", "writes", "Author", "Paper"),
        ("A6", "P9", "writes", "Author", "Paper"),
        
        # Papers published in venues
        ("P1", "V1", "published_in", "Paper", "Venue"),
        ("P2", "V1", "published_in", "Paper", "Venue"),
        ("P3", "V2", "published_in", "Paper", "Venue"),
        ("P4", "V2", "published_in", "Paper", "Venue"),
        ("P5", "V1", "published_in", "Paper", "Venue"),
        ("P6", "V3", "published_in", "Paper", "Venue"),
        ("P7", "V2", "published_in", "Paper", "Venue"),
        ("P8", "V3", "published_in", "Paper", "Venue"),
        ("P9", "V1", "published_in", "Paper", "Venue"),
    ]
    
    for source, target, etype, stype, ttype in edges:
        hin.add_edge(source, target, etype, stype, ttype)
    
    print("Network created: Authors → Papers → Venues\n")
    
    # Find communities at different k-levels
    for k in [1, 2, 3, 4]:
        community = hin.find_kcore_community(k, "writes")
        print(f"k={k}: {len(community)} authors in core")
        print(f"       Members: {sorted(community)}")
        
        # Calculate average productivity
        total_papers = 0
        for author in community:
            author_id = hin.node_to_id[author]
            if "writes" in hin.type_offset_index[author_id]:
                start, end = hin.type_offset_index[author_id]["writes"]
                total_papers += (end - start)
        
        avg = total_papers / len(community) if community else 0
        print(f"       Avg papers/author: {avg:.1f}\n")


# ============================================================================
# EXAMPLE 4: Export Results to JSON
# ============================================================================

def example_4_export_results():
    """Save community results for external analysis."""
    
    print("\n" + "="*70)
    print("EXAMPLE 4: Export Results to JSON")
    print("="*70)
    
    hin = HINKCoreSearch()
    
    # Minimal example
    edges = [
        ("A", "P1", "writes", "Author", "Paper"),
        ("A", "P2", "writes", "Author", "Paper"),
        ("B", "P1", "writes", "Author", "Paper"),
        ("B", "P3", "writes", "Author", "Paper"),
        ("C", "P2", "writes", "Author", "Paper"),
        ("C", "P3", "writes", "Author", "Paper"),
    ]
    
    for source, target, etype, stype, ttype in edges:
        hin.add_edge(source, target, etype, stype, ttype)
    
    # Find community
    community = hin.find_kcore_community(k=2, edge_type="writes")
    
    # Export to JSON
    result = {
        "query": {
            "k": 2,
            "edge_type": "writes"
        },
        "dataset": hin.get_statistics(),
        "community": {
            "members": community,
            "size": len(community),
            "percentage_of_network": len(community) / hin.next_node_id * 100
        }
    }
    
    with open('/home/claude/example_results.json', 'w') as f:
        json.dump(result, f, indent=2)
    
    print("Results exported to example_results.json:")
    print(json.dumps(result, indent=2))


# ============================================================================
# EXAMPLE 5: Comparing Different k Values
# ============================================================================

def example_5_sensitivity_analysis():
    """See how results change with different k values."""
    
    print("\n" + "="*70)
    print("EXAMPLE 5: Sensitivity Analysis - How k affects results")
    print("="*70)
    
    hin = HINKCoreSearch()
    
    # Create synthetic data with clear structure
    edges = [
        # Core group: 3 authors, each with 5 papers
        ("CoreA", "P1", "writes", "Author", "Paper"),
        ("CoreA", "P2", "writes", "Author", "Paper"),
        ("CoreA", "P3", "writes", "Author", "Paper"),
        ("CoreA", "P4", "writes", "Author", "Paper"),
        ("CoreA", "P5", "writes", "Author", "Paper"),
        
        ("CoreB", "P1", "writes", "Author", "Paper"),
        ("CoreB", "P2", "writes", "Author", "Paper"),
        ("CoreB", "P6", "writes", "Author", "Paper"),
        ("CoreB", "P7", "writes", "Author", "Paper"),
        ("CoreB", "P8", "writes", "Author", "Paper"),
        
        ("CoreC", "P3", "writes", "Author", "Paper"),
        ("CoreC", "P4", "writes", "Author", "Paper"),
        ("CoreC", "P6", "writes", "Author", "Paper"),
        ("CoreC", "P7", "writes", "Author", "Paper"),
        ("CoreC", "P9", "writes", "Author", "Paper"),
        
        # Peripheral: 2 authors with fewer papers
        ("PeriphA", "P5", "writes", "Author", "Paper"),
        ("PeriphA", "P8", "writes", "Author", "Paper"),
        ("PeriphA", "P10", "writes", "Author", "Paper"),
        
        ("PeriphB", "P9", "writes", "Author", "Paper"),
        ("PeriphB", "P10", "writes", "Author", "Paper"),
    ]
    
    for source, target, etype, stype, ttype in edges:
        hin.add_edge(source, target, etype, stype, ttype)
    
    print("Authors: CoreA (5 papers), CoreB (5), CoreC (5), PeriphA (3), PeriphB (2)")
    print("Examining how community changes with k:\n")
    
    results_summary = []
    
    for k_value in range(1, 6):
        community = hin.find_kcore_community(k_value, "writes")
        
        # Categorize members
        core = [m for m in community if m.startswith("Core")]
        periph = [m for m in community if m.startswith("Periph")]
        
        result = {
            "k": k_value,
            "community_size": len(community),
            "core_members": len(core),
            "peripheral_members": len(periph),
            "members": sorted(community)
        }
        results_summary.append(result)
        
        print(f"k={k_value}: {len(community)} authors")
        print(f"         Core: {core}, Peripheral: {periph}")
    
    print("\n📊 ANALYSIS:")
    print("  k=1-2: Full network (everyone has at least 1-2 papers)")
    print("  k=3:   Core authors remain, periph starts being filtered")
    print("  k=4-5: Only core authors (high productivity threshold)")


# ============================================================================
# MAIN: Run All Examples
# ============================================================================

if __name__ == "__main__":
    
    print("\n" + "="*70)
    print("HIN K-CORE SEARCH - USAGE EXAMPLES")
    print("="*70)
    
    # Run examples
    example_1_load_from_csv()
    example_2_hospital_hin()
    example_3_community_analysis()
    example_4_export_results()
    example_5_sensitivity_analysis()
    
    print("\n" + "="*70)
    print("✓ All examples completed successfully!")
    print("="*70)
    print("\nNext steps:")
    print("1. Adapt these examples to your own data")
    print("2. Load real edges from your CSV/database")
    print("3. Experiment with different k values")
    print("4. Analyze the discovered communities")
    print("\nDocumentation: HIN_KCORE_ANALYSIS_REPORT.md")
    print("="*70 + "\n")
