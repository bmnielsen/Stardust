//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "mineAttack.h"
#include "baseDefense.h"
#include "../units/factory.h"
#include "../behavior/mining.h"
#include "../behavior/destroying.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


static bool targetForMineAttack(UnitType type)
{
	return
		(type == Terran_Siege_Tank_Siege_Mode) ||
		(type == Terran_Siege_Tank_Tank_Mode) ||
		(type == Terran_Goliath) ||
		(type == Protoss_Dragoon);
}



static pair<int, string> computeMineAttackScore(vector<MyUnit *> & Candidates, const vector<HisUnit *> & EnemyGroup, int hinderingEnemies)
{
	if (findMyClosestBase(EnemyGroup.front()->Pos()))
	{
		if (Candidates.size() < EnemyGroup.size()) return make_pair(-1000, "-");
	}
	else
	{
		if (all_of(EnemyGroup.begin(), EnemyGroup.end(), [](const HisUnit * u){ return u->LifeWithShields() < 50; }))
			 return make_pair(-1000, "-");

		int minAttackers = ((int)EnemyGroup.size() + hinderingEnemies)*2;
		minAttackers -= count_if(EnemyGroup.begin(), EnemyGroup.end(), [](const HisUnit * u){ return u->Is(Terran_Siege_Tank_Siege_Mode); });

		int maxAttackers = minAttackers + 1;

		if (minAttackers > 10) return make_pair(-1000, "-");

		if (maxAttackers >= 6)
			if (!(
					(me().SupplyUsed() >= min(190, 80 + 20*(minAttackers-5)))
				))
				return make_pair(-1000, "-");

		if ((int)Candidates.size() < minAttackers) return make_pair(-1000, "-");

		if ((int)Candidates.size() > maxAttackers) Candidates.resize(maxAttackers);
	}

	int bloodScore = EnemyGroup.size();

	int score = bloodScore;

	string scoreDetail	=
					to_string(score) +
					" = blood: " + to_string(bloodScore);

	return make_pair(score, scoreDetail);
}



