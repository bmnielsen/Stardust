//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "my.h"
#include "production.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/mining.h"
#include "../behavior/constructing.h"
#include "../behavior/repairing.h"
#include "../behavior/harassing.h"
#include "../behavior/healing.h"
#include "../territory/stronghold.h"
#include "../territory/vgridMap.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../strategy/expand.h"
#include "../interactive.h"
#include "../Iron.h"
#include "cc.h"
#include "depot.h"
#include "refinery.h"
#include "bunker.h"
#include "barracks.h"
#include "factory.h"
#include "shop.h"
#include "starport.h"
#include "tower.h"
#include "comsat.h"
#include "ebay.h"
#include "academy.h"
#include "armory.h"
#include "scienceFacility.h"
#include "turret.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MyUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


unique_ptr<MyUnit> MyUnit::Make(BWAPI::Unit u)
{
	switch (u->getType())
	{
		#define MYUNIT_MAKE_CASE(tid)   case tid: return make_unique<My<tid>>(u);
		MYUNIT_MAKE_CASE(Terran_SCV)
		MYUNIT_MAKE_CASE(Terran_Marine)
		MYUNIT_MAKE_CASE(Terran_Medic)
		MYUNIT_MAKE_CASE(Terran_Vulture)
		MYUNIT_MAKE_CASE(Terran_Vulture_Spider_Mine)
		MYUNIT_MAKE_CASE(Terran_Siege_Tank_Tank_Mode)
		MYUNIT_MAKE_CASE(Terran_Goliath)
		MYUNIT_MAKE_CASE(Terran_Wraith)
		MYUNIT_MAKE_CASE(Terran_Valkyrie)
		MYUNIT_MAKE_CASE(Terran_Science_Vessel)
		MYUNIT_MAKE_CASE(Terran_Dropship)
	}

	assert_throw_plus(false, "could not make " + u->getType().getName());
	return nullptr;
}
	

MyUnit::MyUnit(BWAPI::Unit u, unique_ptr<IBehavior> pBehavior)
	: MyBWAPIUnit(u, move(pBehavior))
{
	ai()->GetGridMap().Add(this);
}


