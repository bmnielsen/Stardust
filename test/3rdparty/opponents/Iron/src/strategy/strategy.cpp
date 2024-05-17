//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "strategy.h"
#include "walling.h"
#include "firstDepotPlacement.h"
#include "secondDepotPlacement.h"
#include "firstBarracksPlacement.h"
#include "firstFactoryPlacement.h"
#include "groupAttack.h"
#include "groupAttackSCV.h"
#include "mineAttack.h"
#include "wraithAttack.h"
#include "scan.h"
#include "workerDefense.h"
#include "workerRush.h"
#include "zerglingRush.h"
#include "zealotRush.h"
#include "marineRush.h"
#include "dragoonRush.h"
#include "cannonRush.h"
#include "wraithRush.h"
#include "terranFastExpand.h"
#include "freeTurrets.h"
#include "earlyRunBy.h"
#include "shallowTwo.h"
#include "lateCore.h"
#include "unblockTraffic.h"
#include "patrolBases.h"
#include "mineSpots.h"
#include "baseDefense.h"
#include "killMines.h"
#include "fight.h"
#include "surround.h"
#include "berserker.h"
#include "expand.h"
#include "../units/cc.h"
#include "../units/fightSim.h"
#include "../units/production.h"
#include "../units/army.h"
#include "../behavior/scouting.h"
#include "../behavior/mining.h"
#include "../behavior/refining.h"
#include "../behavior/supplementing.h"
#include "../behavior/walking.h"
#include "../behavior/constructing.h"
#include "../behavior/harassing.h"
#include "../behavior/exploring.h"
#include "../behavior/executing.h"
#include "../behavior/fleeing.h"
#include "../behavior/chasing.h"
#include "../behavior/laying.h"
#include "../behavior/vchasing.h"
#include "../behavior/defaultBehavior.h"
#include "../Iron.h"
#include <numeric>

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Strategy
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Strategy::Strategy()
: m_HisPossibleLocations(ai()->GetMap().StartingLocations())
{
	m_Strats.push_back(make_unique<Walling>());
	m_Strats.push_back(make_unique<FirstDepotPlacement>());
	m_Strats.push_back(make_unique<SecondDepotPlacement>());
	m_Strats.push_back(make_unique<FirstBarracksPlacement>());
//	m_Strats.push_back(make_unique<FirstFactoryPlacement>());
	m_Strats.push_back(make_unique<WorkerDefense>());
	m_Strats.push_back(make_unique<WorkerRush>());
	m_Strats.push_back(make_unique<ZerglingRush>());
//	m_Strats.push_back(make_unique<ZealotRush>());
	m_Strats.push_back(make_unique<MarineRush>());
//	m_Strats.push_back(make_unique<DragoonRush>());
	m_Strats.push_back(make_unique<CannonRush>());
	m_Strats.push_back(make_unique<WraithRush>());
	m_Strats.push_back(make_unique<TerranFastExpand>());
	m_Strats.push_back(make_unique<EarlyRunBy>());
//	m_Strats.push_back(make_unique<LateCore>());
	m_Strats.push_back(make_unique<FreeTurrets>());
	m_Strats.push_back(make_unique<UnblockTraffic>());
	m_Strats.push_back(make_unique<PatrolBases>());
	m_Strats.push_back(make_unique<Expand>());
	m_Strats.push_back(make_unique<MineSpots>());
	m_Strats.push_back(make_unique<KillMines>());
//	m_Strats.push_back(make_unique<BaseDefense>());	TODO revoir
	m_Strats.push_back(make_unique<AirFight>());
	m_Strats.push_back(make_unique<GroundFight>()); // after AirFight
//	m_Strats.push_back(make_unique<Surround>());
	m_Strats.push_back(make_unique<Berserker>());

	const TilePosition myStartingPos = me().Player()->getStartLocation();

	swap(m_HisPossibleLocations.front(), *find(m_HisPossibleLocations.begin(), m_HisPossibleLocations.end(), myStartingPos));
	assert_throw(m_HisPossibleLocations.front() == myStartingPos);

	const Area * pMyStartingArea = ai()->GetMap().GetArea(myStartingPos);
	really_remove_if(m_HisPossibleLocations, [pMyStartingArea](TilePosition t)
		{ return !ai()->GetMap().GetArea(t)->AccessibleFrom(pMyStartingArea); });

	// sorts m_HisPossibleLocations, making each element the nearest one from the previous one
	for (int i = 1 ; i < (int)m_HisPossibleLocations.size() ; ++i)
	{
		TilePosition lastPos = m_HisPossibleLocations[i-1];
		for (int j = i+1 ; j < (int)m_HisPossibleLocations.size() ; ++j)
		{
			int groundDist_lastPos_i;
			int groundDist_lastPos_j;
			ai()->GetMap().GetPath(Position(lastPos), Position(m_HisPossibleLocations[i]), &groundDist_lastPos_i);
			ai()->GetMap().GetPath(Position(lastPos), Position(m_HisPossibleLocations[j]), &groundDist_lastPos_j);
			if (groundDist_lastPos_j < groundDist_lastPos_i)
				swap(m_HisPossibleLocations[i], m_HisPossibleLocations[j]);
		}
	}

	really_remove(m_HisPossibleLocations, myStartingPos);

	//	assert_throw_plus(!m_HisPossibleLocations.empty(), "enemy must be accessible");
	if (m_HisPossibleLocations.empty()) him().SetNotAccessible();
}


