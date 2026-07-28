#include "autocattery/autocattery.hpp"
#include <cassert>
#include <iostream>
using namespace autocattery;
CatSnapshot Cat(CatId id,int v,RoomId room){CatSnapshot c;c.id=id;c.display_name="fixture";c.life_stage=LifeStage::Adult;c.room_id=room;c.available_for_combat=TriState::Yes;c.available_for_breeding=TriState::Yes;c.base_stats={v,v,v,v,v,v};return c;}
int main(){
 HouseSnapshot h;h.snapshot_id=1;h.cats={Cat(1,8,1),Cat(2,5,1),Cat(3,2,2),Cat(4,1,2)};
 h.cats[3].protection=ProtectionLevel::NoCullOrMove;
 RoomSnapshot r1;r1.id=1;r1.room_type="breeding";r1.residents={1,2};r1.hard_capacity=2;r1.soft_capacity=2;r1.allows_breeding=true;
 RoomSnapshot r2;r2.id=2;r2.room_type="combat_staging";r2.residents={3,4};r2.hard_capacity=3;r2.soft_capacity=2;
 h.rooms={r1,r2};AlgorithmConfig cfg;cfg.combat.recommended_count=1;cfg.breeding.core_breeders=1;cfg.breeding.reserve_breeders=0;cfg.classification.minimum_combat_pool=1;cfg.classification.minimum_breeding_pool=1;cfg.classification.minimum_general_reserve=0;
 auto s1=ScoreCombat(h.cats[0],cfg.combat),s2=ScoreCombat(h.cats[1],cfg.combat);assert(s1.score>s2.score);
 auto cp=ClassifyCats(h,cfg);assert(cp.decisions.size()==4);for(auto&d:cp.decisions)if(d.cat_id==4)assert(!d.destructive_action_allowed);
 auto rp=PlanRooms(h,cp,cfg.rooms);assert(rp.snapshot_id==1);for(auto&m:rp.moves)assert(m.cat_id!=4);
 std::cout<<"autocattery_core_tests: OK\n";return 0;
}
