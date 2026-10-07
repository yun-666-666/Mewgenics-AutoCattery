"""Keep distinct families even when same-family pairs have higher scores."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_population import Pedigree, select_lineage_pairs

pedigree = Pedigree([(i, None, None, 0.0) for i in range(1, 9)])
pedigree.add(9, 1, 2)
pedigree.add(10, 3, 4)
pedigree.add(11, 1, 2)
pedigree.add(12, 3, 4)
ranked = [(100, 9, 10), (99, 11, 12), (50, 5, 6)]
# Each top pair has COI zero, but their offspring would be relatives.
assert pedigree.coi(9, 10) == pedigree.coi(11, 12) == 0
assert select_lineage_pairs(ranked, 2, pedigree.coi) == [(9, 10), (5, 6)]
assert select_lineage_pairs(ranked, 2, pedigree.coi, False) == [(9, 10), (11, 12)]
assert select_lineage_pairs(ranked, 1, pedigree.coi) == [(9, 10)]
assert select_lineage_pairs(ranked, 0, pedigree.coi) == []
assert select_lineage_pairs(ranked[:2], 2, pedigree.coi) == [(9, 10), (11, 12)]
pedigree.add(13, 9, 10)
pedigree.add(14, 5, 6)
assert pedigree.coi(13, 14) == 0
print("PASS distinct-family selection preserves unrelated future offspring and respects settings")