void Strategy::RemovePossibleLocation(TilePosition pos)
{
	assert_throw(contains(m_HisPossibleLocations, pos));
	really_remove(m_HisPossibleLocations, pos);
}


void Strategy::OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit)
{
	for (auto & strat : m_Strats)
		strat->OnBWAPIUnitDestroyed(pBWAPIUnit);
}


bool Strategy::TimeToScout() const
{
	if (auto s = ai()->GetStrategy()->Active<Walling>())
		if (me().Units(Terran_Vulture).empty())
			if (!him().AllUnits(Zerg_Zergling).empty())
				return false;

	if (ai()->Frame() > 1)
	if (!him().IsTerran() && !him().IsProtoss())
		if (!ai()->GetStrategy()->Has<Walling>())
			{
				if (ai()->GetMap().StartingLocations().size() == 3) return ai()->Frame() >= 600;
				if (ai()->GetMap().StartingLocations().size() >= 4) return true;
			}
	

	if (auto s = ai()->GetStrategy()->Active<Walling>())
		if (him().IsProtoss())
		{
			//ert
			if ((s->GetWall()->Size() == 1) || (s->GetWall()->Size() == 2))
				return !me().Buildings(Terran_Supply_Depot).empty() &&
						(me().Buildings(Terran_Supply_Depot).front()->RemainingBuildTime() < 300);

			return !me().Buildings(Terran_Barracks).empty() &&
						((me().Buildings(Terran_Barracks).front()->RemainingBuildTime() < 300) ||
						(me().SupplyUsed() >= 14));
		}
		else
			return me().CompletedBuildings(Terran_Supply_Depot) >= 2;

/* //ert
	if (me().GetWall() && me().GetWall()->Active())
		if ((me().GetWall()->Size() == 1) || (me().GetWall()->Size() == 2))
			return me().SupplyUsed() >= 11;
*/
	return me().Buildings(Terran_Barracks).size() >= 1;
}


bool Strategy::TimeToBuildFirstShop() const
{
	if (me().Buildings(Terran_Machine_Shop).size() >= 1) return false;
//	return true;//drop

	int minVulturesBeforeShops = 3;
	if (him().IsProtoss())
	{
		minVulturesBeforeShops = max(2, int((him().AllUnits(Protoss_Zealot).size() + 1)/2));

		if (!him().AllUnits(Protoss_Dragoon).empty())
			minVulturesBeforeShops = 0;

		if (him().AllUnits(Protoss_Zealot).size() <= 1)
			minVulturesBeforeShops = 0;

		if ((him().Buildings(Protoss_Nexus).size() >= 2) ||
			((him().Buildings(Protoss_Nexus).size() == 1) &&
				!contains(ai()->GetMap().StartingLocations(), him().Buildings(Protoss_Nexus).front()->TopLeft())))
			minVulturesBeforeShops = 0;

		
/*
		if		(him().MayDarkTemplar()) minVulturesBeforeShops = 0;
		else if	(me().Army().TankRatioWanted() >= 7) minVulturesBeforeShops = 0;
		else if (me().Army().TankRatioWanted() >= 5) minVulturesBeforeShops = 1;
		else if (me().Army().TankRatioWanted() >= 3) minVulturesBeforeShops = 2;
*/
	}

	if (him().SpeedLings()) minVulturesBeforeShops = 1;

	if ((int)me().Units(Terran_Vulture).size() < minVulturesBeforeShops)
		return false;

	return true;
}


