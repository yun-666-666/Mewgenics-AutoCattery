#pragma once
#include "domain.hpp"
namespace autocattery {
ScoreResult ScoreCombat(const CatSnapshot&, const CombatConfig&);
ScoreResult ScoreBreeding(const CatSnapshot&, const BreedingConfig&);
std::vector<ScoreResult> RankScores(std::vector<ScoreResult>, const HouseSnapshot&);
}