MyUnit::~MyUnit()
{
#if !DEV
	try //3
#endif
	{
		ai()->GetGridMap().Remove(this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void MyUnit::Update()
{CI(this);
	ai()->GetGridMap().Remove(this);
	MyBWAPIUnit::Update();
	ai()->GetGridMap().Add(this);

	{
		vector<HisBWAPIUnit *> HisBWAPIUnits;
		for (auto & u : him().Units())
			HisBWAPIUnits.push_back(u.get());

		for (auto & b : him().Buildings())	
			if (b->Completed())
				HisBWAPIUnits.push_back(b.get());

		const int minDistanceToRange = 10*32;
		m_FaceOffs.clear();
		if (!Loaded())
			for (HisBWAPIUnit * his : HisBWAPIUnits)
			//if (!(his->IsHisBuilding() && his->IsHisBuilding()->InFog()))
			{
//				if (his->IsHisBuilding() && his->IsHisBuilding()->InFog())
//					bw << "o" << endl;
				m_FaceOffs.emplace_back(this, his);
				if ((!m_FaceOffs.back().MyAttack() && !m_FaceOffs.back().HisAttack()) ||
					((m_FaceOffs.back().DistanceToMyRange() > minDistanceToRange) && (m_FaceOffs.back().DistanceToHisRange() > minDistanceToRange)))
					m_FaceOffs.pop_back();
			}
	}
/*
	{
		vector<MyBWAPIUnit *> MyBWAPIUnits;
		for (auto & u : me().Units()) 	 MyBWAPIUnits.push_back(u);
		for (auto & b : me().Buildings())	 MyBWAPIUnits.push_back(b);

		const int minDistanceToRange = 10*32;
		m_FaceOffs.clear();
		for (MyBWAPIUnit * my : MyBWAPIUnits) if (this != my)
		{
			m_FaceOffs.emplace_back(this, my);
			if ((!m_FaceOffs.back().MyAttack() && !m_FaceOffs.back().HisAttack()) ||
				((m_FaceOffs.back().DistanceToMyRange() > minDistanceToRange) && (m_FaceOffs.back().DistanceToHisRange() > minDistanceToRange)))
				m_FaceOffs.pop_back();
		}
	}
*/
}


Healing * MyUnit::GetHealer() const
{CI(this);
	for (Healing * pHealer : Healing::Instances())
		if (pHealer->Target() == this)
			return pHealer;

	return nullptr;
}


void MyUnit::StimPack(check_t checkMode)
{CI(this);
	assert_throw(me().HasResearched(TechTypes::Stim_Packs));
	assert_throw(!Unit()->isStimmed());
	assert_throw(Unit()->canUseTechWithoutTarget(TechTypes::Stim_Packs));
//	bw << NameWithId() << " stimmed! " << endl;
	bool result = Unit()->useTech(TechTypes::Stim_Packs);
	OnCommandSent(checkMode, result, NameWithId() + " stimPack ");
}


void MyUnit::OnDangerHere()
{CI(this);
	m_lastDangerReport = ai()->Frame();
}


bool MyUnit::AllowedToChase() const
{CI(this);
	return ai()->Frame() >= m_noChaseUntil;
}


void MyUnit::SetNoChaseFor(frame_t time)
{CI(this);
	m_noChaseUntil = ai()->Frame() + time;
}


bool MyUnit::WorthBeingRepaired() const
{CI(this);

	if (Unit()->isCloaked() && Life() >= MaxLife() / 4)
		return false;

	return true;
}

Position MyUnit::GetRaidingTarget() const
{
	if (Interactive::raidingDefaultTarget != Positions::None)
		return Interactive::raidingDefaultTarget;

	if (Is(Terran_Marine) || Is(Terran_Medic))
		if (him().IsProtoss())
		{
			MyUnit * pClosestSiegedTank = nullptr;
			int minDist = 20*32;
			for (const auto & u : me().Units(Terran_Siege_Tank_Tank_Mode))
				if (u->Completed() && !u->Loaded())
					if (u->Is(Terran_Siege_Tank_Siege_Mode) || u->GetBehavior()->IsSieging())
					{
						int d = groundDist(Pos(), u->Pos());
						if (d < minDist)
						{
							minDist = d;
							pClosestSiegedTank = u.get();
						}
					}
			if (pClosestSiegedTank)
			{
			//	bw->drawTriangleMap(pClosestSiegedTank->Pos()+Position(0, -30), pClosestSiegedTank->Pos()+Position(-25, 20), pClosestSiegedTank->Pos()+Position(+25, 20), Colors::White);
				return pClosestSiegedTank->Pos();
			}
						

			if (him().NaturalArea())
				if (him().HasBuildingsInNatural())
					return center(him().NaturalArea()->Top());
		}

	if (const My<Terran_Dropship> * pDropship = IsMy<Terran_Dropship>())
		if (pDropship->GetDamage() == My<Terran_Dropship>::damage_t::require_repair)
		{
			map<int, Position> Candidates;
			for (int times = 0 ; times < 100 ; ++ times)
			{
				Position pos = ai()->GetMap().RandomPosition();
				const auto & Cell = ai()->GetGridMap().GetCell(TilePosition(pos));

				int score = 0;

				score += abs(roundedDist(pos, Pos()) - 30*32);

				score += 1000000 * Cell.AvgHisUnitsAndBuildingsLast256Frames();

				const Area * pArea = ai()->GetMap().GetArea(TilePosition(pos));
				if (!pArea || (him().GetArea() == pArea))
					score += 1000000000;

				for (const auto & b : him().Buildings())
					score += 10000 * max(0, 30*32 - roundedDist(pos, b->Pos()));

				for (const auto & u : him().AllUnits())
					score += 10000 * max(0, 30*32 - roundedDist(pos, u.second.LastPosition()));

				Candidates.emplace(score, pos);
			}
		///	bw << Candidates.begin()->first << " " << Candidates.begin()->second << endl;
		///	ai()->SetDelay(2000);
			return Candidates.begin()->second;
		}

	if (him().IsZerg())
		if (Is(Terran_Vulture) || Is(Terran_Wraith))
		{
			vector<const HisBuilding *> HisHatcheries;
			for (UnitType type : {Zerg_Hatchery, Zerg_Lair, Zerg_Hive})
				for (const auto & b : him().Buildings(type))
					HisHatcheries.push_back(b);

			if (HisHatcheries.size() >= 3)
				return HisHatcheries[rand() % HisHatcheries.size()]->Pos();
		}

	if (him().IsProtoss())
		if (Is(Terran_Vulture) || Is(Terran_Wraith))
		{
			if (him().Buildings(Protoss_Nexus).size() >= 3)
				return him().Buildings(Protoss_Nexus)[rand() % him().Buildings(Protoss_Nexus).size()]->Pos();
		}

	if (him().IsTerran())
		if (Is(Terran_Vulture) || Is(Terran_Wraith))
		{
			if (him().Buildings(Terran_Command_Center).size() >= 3)
				return him().Buildings(Terran_Command_Center)[rand() % him().Buildings(Terran_Command_Center).size()]->Pos();
		}

	return him().StartingBase()->Center();
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MyBuilding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


unique_ptr<MyBuilding> MyBuilding::Make(BWAPI::Unit u)
{
	switch (u->getType())
	{
		#define MYBUILDING_MAKE_CASE(tid)   case tid: return make_unique<My<tid>>(u);
		MYBUILDING_MAKE_CASE(Terran_Command_Center)
		MYBUILDING_MAKE_CASE(Terran_Refinery)
		MYBUILDING_MAKE_CASE(Terran_Bunker)
		MYBUILDING_MAKE_CASE(Terran_Supply_Depot)
		MYBUILDING_MAKE_CASE(Terran_Barracks)
		MYBUILDING_MAKE_CASE(Terran_Factory)
		MYBUILDING_MAKE_CASE(Terran_Machine_Shop)
		MYBUILDING_MAKE_CASE(Terran_Starport)
		MYBUILDING_MAKE_CASE(Terran_Control_Tower)
		MYBUILDING_MAKE_CASE(Terran_Comsat_Station)
		MYBUILDING_MAKE_CASE(Terran_Engineering_Bay)
		MYBUILDING_MAKE_CASE(Terran_Academy)
		MYBUILDING_MAKE_CASE(Terran_Armory)
		MYBUILDING_MAKE_CASE(Terran_Missile_Turret)
		MYBUILDING_MAKE_CASE(Terran_Science_Facility)
	}

	assert_throw_plus(false, "could not make " + u->getType().getName());
	return nullptr;
}


MyBuilding::MyBuilding(BWAPI::Unit u, unique_ptr<IBehavior> pBehavior)
	: MyBWAPIUnit(u, move(pBehavior)), m_size(u->getType().tileSize())
{
	if (!Flying()) PutBuildingOnTiles();

	ai()->GetGridMap().Add(this);
	if (auto s = ai()->GetStrategy()->Active<Walling>())
		for (int i = 0 ; i < s->GetWall()->Size() ; ++i)
			if (TopLeft() == s->GetWall()->Locations()[i])
			{
				m_pWall = s->GetWall();
				m_indexInWall = i;
			}
}


MyBuilding::~MyBuilding()
{
#if !DEV
	try //3
#endif
	{
		if (!Flying()) RemoveBuildingFromTiles();
	
		ai()->GetGridMap().Remove(this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void MyBuilding::Update()
{CI(this);
	if (Type().isAddon())
		if (Unit()->getPosition() == Positions::Unknown)
			return;

	ai()->GetGridMap().Remove(this);
	MyBWAPIUnit::Update();
	ai()->GetGridMap().Add(this);

	if (JustLifted()) RemoveBuildingFromTiles();

	if (JustLanded())
	{
		PutBuildingOnTiles();

		if (Is(Terran_Command_Center))
			if (auto s = ai()->GetStrategy()->Active<Expand>())
				if (s->LiftCC())
					if (GetStronghold() != me().Bases().back()->GetStronghold())
					{
						LeaveStronghold();
						EnterStronghold(me().Bases().back()->GetStronghold());
					}

	}

	if (ai()->Frame() % 100 == 0)
	{
		m_activity <<= 1;
		if (Unit()->isTraining()) m_activity |= 1;
	}
}


void MyBuilding::RecordTrainingExperts(Production & Prod)
{CI(this);
	for (auto & e : m_TrainingExperts)
		Prod.Record(e.get());
}


void MyBuilding::RecordResearchingExperts(Production & Prod)
{CI(this);
	for (auto & e : m_ResearchingExperts)
		Prod.Record(e.get());
}


int MyBuilding::IdlePosition() const
{CI(this);
	vector<const MyBuilding *> List;
	for (const auto & b : me().Buildings(Type()))
		List.push_back(b.get());

	sort(List.begin(), List.end(), [](const MyBuilding * a, const MyBuilding * b)
		{
			if (a->TimeToTrain() < b->TimeToTrain()) return true;
			if (a->TimeToTrain() > b->TimeToTrain()) return false;
			return a < b;
		});

	return (int)distance(List.begin(), find(List.begin(), List.end(), this));
}


int MyBuilding::Activity() const
{CI(this);

	uint_least32_t mask20bits = (uint_least32_t(1) << 20) - 1;
	return numberOfSetBits(m_activity & mask20bits) / 2;
}


Constructing * MyBuilding::CurrentBuilder() const
{
	for (Constructing * builder : Constructing::Instances())
		if (builder->Location() == TopLeft())
			return builder;

	return nullptr;
}


void MyBuilding::Train(BWAPI::UnitType type, check_t checkMode)
{CI(this);
	assert_throw(me().CanPay(type));

	bool result = Unit()->train(type);
	OnCommandSent(checkMode, result, NameWithId() + " trains " + type.getName());
}


void MyBuilding::Research(TechType type, check_t checkMode)
{CI(this);
	assert_throw(me().CanPay(type));

	bool result = Unit()->research(type);
	OnCommandSent(checkMode, result, NameWithId() + " researches " + type.getName());
}


void MyBuilding::Research(UpgradeType type, check_t checkMode)
{CI(this);
	assert_throw(me().CanPay(type));

	bool result = Unit()->upgrade(type);
	OnCommandSent(checkMode, result, NameWithId() + " upgrades " + type.getName());
}


void MyBuilding::BuildAddon(BWAPI::UnitType type, check_t checkMode)
{CI(this);
	assert_throw(me().CanPay(type));

///	bw << NameWithId() << " builds Addon " << type << endl;
	bool result = Unit()->buildAddon(type);
	OnCommandSent(checkMode, result, NameWithId() + " builds Addon " + type.getName());
}


void MyBuilding::Lift(check_t checkMode)
{CI(this);
	bool result = Unit()->lift();
	OnCommandSent(checkMode, result, NameWithId() + " lift");
	if (result) m_liftedSince = ai()->Frame();
}


bool MyBuilding::Land(TilePosition location, check_t checkMode)
{CI(this);
///	bw << NameWithId() << " land " << endl;
	bool result = Unit()->land(location);
	OnCommandSent(checkMode, result, NameWithId() + " land");
	return result;
}


void MyBuilding::CancelConstruction(check_t checkMode)
{CI(this);
///	bw << "CancelConstruction of " << NameWithId() << endl;
///	ai()->SetDelay(5000);
	bool result = Unit()->cancelConstruction();
	OnCommandSent(checkMode, result, NameWithId() + " cancelConstruction");
}


void MyBuilding::CancelTrain(check_t checkMode)
{CI(this);
///	bw << "CancelTraining of " << NameWithId() << endl;
///	ai()->SetDelay(5000);
	bool result = Unit()->cancelTrain();
	OnCommandSent(checkMode, result, NameWithId() + " cancelTrain");
}


void MyBuilding::CancelResearch(check_t checkMode)
{CI(this);
//	bw << "CancelResearch of " << NameWithId() << endl;
//	ai()->SetDelay(5000);
	if (Unit()->isResearching())
	{
		bool result = Unit()->cancelResearch();
		OnCommandSent(checkMode, result, NameWithId() + " cancelResearch");
	}
	else if (Unit()->isUpgrading())
	{
		bool result = Unit()->cancelUpgrade();
		OnCommandSent(checkMode, result, NameWithId() + " cancelUpgrade");
	}
	else assert_throw(false);
}


void MyBuilding::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * pOther)
{CI(this);
	if (Type().isAddon())
	{
		CHECK_POS(TopLeft() - 1);
		if (pOther == ai()->GetVMap().GetBuildingOn(ai()->GetMap().GetTile(TopLeft() - 1)))
			ConstructingThisAddonExpert()->OnMainBuildingDestroyed(pOther->IsMyBuilding());
	}
}


double MyBuilding::MinLifePercentageToRepair() const
{
	if (GetWall()) return 0.99;

	return 0.35;
}


double MyBuilding::MaxLifePercentageToRepair() const
{
	if (GetWall()) return 1.0;

	return 0.66;
}

/*
static double repairRate(UnitType target)
{
	return 0.9 * target.maxHitPoints() * 256 / target.buildTime(); 
}
*/

int MyBuilding::MaxRepairers() const
{

	if (GetWall())
	{
		vector<Repairing *> Repairers = this->Repairers();
		int repairers = Repairers.size();
		int activeRepairers =
			count_if(Repairers.begin(), Repairers.end(), [](const Repairing * r)
					{ return (r->State() == Repairing::repairing) && (ai()->Frame() - r->InStateSince() > 50); });

		if (!activeRepairers)
			return max(1, min(2, (int)him().AllUnits(Zerg_Zergling).size())) + int(-DeltaLifePerFrame() + 0.5);

		{
			if (DeltaLifePerFrame() < 0.3)
			{
				int lacking = 1 + int(-(DeltaLifePerFrame() - 0.3));
			//	bw << DeltaLifePerFrame() << " < 0.3 --> MaxRepairers() : " << repairers << " -> " << activeRepairers + lacking << endl;

				return activeRepairers + lacking;
			}
		}

		return repairers;
	}


	return MyBWAPIUnit::MaxRepairers();
}


bool MyBuilding::DefaultBehaviorOnFrame_common()
{CI(this);
	if (Completed())
		if (double(Life()) / MaxLife() < MinLifePercentageToRepair() - 0.000001)
			if (RepairersCount() < MaxRepairers())
			{
				int radius = (Life()*4 > MaxLife()*3) ? 16*32 :
							(Life()*4 > MaxLife()*2) ? 32*32 :
							(Life()*4 > MaxLife()*1) ? 64*32 :
							1000000;

				if (Is(Terran_Missile_Turret) && GetStronghold()) radius *= 2;

				if (GetWall())
				{
					if (me().LostUnitsOrBuildings(Terran_SCV) >= 4) return false;

					if (ai()->GetStrategy()->Active<Walling>())
					{
						radius = 64*32;

						if (none_of(him().Units().begin(), him().Units().end(), [this](const unique_ptr<HisUnit> & u)
							{ return !u->Type().isWorker() && (dist(u->Pos(), Pos()) < 12*32); }))
							return false;
					}
				}

				if (Repairing::GetRepairer(this, radius))
					return true;
			}

	return false;
}

	
} // namespace iron