bool Strategy::RessourceDispatch()
{
	assert_throw(!me().Bases().empty());
	const VBase * main = me().StartingVBase();

	int currentRefiners = Refining::Instances().size();

	int maxRefiners = 0;
	for (const VBase * base : me().Bases())
		maxRefiners += base->MaxRefiners();

	int wantedRefiners = currentRefiners;

	if (me().Buildings(Terran_Factory).size() <= 1)
	{
		if ((me().GasAvailable() < 140) && (me().SCVworkers() >= 13) && (me().CreationCount(Terran_Vulture) == 0))
		{
			wantedRefiners = main->MaxRefiners();

			if (me().Buildings(Terran_Factory).size() == 1)
			{
				if (!him().IsProtoss())
					wantedRefiners = min(2, wantedRefiners);
				else
					if (me().Buildings(Terran_Factory).back()->RemainingBuildTime() > 50)
						wantedRefiners = 1;
			}
		}
	}
	else
	{
		const bool useAvailableRatio = max(me().Production().MineralsAvailable(), me().Production().GasAvailable()) >= 100;
		const double g = 0.5;
		const double ratio1 = useAvailableRatio ? me().Production().AvailableRatio() : 1.0;

		if		(ratio1 < 1*g) { wantedRefiners = max(0, currentRefiners - 1); }//bw << ratio1 << " < " << 1*g << " --> " << wantedRefiners << endl; }
		else if (ratio1 > 1/g) { wantedRefiners = min(maxRefiners, currentRefiners + 1); }//bw << ratio1 << " > " << 1/g << " --> " << wantedRefiners << endl; }
		else
		{
			const double h = 0.8;
			const double ratio2 = gatheringRatio();

			if		(ratio2 < 5*h) { wantedRefiners = max(0, currentRefiners - 1); }//bw << ratio2 << " < " << 5*h << " --> " << wantedRefiners << endl; }
			else if (ratio2 > 5/h) { wantedRefiners = min(maxRefiners, currentRefiners + 1); }//bw << ratio2 << " > " << 5/h << " --> " << wantedRefiners << endl; }
		}
	}

	if (auto s = ai()->GetStrategy()->Detected<MarineRush>())
		if (!((me().Buildings(Terran_Factory).size() == 0) && (me().GasAvailable() < 100)))
			wantedRefiners = 0;

	if (auto s = ai()->GetStrategy()->Detected<ZerglingRush>())
		if (me().Buildings(Terran_Factory).size() <= 1)
			if (s->TechRestartingCondition())
			{
				if (me().GasAvailable() >= 100) wantedRefiners = 1;
				else wantedRefiners = 3;
				
				if		(me().MineralsAvailable() < 50) wantedRefiners = min(1, wantedRefiners);
				else if (me().MineralsAvailable() < 100) wantedRefiners = min(2, wantedRefiners);
			}
			else
				wantedRefiners = 0;

	if (auto s = ai()->GetStrategy()->Detected<ZealotRush>())
			wantedRefiners = 0;

	if (currentRefiners < wantedRefiners)
	{
		const VBase * pBestBase = nullptr;
		int maxGasAmount = numeric_limits<int>::min();
		for (const VBase * base : me().Bases())
			if (base->LackingRefiners() > 0)
				if (!base->Miners().empty())
					if (base->GasAmount() > maxGasAmount)
					{
						maxGasAmount = base->GasAmount();
						pBestBase = base;
					}

		if (pBestBase)
			for (Mining * m : pBestBase->Miners())
				if (m->MovingTowardsMineral())
				{
					m->Agent()->ChangeBehavior<Refining>(m->Agent());
					return true;
				}
	}
	else if (currentRefiners > wantedRefiners)
	{
		const VBase * pBestBase = nullptr;
		int minGasAmount = numeric_limits<int>::max();
		for (const VBase * base : me().Bases())
			if (base->LackingMiners() > 0)
				if (!base->Refiners().empty())
					if (base->GasAmount() < minGasAmount)
					{
						minGasAmount = base->GasAmount();
						pBestBase = base;
					}

		if (pBestBase)
			for (Refining * r : pBestBase->Refiners())
				if (r->MovingTowardsRefinery())
				{
					r->Agent()->ChangeBehavior<Mining>(r->Agent());
					return true;
				}
	}

	return false;
}


