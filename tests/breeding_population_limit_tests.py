"""Deterministic retention: compatible pairs, combat reserve, genetic quality."""
from pathlib import Path
from types import SimpleNamespace
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from breeding_daily import NativeDailySimulation, SimulationConfig


class PopulationFixture:
    trim_population = NativeDailySimulation.trim_population
    _remove_ids = NativeDailySimulation._remove_ids
    select_pairs = NativeDailySimulation.select_pairs

    def __init__(self, limit=150):
        self.config = SimulationConfig(population_limit=limit)
        self.day = 50
        self.pedigree = SimpleNamespace(coi=lambda a, b: 0.0)
        self.cats = {key: key for key in range(1, 181)}
        self.cat_rooms = {key: "room" for key in self.cats}
        self.native = SimpleNamespace(effective_stats=lambda key: [100 if key <= 8 else 7] * 7)

    def _candidate_pairs(self):
        return [((), 179, 180), ((), 177, 178)]

    def _dead(self, cat):
        return False

    def _breedable(self, cat):
        return True

    def _stats(self, cat):
        return [7 if 9 <= cat <= 160 else 3] * 7

    def _all_seven(self, cat):
        return self._stats(cat) == [7] * 7

    def _read(self, cat, offset, kind):
        assert offset == 0xC38 and kind == "<q"
        return 40


def main():
    fixture = PopulationFixture()
    removed = fixture.trim_population()
    assert len(fixture.cats) == 150 and len(removed) == 30
    assert set(range(1, 9)) <= fixture.cats.keys()
    assert {177, 178, 179, 180} <= fixture.cats.keys()
    assert set(range(161, 177)) <= set(removed)
    assert not set(removed) & fixture.cat_rooms.keys()
    assert fixture.trim_population() == []
    second = PopulationFixture()
    second.cats = dict(reversed(list(second.cats.items())))
    assert second.trim_population() == removed
    small = PopulationFixture(4)
    small.trim_population()
    assert set(small.cats) == {177, 178, 179, 180}
    invalid = PopulationFixture(3)
    try:
        invalid.trim_population()
    except ValueError:
        pass
    else:
        raise AssertionError("A limit below two breeding pairs must be rejected")
    print("PASS population limit: exact 150, compatible pairs, combat reserve, genetic selection, deterministic")


if __name__ == "__main__":
    main()