void mineAttack()
{
	static frame_t sAttackLastTime = 0;
	if (ai()->Frame() - sAttackLastTime < 200) return;

	if (!me().HasResearched(TechTypes::Spider_Mines)) return;

	if (him().IsProtoss())
		if (me().CompletedBuildings(Terran_Command_Center) >= 2)
			return;


	map<HisUnit *, vector<MyUnit *>> map_Target_Candidates;
	map<HisUnit *, vector<HisUnit *>> map_Target_EnemyGroup;
	map<HisUnit *, int> map_Target_hinderingEnemies;

	exceptionHandler("(1)", 2000, [&map_Target_EnemyGroup, &map_Target_Candidates, &map_Target_hinderingEnemies]()
	{
	for (auto & u : him().Units())
		if (!u->InFog())
			if (targetForMineAttack(u->Type()))
				if (u->Is(Terran_Siege_Tank_Siege_Mode) || findMyClosestBase(u->Pos()))
					if (u->Chasers().empty())
					if (u->VChasers().empty())
					if (u->Destroyers().empty())
						if (u->MinesTargetingThis().empty())
						{
							bool dangerousBuildingsAround = false;
							for (auto & neigh : him().Buildings())
								if (neigh->GroundThreatBuilding())
									if (roundedDist(u->Pos(), neigh->Pos()) < 8*32)
										{ dangerousBuildingsAround = true; break; }
							if (dangerousBuildingsAround) continue;

							int hinderingEnemies = 0;

							for (auto & neigh : him().Units())
								if (neigh->GroundAttack())
									if (!neigh->Type().isWorker())
									{
										int airD = roundedDist(u->Pos(), neigh->Pos());
										if (!targetForMineAttack(neigh->Type()))
										{
											if (airD < 13*32) ++hinderingEnemies;
										}
										else
										{
											int groundD = groundDist(u->Pos(), neigh->Pos());
											if (groundD < 4*32) map_Target_EnemyGroup[u.get()].push_back(neigh.get());
											else if (airD < 15*32) ++hinderingEnemies;
										}
									}

							for (const auto & info : him().AllUnits())
								if (!info.second.Type().isBuilding())
									if (!info.first->isVisible())
										if (groundAttack(info.second.Type(), info.first->getPlayer()))
											if (!info.second.Type().isWorker())
											{
												int airD = roundedDist(u->Pos(), info.second.LastPosition());
												if (airD < 15*32) ++hinderingEnemies;
											}

							const int enemyGroupSize = map_Target_EnemyGroup[u.get()].size();

							if (hinderingEnemies >= enemyGroupSize) continue;

							map_Target_Candidates[u.get()].reserve(5);
							map_Target_hinderingEnemies[u.get()] = hinderingEnemies;
						}
	});

	vector<MyUnit *> Candidates;

	exceptionHandler("(2)", 2000, [&Candidates]()
	{
	for (MyUnit * u : me().Units())
		if (u->Is(Terran_Vulture))
			if (u->Completed() && !u->Loaded())
				if (u->IsMy<Terran_Vulture>()->RemainingMines() >= 2)
				//	if ((u->Life() >= 50) || !u->WorthBeingRepaired())
						if (!u->GetBehavior()->IsVChasing())
						if (!u->GetBehavior()->IsDestroying())
						if (!u->GetBehavior()->IsLaying())
						if (!u->GetBehavior()->IsKillingMine())
							Candidates.push_back(u);
	});

	exceptionHandler("(3)", 2000, [&Candidates, &map_Target_EnemyGroup, &map_Target_Candidates]()
	{
	for (MyUnit * u : Candidates)
	{
		bool dangerousBuildingsAround = false;
		for (const FaceOff & faceOff : u->FaceOffs())
			if (faceOff.His()->IsHisBuilding())
				if (faceOff.DistanceToHisRange() < 5*32) { dangerousBuildingsAround = true; break; }
		if (dangerousBuildingsAround) continue;

		HisUnit * pClosestEnemy = nullptr;
		int minDist = numeric_limits<int>::max();
		for (const FaceOff & faceOff : u->FaceOffs())
			if (HisUnit * pHisUnit = faceOff.His()->IsHisUnit())
				if (faceOff.AirDistanceToHitHim() < minDist)
				{
					minDist = faceOff.AirDistanceToHitHim();
					pClosestEnemy = pHisUnit;
				}

		if (pClosestEnemy)
			for (const FaceOff & faceOff : u->FaceOffs())
				if (HisUnit * pHisUnit = faceOff.His()->IsHisUnit())
				{
					auto it = map_Target_Candidates.find(pHisUnit);
					if (it == map_Target_Candidates.end()) continue;

					//if (faceOff.GroundDistanceToHitHim() > 16*32) continue;
					if (!faceOff.MyAttack()) continue;
					if (!faceOff.HisAttack()) continue;
					if (!contains(map_Target_EnemyGroup[pHisUnit], pClosestEnemy)) continue;
					if (!((u->Life() >= 50) || !u->WorthBeingRepaired()) && !findMyClosestBase(pHisUnit->Pos()))
						continue;
					assert_throw(!contains(it->second, u));

					it->second.push_back(u);
				}
	}
	});


	multimap<int, pair<HisUnit *, string>> OrderedTargets;

	exceptionHandler("(4)", 2000, [&OrderedTargets, &map_Target_EnemyGroup, &map_Target_Candidates, &map_Target_hinderingEnemies]()
	{
	for (auto & a : map_Target_Candidates)
	{
		auto score = computeMineAttackScore(a.second, map_Target_EnemyGroup[a.first], map_Target_hinderingEnemies[a.first]);
		if (score.first >= 1)
			OrderedTargets.emplace(score.first, make_pair(a.first, score.second));
	}
	});

//14066: Exception in (5): An error occurred in d:\prog\starcraft\iron\iron\units\bwapiUnits.h, line 66 - 00000000
//14110: Exception in (5): An error occurred in d:\prog\starcraft\iron\iron\units\bwapiUnits.h, line 66 - 00000000

	exceptionHandler("(5)", 2000, [&OrderedTargets, &map_Target_EnemyGroup, &map_Target_Candidates]()
	{
	for (auto it = OrderedTargets.crbegin() ; it != OrderedTargets.crend() ; ++it)
	{
		const int score = it->first;
		::unused(score);
//		HisUnit * pTarget = it->second.first;
		auto & candidates = map_Target_Candidates[it->second.first];
		auto & EnemyGroup = map_Target_EnemyGroup[it->second.first];
		really_remove_if(EnemyGroup, [](HisUnit * u){ return u->InFog(); });

			
		for (auto u : candidates)
		{
			assert_throw_plus(u, "null"); // ici
			auto uu = u->IsMy<Terran_Vulture>();
			assert_throw_plus(uu, u->Type().toString().c_str());
			u->ChangeBehavior<Destroying>(uu, EnemyGroup);
		}
/*
		{
			for (int r = 29 ; r <= 30 ; ++r)
				bw->drawBoxMap(pTarget->Pos() - r, pTarget->Pos() + r, Colors::Yellow);
			string caption = it->second.second;
//			string caption = to_string(score);
			bw->drawTextMap(pTarget->Pos() - Position(30, 42), "%c%s", Text::Yellow, caption.c_str());
					
			{
				bw << ai()->Frame() << ") " << score << " mine-attack " << pTarget->NameWithId();
				for (auto u : candidates)
					bw << "  " << u->Unit()->getID();
				bw << endl; ai()->SetDelay(2000);
			}
		}
*/

		sAttackLastTime = ai()->Frame();
		break;
	}
	});
}




} // namespace iron