bool Strategy::MiningDispatch()
{
	if (me().Bases().size() < 2) return false;

	if (ai()->GetStrategy()->Active<Walling>()) return false;

	const VBase * pLowestRateBase = nullptr;
	const VBase * pHighestRateBase = nullptr;
	int minLackingMiners = numeric_limits<int>::max();
	int maxLackingMiners = numeric_limits<int>::min();

	int maxLackingMinersInMain = 1000;
	if (him().IsProtoss())
		if (me().Army().KeepTanksAtHome())
			if (me().Bases().size() == 2)
			{
				maxLackingMinersInMain = 2;
				if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 >= (int)(him().AllUnits(Protoss_Dragoon).size())*3 ||
						me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 6)
					if (me().CompletedUnits(Terran_Vulture) >= (int)him().AllUnits(Protoss_Zealot).size()*2 ||
							((me().CompletedUnits(Terran_Vulture) >= (int)him().AllUnits(Protoss_Zealot).size())
							&& (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 4)))
						maxLackingMinersInMain = 1000;
			}

	for (const VBase * base : me().Bases())
		if (base->Active())
		{
			if (base->LackingMiners() < minLackingMiners)
			{
				minLackingMiners = base->LackingMiners();
				pHighestRateBase = base;
			}

			if (base->LackingMiners() > maxLackingMiners)
				if (base->OtherSCVs() < 3)
				{
					maxLackingMiners = base->LackingMiners();
					pLowestRateBase = base;
				}
		}

	if (minLackingMiners < maxLackingMinersInMain)
		if (pLowestRateBase && pHighestRateBase)
			if (pLowestRateBase != pHighestRateBase)
				if (maxLackingMiners >= 3)
					if (maxLackingMiners - minLackingMiners >= 2)
						for (Mining * m : pHighestRateBase->Miners())
							if (m->MovingTowardsMineral())
							{
								My<Terran_SCV> * pSCV = m->Agent();
								pSCV->ChangeBehavior<Exploring>(pSCV, pLowestRateBase->GetArea()->BWEMPart());
								pSCV->LeaveStronghold();
								pSCV->EnterStronghold(pLowestRateBase->GetStronghold());
								pSCV->ChangeBehavior<Mining>(pSCV);
								return true;
							}

	return false;
}


static void cancelFreeTurrets()
{
	if (MyBuilding * b = me().FindBuildingNeedingBuilder(Terran_Missile_Turret, !bool("inStrongHold")))
		if (b->CanAcceptCommand())
		{
		///	ai()->SetDelay(5000);
		///	bw << "CancelConstruction of " << b->NameWithId() << endl;
			b->CancelConstruction();
		}
}


void Strategy::OnFrame(bool preBehavior)
{
	for (auto & strat : m_Strats)
		if (preBehavior == strat->PreBehavior())
			exceptionHandler("Strategy::OnFrame(" + strat->Name() + ")", 5000, [&strat]()
			{
				strat->OnFrame();
			});

	really_remove_if(m_Strats, [](const unique_ptr<Strat> & strat){ return strat->ToRemove(); });


	if (preBehavior) return;

/*
	static const int N = 500000;// + 5*(rand()%100);
	if (him().Accessible())
		if (ai()->Frame() == N)
		{
			for (int i = 0 ; i < 1 ; ++i)
			{
				auto * pScout = me().Units(Terran_SCV)[i]->As<Terran_SCV>();
				pScout->ChangeBehavior<Scouting>(pScout);
			}
		}
*/



//	bw << ai()->Frame() << ") " << bw->getLatencyFrames() << " ; " << bw->getRemainingLatencyFrames() << endl;

//	DO_ONCE	bw << me().Player()->damage(UnitType(Terran_Vulture_Spider_Mine).groundWeapon()) << endl;


	
	if (TimeToScout())
		if (me().SCVsoldiers() < m_minScoutingSCVs)
			if ((Scouting::InstancesCreated() == 0) ||					// initial scout
				!him().StartingBase() ||								// initial scout was killed before his StartingVBase was found.
				ai()->GetStrategy()->Detected<CannonRush>() ||			// CannonRush Strat involves creating several Scouting SCVs.
				ai()->GetStrategy()->Detected<WorkerRush>() ||			// CannonRush Strat involves creating several Scouting SCVs.
				ai()->GetStrategy()->Has<ZerglingRush>()				// always have m_minScoutingSCVs Scouting SCVs until ZerglingRush is discarded
				//||m_stoneAttackActive
				)
				if (My<Terran_SCV> * pWorker = findFreeWorker(me().StartingVBase()))
				{
				//	pWorker->SetSoldierForever();
					return pWorker->ChangeBehavior<Scouting>(pWorker);
				}

/*
	for (const auto & b : him().Buildings())
		if (b->Is(Zerg_Spawning_Pool))
			DO_ONCE
			{
				ai()->SetDelay(500);
				bw << "Stone attack" << endl;
				ai()->GetStrategy()->SetMinScoutingSCVs(Mining::Instances().size());
				m_stoneAttackActive = true;
				break;
			}
*/

	exceptionHandler("Strategy::marineHandlingOnZerglingOrHydraPressure()", 2000, [this]()
	{
		marineHandlingOnZerglingOrHydraPressure();
	});

	exceptionHandler("Strategy::groupAttackSCV", 2000, [this]()
	{
		groupAttackSCV();
	});
/*
	exceptionHandler("Strategy::WorkerDefense", 2000, [this]()
	{
		WorkerRush::WorkerDefense();
	});
*/
	exceptionHandler("Strategy::groupAttack", 2000, [this]()
	{
		groupAttack();
	});

	exceptionHandler("Strategy::mineAttack", 2000, [this]()
	{
		mineAttack();
	});

	exceptionHandler("Strategy::wraithAttack", 2000, [this]()
	{
		wraithAttack();
	});

	exceptionHandler("Strategy::valkyrieAttack()", 2000, [this]()
	{
		valkyrieAttack();
	});

	exceptionHandler("Strategy::vesselAttack()", 2000, [this]()
	{
		vesselAttack();
	});

	exceptionHandler("Strategy::scan", 2000, [this]()
	{
		scan();
	});

	exceptionHandler("Strategy::cancelFreeTurrets", 2000, [this]()
	{
		cancelFreeTurrets();
	});


	for (VBase * base : me().Bases())
		if (base->CheckSupplementerAssignment())
			return;


	if (RessourceDispatch()) return;


	if (MiningDispatch()) return;
/*
	if (me().CompletedUnits(Terran_Medic) > 0)
		for (auto & u : me().Units(Terran_Marine))
			if (Exploring * e = u->GetBehavior()->IsExploring())
				if (e->Where() == findNatural(me().StartingVBase())->BWEMPart()->GetArea())
					u->ChangeBehavior<DefaultBehavior>(u.get());
*/

/*
	if (ai()->Delay() == 1000)
		for (const auto & u : me().Units(Terran_Vulture))
			if (u->Completed())
				if (u->Unit()->getSpiderMineCount() > 0)
					if (u->Pos() != u->PrevPos(30))
						if (!u->GetBehavior()->IsLaying())
							return u->ChangeBehavior<Laying>(u.get());
*/
/*
	if (ai()->Delay() == 1000)
		for (const auto & u : me().Units(Terran_Vulture))
			if (u->Completed())
				if (u->Pos() != u->PrevPos(30))
					if (!u->GetBehavior()->IsExecuting())
						return u->ChangeBehavior<Executing>(u.get());
*/

/*
	if (me().Units(Terran_Marine).size() >= 3)
		DO_ONCE
		{
			ai()->SetDelay(50);
			him().SetHasCannons();
		}
*/


/*
	DO_ONCE
	{

		Position pos = center(me().GetArea()->Top());

//		Timer t;
		FightSim Sim(zone_t::ground);
//		Sim.AddMyUnit(Terran_Marine, 40, 0, pos, 0);
//		Sim.AddMyUnit(Terran_Marine, 40, 0, pos, 0);
//		Sim.AddMyUnit(Terran_Marine, 40, 0, pos, 0);
//		Sim.AddMyUnit(Terran_Vulture, 80, 0, pos, 0);
//		Sim.AddMyUnit(Terran_Vulture, 80, 0, pos, 0);
//		Sim.AddMyUnit(Terran_Vulture, 80, 0, pos, 0);
//		Sim.AddMyUnit(Terran_Vulture, 80, 0, pos, 0);
		Sim.AddMyUnit(Terran_Wraith, 120, 0, pos, 0);


//		Sim.AddHisUnit(Zerg_Mutalisk, 120, 0, pos);
//		Sim.AddHisUnit(Terran_Vulture, 80, 0, pos);
		Sim.AddHisUnit(Terran_Missile_Turret, 200, 0, pos);
//		Sim.AddHisUnit(Protoss_Dragoon, 100, 80, pos);
//		Sim.AddHisUnit(Terran_Battlecruiser, 500, 0, pos);
//		Sim.AddHisUnit(Terran_Goliath, 125, 0, pos);
//		Sim.AddHisUnit(Terran_Siege_Tank_Tank_Mode, 150, 0, pos);
//		Sim.AddHisUnit(Terran_Siege_Tank_Siege_Mode, 150, 0, pos);
		Sim.Eval();
//		bw << t.ElapsedMilliseconds() << endl;
	}
*/


/*
	DO_ONCE
	{
		bw << "groundRange = " << groundRange(Terran_SCV, him().Player()) << endl;
	}
*/

/*
	static int yy;
	if (me().CompletedUnits(Terran_Marine) == 2)
	for (const auto & u : me().Units(Terran_Marine))
		if (u->Completed())
			DO_ONCE
			{
				int d = groundDist(u.get()->Pos(), him().StartingBase()->Center());
				yy = ai()->Frame();
				bw << "dist = " << d << endl;
				bw << "expected time = " << d / UnitType(Terran_Marine).topSpeed() << endl;
				u->ChangeBehavior<Walking>(u.get(), him().StartingBase()->Center(), __FILE__ + to_string(__LINE__));
				break;
			}

	for (const auto & u : me().Units(Terran_Marine))
		if (dist(u->Pos(), him().StartingBase()->Center()) < 10*32)
			DO_ONCE
			{
				bw << "real time = " << ai()->Frame() - yy << endl;
			}
*/
//	me().GetVArea()->ComputeProtossWalling();

//	for (VArea & area : ai()->GetVMap().Areas())
//		area.Initialize();


	static bool said_IAmStone = false;
	if (!said_IAmStone)
		if ((200 < ai()->Frame()) && (ai()->Frame() < 1200))
			for (const auto & b : him().Buildings())
				if (!b->InFog())
					if (b->Type().isResourceDepot())
						for (const auto & u : me().Units(Terran_SCV))
							if (dist(u->Pos(), b->Pos()) < 10*32)
							{
								said_IAmStone = true;
								if (rand() % 2 == 0)
								{
									if		(rand() % 2 == 0) { bw->sendText("I am not Stone"); }
									else if (rand() % 2 == 0) { bw->sendText("Do you know Stone?"); }
									else if (rand() % 2 == 0) { bw->sendText("Do you remember Stone?"); }
									else if (rand() % 2 == 0) { bw->sendText("I am Stone"); }
								}
							}
							
}


} // namespace iron



