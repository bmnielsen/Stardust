//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "factory.h"
#include "shop.h"
#include "production.h"
#include "army.h"
#include "../behavior/raiding.h"
#include "../behavior/exploring.h"
#include "../behavior/executing.h"
#include "../behavior/guarding.h"
#include "../behavior/razing.h"
#include "../behavior/exploring.h"
#include "../behavior/walking.h"
#include "../behavior/scouting.h"
#include "../territory/stronghold.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../strategy/expand.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/zealotRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/cannonRush.h"
#include "../strategy/wraithRush.h"
#include "../strategy/terranFastExpand.h"
#include "../strategy/shallowTwo.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

static bool enemyBunkerNearBase()
{
	const vector<const Area *> MyEnlargedAreas = me().EnlargedAreas();
	vector<const Area *> MyTerritory = MyEnlargedAreas;
	for (const Area * area : MyEnlargedAreas)
		for (const Area * neighbour : area->AccessibleNeighbours())
			push_back_if_not_found(MyTerritory, neighbour);

	for (const unique_ptr<HisBuilding> & b : him().Buildings())
		if (b->Is(Terran_Bunker))
			if (contains(MyTerritory, b->GetArea(check_t::no_check)))
				return true;

	return false;
}




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Factory>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInTraining<Terran_Vulture> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Vulture, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


void ExpertInTraining<Terran_Vulture>::UpdateTrainingPriority()
{
//	{ m_priority = 0; return; }	//drop

//	if (me().StartingBase()->Center().y < ai()->GetMap().Center().y) { m_priority = 0; return; }

	if (ai()->GetStrategy()->Detected<CannonRush>())
		if (me().Units(Terran_Siege_Tank_Tank_Mode).size() < 2)
			{ m_priority = 0; return; }
/*
	if (him().IsProtoss())
		if (me().Buildings(Terran_Machine_Shop).empty())
			if (him().AllUnits(Protoss_Zealot).empty())
				{ m_priority = 0; return; }
*/

/*
	if (him().IsProtoss())
		//if (Units() >= 1) //new
			if (me().Units(Terran_Siege_Tank_Tank_Mode).size() < 1)
				//if (!((him().AllUnits(Protoss_Zealot).size() >= 4) && (him().AllUnits(Protoss_Dragoon).size() == 0)))	//new
					{ m_priority = 0; return; }
*/
	if (//ai()->GetStrategy()->Detected<ZerglingRush>() ||
		ai()->GetStrategy()->Detected<MarineRush>())
		{ m_priority = 620; return; }

//	if (ai()->GetStrategy()->TimeToBuildFirstShop())
//		{ m_priority = 0; return; }

	int minVulturesBeforeTanks = 5;

	if (him().IsProtoss())
	{
		minVulturesBeforeTanks = 0;
		if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1)
		if (him().AllUnits(Protoss_Zealot).size() >= 1)
			minVulturesBeforeTanks = 3;
	}

	if (him().ZerglingPressure())
	{
		minVulturesBeforeTanks = Units() + 5;
	}

	if (him().HydraPressure())
	{
		minVulturesBeforeTanks = 8;
	}


	if (me().Buildings(Terran_Command_Center).size() >= 2)
		if (him().AllUnits(Zerg_Hydralisk).size()*1.5 > me().Units(Terran_Vulture).size())
			minVulturesBeforeTanks = 10;

/*
	if (him().IsProtoss())
	{
		const int dragoons = him().AllUnits(Protoss_Dragoon).size();
		minVulturesBeforeTanks = min(10, max(minVulturesBeforeTanks, 2*dragoons));
	}
*/
	if (him().IsTerran())
	{
		minVulturesBeforeTanks = 7;

		if (me().HasResearched(TechTypes::Spider_Mines))
			if (count_if(me().Units(Terran_Vulture).begin(), me().Units(Terran_Vulture).end(), [](const unique_ptr<MyUnit> & u)
						{ return u->Completed() && u->IsMy<Terran_Vulture>()->RemainingMines() >= 2; }) < 4)
				minVulturesBeforeTanks = 10;
			
		if (ai()->GetStrategy()->Detected<WraithRush>())
			minVulturesBeforeTanks = 3;

		if (ai()->GetStrategy()->Detected<TerranFastExpand>())
			minVulturesBeforeTanks = 3;

		if (enemyBunkerNearBase())
			minVulturesBeforeTanks = 3;
	}

//	if (him().HasCannons()) minVulturesBeforeTanks = 2;

	if (Interactive::moreTanks || Interactive::moreGoliaths) minVulturesBeforeTanks = 0;

	if (auto s = ai()->GetStrategy()->Detected<ZerglingRush>())
	{
//		if (Units() == 0)
//			{ m_priority = 630; return; }

		if ((int)me().Units(Terran_Marine).size() < s->MaxMarines())
			{ m_priority = 0; return; }
	}

	if (Units() + Where()->IdlePosition() < minVulturesBeforeTanks)
	{
		m_priority = (Units() < 3) ? 605 : (Units() < 5) ? 585 : 470;
	}
	else
		m_priority = 400;

	int baseTurrets = count_if(me().Buildings(Terran_Missile_Turret).begin(), me().Buildings(Terran_Missile_Turret).end(), [](const unique_ptr<MyBuilding> & b){ return b->GetStronghold(); });
	if (baseTurrets * 2.5 < int(him().AllUnits(Zerg_Mutalisk).size() * me().Bases().size()))
		m_priority = 333;

///	if (him().HydraPressure_needVultures())
///		m_priority = 550;
}


template<>
class ExpertInTraining<Terran_Siege_Tank_Tank_Mode> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Siege_Tank_Tank_Mode, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


void ExpertInTraining<Terran_Siege_Tank_Tank_Mode>::UpdateTrainingPriority()
{
//	if (me().StartingBase()->Center().y < ai()->GetMap().Center().y) { m_priority = 0; return; }

//	if (Units() >= 1) { m_priority = 0; return; }//drop

	if (him().AllUnits(Protoss_Dragoon).size() >= 1)
		if (Units() < 1)
			{ m_priority = 10000; return; }

	if (ai()->GetStrategy()->Detected<CannonRush>())
		if (Units() < 2)
			{ m_priority = 10000; return; }
/*	
	for (auto & u : him().Units())
		if (u->Is(Protoss_Dragoon))
			if (u->GetArea() == me().GetArea())
				{ m_priority = 610; return; }
*/

	if (him().IsTerran())
	{
		static bool goTank = false;
		if (!goTank)
			if (me().Buildings(Terran_Command_Center).size() >= 2)
				if (!ai()->GetStrategy()->Detected<WraithRush>())
					if (me().Army().GroundLead() ||
						(me().Units(Terran_Vulture).size() >= 10) /*||
						ai()->GetStrategy()->Detected<TerranFastExpand>()*/)
						goTank = true;

		if (!goTank)
			if (!ai()->GetStrategy()->Detected<MarineRush>())
				if (enemyBunkerNearBase())
					goTank = true;

		if (Interactive::moreTanks)
			goTank = true;

		if (!goTank) { m_priority = 0; return; }
	}

	if (him().IsZerg())
	{
		if (!him().HydraPressure())
		{
			if (const Area * pMyNaturalArea = findNatural(me().StartingVBase())->BWEMPart()->GetArea())
				for (const auto & b : him().Buildings())
					if (b->GetArea(check_t::no_check) == pMyNaturalArea)
						{ m_priority = 410; return; }

			if (!Interactive::moreTanks)
			{
				if (me().Buildings(Terran_Armory).size() == 0) { m_priority = 0; return; }
		
				if (him().MayMuta())
					if (me().CompletedBuildings(Terran_Armory) == 0) { m_priority = 0; return; }
			}
		}
	}


/*
	int defensiveTanks = count_if(me().Units(Terran_Siege_Tank_Tank_Mode).begin(), me().Units(Terran_Siege_Tank_Tank_Mode).end(),
		[](const unique_ptr<MyUnit> & u){ return u->GetStronghold(); });

	if (him().MayReaver())
		if (defensiveTanks < 2)
			{ m_priority = 410; return; }
*/
	if (him().IsProtoss())
	{
		if (him().Buildings(Protoss_Photon_Cannon).size() >= 3)
			if (Units() == 0)
				{ m_priority = 590; return; }

	}

	const double r = (Units() + Where()->IdlePosition()) * 10 / (double)(1 +	me().Units(Terran_Vulture).size() +
																		me().Units(Terran_Siege_Tank_Tank_Mode).size() +
																		me().Units(Terran_Goliath).size());
	const double k = me().Army().TankRatioWanted() - r;
													

	if ((k > 0) && me().Army().FavorTanksOverGoliaths() &&
		(me().Production().GasAvailable() >= Cost(Terran_Siege_Tank_Tank_Mode).Gas()*3/4))
		m_priority = 410;
	else
		m_priority = 390;
}


template<>
class ExpertInTraining<Terran_Goliath> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Goliath, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


void ExpertInTraining<Terran_Goliath>::UpdateTrainingPriority()
{
	const double r = (Units() + Where()->IdlePosition()) * 10 / (double)(1 +	me().Units(Terran_Vulture).size() +
																		me().Units(Terran_Siege_Tank_Tank_Mode).size() +
																		me().Units(Terran_Goliath).size());
	const double k = me().Army().GoliathRatioWanted() - r;
													


	if ((k > 0) && !me().Army().FavorTanksOverGoliaths() &&
		(me().Production().GasAvailable() >= Cost(Terran_Goliath).Gas()*2/4))
		m_priority = 410;
	else
		m_priority = 390;
}


template<>
class ExpertInConstructing<Terran_Factory> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Factory) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Factory>::UpdateConstructingPriority()
{
	if (me().CompletedBuildings(Terran_Barracks) == 0) { m_priority = 0; return; }
	if (me().CompletedBuildings(Terran_Refinery) == 0) { m_priority = 0; return; }
	
	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (auto s = ai()->GetStrategy()->Detected<ZerglingRush>())
	{
		if (!s->TechRestartingCondition())	{ m_priority = 0; return; }
		
		if ((int)me().Units(Terran_Marine).size() < s->MaxMarines())
			if (!me().UnitsBeingTrained(Terran_Marine))
				{ m_priority = 0; return; }

		if (me().Units(Terran_SCV).size() < 20)
			if (!me().UnitsBeingTrained(Terran_SCV))
				if (me().SupplyAvailable() > 0)
					{ m_priority = 0; return; }
	}

	if (ai()->GetStrategy()->Detected<MarineRush>())				if (Buildings() >= 1)				{ m_priority = 0; return; }
	if (ai()->GetStrategy()->Detected<CannonRush>())				if (Buildings() >= 1)				{ m_priority = 0; return; }

	if (Interactive::moreVultures || Interactive::moreTanks || Interactive::moreGoliaths || Interactive::moreWraiths)
		if (Buildings() < 1)
			{ m_priority = 590; return; }


	const int mechProductionSites = me().Buildings(Terran_Factory).size() + me().Buildings(Terran_Starport).size();
	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });
//	const int lowerBound = min(12, 2*activeBases);
//	const int upperBound = min(12, (him().IsProtoss() ? 1 : 3)*activeBases);

	if ((Buildings() == 0) /*&& (!him().IsProtoss() || him().HasCannons())*/)
	{
		if ((me().SupplyUsed() >= 16) && (me().GasAvailable() > 75)/* &&
				(!ai()->GetStrategy()->Detected<ZerglingRush>() ||
				ai()->GetStrategy()->Detected<ZerglingRush>()->TechRestartingCondition()) ||
				(me().MineralsAvailable() > 150)*/)
			m_priority = 610;
		else
			m_priority = 590;

		return;
	}

	if (Buildings() == 1)
	{
		if (him().IsProtoss())
		{
			if (me().Buildings(Terran_Missile_Turret).size() >= 2)
				{ m_priority = 590; return; }
			
			if (me().Buildings(Terran_Command_Center).size() >= 2)
			{
				//if ((me().MineralsAvailable() >= 200) && (me().GasAvailable() >= 100))
				{
					if (me().Buildings(Terran_Factory).front()->Unit()->isTraining())
						if (((int)him().AllUnits(Protoss_Zealot).size()/2 > me().CompletedUnits(Terran_Vulture)) ||
							((int)him().AllUnits(Protoss_Dragoon).size() > me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)))
							{ m_priority = 590; return; }

					if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1)
						if (me().HasResearched(TechTypes::Tank_Siege_Mode) || me().Player()->isResearching(TechTypes::Tank_Siege_Mode))
							{ m_priority = 151; return; }
				}
			}


			m_priority = 0;
			return;
		}
		else
		{
			if ((me().SupplyUsed() >= 18) && (me().GasAvailable() > 75) &&
				(!ai()->GetStrategy()->Detected<ZerglingRush>() || (me().MineralsAvailable() > 150)))
				m_priority = 610;
			else
				m_priority = 590;

			return;
		}
	}

	if (him().IsZerg())
	{
		if (him().HydraPressure())
			if (Buildings() < 3)
			{
				m_priority = 450;
				if (me().Units(Terran_Vulture).size() >= 8)
					if ((me().MineralsAvailable() >= 150) && (me().GasAvailable() >= 100))
						m_priority = 700;
				return;
			}

	}

	if (me().Buildings(Terran_Command_Center).size() >= 2)
//		if (!him().IsProtoss() || (activeBases >= 2))
		if (!him().IsProtoss() || (me().CompletedBuildings(Terran_Factory) >= 2))
		{
			if (auto s = ai()->GetStrategy()->Detected<WraithRush>())
				if (Buildings() == 2)
					if (me().Buildings(Terran_Missile_Turret).size() >= 4)
						{ m_priority = 500; return; }

			if (mechProductionSites < min(12, 3*activeBases))
			{

	/*
				if (mechProductionSites < lowerBound-1)	{ m_priority = min(500, 390 + elapsed/10); return; }
				if (mechProductionSites < lowerBound)	{ m_priority = min(500, 290 + elapsed/20); return; }
				if (mechProductionSites < upperBound)	{ m_priority = 200 + 10*(upperBound - mechProductionSites); return; }
	*/
				m_priority = 50 * me().FactoryActivity();

				if (me().SupplyUsed() >= 195)
				{
					static map<int, frame_t> startingFrameByNumberOfFactories;
					if (startingFrameByNumberOfFactories[Buildings()] == 0)
						startingFrameByNumberOfFactories[Buildings()] = ai()->Frame();
					frame_t elapsed = ai()->Frame() - startingFrameByNumberOfFactories[Buildings()];

					m_priority = max(m_priority, min(500, 290 + elapsed/20));
				}

				return;
			}
		}


	m_priority = 0;
}


ExpertInConstructing<Terran_Factory>	My<Terran_Factory>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Factory>::GetConstructingExpert() { return &m_ConstructingExpert; }

vector<ConstructingAddonExpert *> My<Terran_Factory>::m_ConstructingAddonExperts {My<Terran_Machine_Shop>::GetConstructingAddonExpert()};


My<Terran_Factory>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Factory);

	AddTrainingExpert<Terran_Vulture>();
	AddTrainingExpert<Terran_Siege_Tank_Tank_Mode>();
	AddTrainingExpert<Terran_Goliath>();

	m_ConstructingExpert.OnBuildingCreated();
}




void My<Terran_Factory>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Vulture>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Vulture>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Vulture);
}


void My<Terran_Vulture>::PlaceMine(Position pos, check_t checkMode)
{CI(this);
	assert_throw(me().HasResearched(TechTypes::Spider_Mines));
	assert_throw(RemainingMines() >= 1);
///	bw << NameWithId() << " place mine at" << pos << "!" << endl;
	bool result = Unit()->useTech(TechTypes::Spider_Mines, pos);
	OnCommandSent(checkMode, result, NameWithId() + " place mine at " + my_to_string(pos));
}


bool My<Terran_Vulture>::WorthBeingRepaired() const
{CI(this);
	if (Life() < 50)
//		if (me().Units(Terran_Vulture).size() > 3)
			if (me().HasResearched(TechTypes::Spider_Mines))
				if (RemainingMines() <= 1)
					return false;

	return true;
}


void My<Terran_Vulture>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));

	if (him().StartingBase())
	{
		if (him().StartingBase()->GetArea()->AccessibleFrom(GetArea()))
		{
			if (!him().StartingBaseDestroyed())
			{
			///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
				return ChangeBehavior<Raiding>(this, GetRaidingTarget());
			}
			else
			{
				return ChangeBehavior<Exploring>(this, GetArea());
			}
		}
	}
	else
	{
	///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
		return ChangeBehavior<Scouting>(this);
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Siege_Tank_Tank_Mode>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Siege_Tank_Tank_Mode>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Siege_Tank_Tank_Mode);
}



void My<Terran_Siege_Tank_Tank_Mode>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Executing>(this);//drop

	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

	if (Is(Terran_Siege_Tank_Siege_Mode))
		return Unsiege();

/*
	if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
		if (pShallowTwo->KeepMarinesAtHome())
		{
			if (!GetStronghold())
				EnterStronghold(me().StartingVBase()->GetStronghold());
			return ChangeBehavior<Exploring>(this, homeAreaToHold());
		}
*/

	if (auto s = ai()->GetStrategy()->Active<Walling>())
		return ChangeBehavior<Exploring>(this, homeAreaToHold());

	if (me().Army().KeepTanksAtHome())
	{
		if (!GetStronghold())
			EnterStronghold(me().StartingVBase()->GetStronghold());

		int nBase = min(1, (int)me().Bases().size() - 1);
		int nTanks = (int)me().Units(Terran_Siege_Tank_Tank_Mode).size();
		int iTank = 0;
		for ( ; iTank < nTanks ; ++iTank)
			if (me().Units(Terran_Siege_Tank_Tank_Mode)[iTank].get() == this)
				break;

		if (activeBases < 2)
		{
			if (iTank < 2) nBase = 0;
		}
		if (me().Bases().size() < 3)
		{
			if (iTank < 1) nBase = 0;
		}

		return ChangeBehavior<Exploring>(this, me().Bases()[nBase]->GetArea()->BWEMPart());
	}

	if (him().StartingBase())
	{
		if (him().StartingBase()->GetArea()->AccessibleFrom(GetArea()))
		{
/*
			if (him().IsProtoss())
				if (him().MayReaver())// || !him().HasCannons())
					if (GetStronghold())
					{
						const Base * base = GetStronghold()->HasBase()->BWEMPart();
						int defensiveTanks = count_if(me().Units(Terran_Siege_Tank_Tank_Mode).begin(), me().Units(Terran_Siege_Tank_Tank_Mode).end(),
							[this](const unique_ptr<MyUnit> & u){ return u->GetStronghold() == GetStronghold(); });

						if (defensiveTanks <= 2)
							return ChangeBehavior<Guarding>(this, base);
					}
*/
			if (ai()->GetStrategy()->Detected<CannonRush>() && Razing::Condition(this))
				return ChangeBehavior<Razing>(this, GetArea());

			if (!him().StartingBaseDestroyed())
			{
			///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
				return ChangeBehavior<Raiding>(this, GetRaidingTarget());
			}
			else
			{
				return ChangeBehavior<Exploring>(this, GetArea());
			}
		}
	}
	else
	{
	///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
		return ChangeBehavior<Scouting>(this);
	}
}


int My<Terran_Siege_Tank_Tank_Mode>::CanSiegeAttack(const BWAPIUnit * u) const
{CI(this);
	// from UnitInterface::isInWeaponRange:

    int minRange = 2*32;
    int maxRange = 12*32;

    int distance = GetDistanceToTarget(u);

    if ((minRange ? minRange < distance : true) && distance <= maxRange)
		return max(distance, 1);

	return 0;
}


void My<Terran_Siege_Tank_Tank_Mode>::Siege(check_t checkMode)
{CI(this);
	assert_throw(Is(Terran_Siege_Tank_Tank_Mode));
///	bw << NameWithId() + " sieges" << endl;
	bool result = Unit()->siege();
	OnCommandSent(checkMode, result, NameWithId() + " siege");
}


void My<Terran_Siege_Tank_Tank_Mode>::Unsiege(check_t checkMode)
{CI(this);
	assert_throw(Is(Terran_Siege_Tank_Siege_Mode));
///	bw << NameWithId() + " unsieges" << endl;
	bool result = Unit()->unsiege();
	OnCommandSent(checkMode, result, NameWithId() + " unsiege");
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Vulture_Spider_Mine>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Vulture_Spider_Mine>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Vulture_Spider_Mine);

	TilePosition t(u->getPosition());
	t.x -= t.x % 3;
	t.y -= t.y % 3;
	CHECK_POS(t);
	ai()->GetVMap().PutMine(ai()->GetMap().GetTile(t));
}

My<Terran_Vulture_Spider_Mine>::~My()
{
	TilePosition t(Pos());
	t.x -= t.x % 3;
	t.y -= t.y % 3;
	CHECK_POS(t);
	ai()->GetVMap().RemoveMine(ai()->GetMap().GetTile(t));
}


void My<Terran_Vulture_Spider_Mine>::DefaultBehaviorOnFrame()
{CI(this);
	m_pTarget = nullptr;
	m_dangerous = false;

	if (Unit()->isBurrowed())
		m_ready = true;
	
	if (m_ready)
	{
		int minDist = GroundRange() + 2*32;
		for (const auto & faceOff : FaceOffs())
			if (faceOff.His()->IsHisUnit())
				if (faceOff.MyAttack())
					if (faceOff.GroundDistanceToHitHim() < minDist)
					{
						minDist = faceOff.GroundDistanceToHitHim();
						m_pTarget = &faceOff;
					}

		if (m_pTarget)
			if ((minDist < GroundRange()) ||
				(minDist < GroundRange() + 8) && !Unit()->isBurrowed())
			{
				m_dangerous = true;
			///	ai()->SetDelay(50);
			///	bw << NameWithId() << " dangerous !!!" << endl;

				for (const auto & u : me().Units())
					if (u->Completed() && !u->Loaded())
					if (u->CanAcceptCommand())
						if (!u->Flying())
							if (!u->Is(Terran_Siege_Tank_Siege_Mode))
							if (!u->GetBehavior()->IsConstructing())
								if (groundDist(u->Pos(), Pos()) < 12*32)
								{
									Vect V = toVect(u->Pos() - m_pTarget->His()->Pos());
									V.Normalize();
									Position delta = toPosition(V * 2*32);
									Position dest = ai()->GetMap().Crop(u->Pos() + delta);
#if DEV
									drawLineMap(u->Pos(), dest, Colors::Yellow, crop);//1
#endif
									u->Move(dest);
								}
			}
	}


#if DEV
	if (m_pTarget)
	{
	//	ai()->SetDelay(500);
		drawLineMap(Pos(), m_pTarget->His()->Pos(), Colors::Orange);
		m_pTarget->His()->IsHisUnit()->AddMineTargetingThis(Pos());
	}
#endif

}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Goliath>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Goliath>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Goliath);
}


void My<Terran_Goliath>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Executing>(this);

	if (me().Army().KeepGoliathsAtHome())
	{
		if (!GetStronghold())
			EnterStronghold(me().StartingVBase()->GetStronghold());
//		return ChangeBehavior<Exploring>(this, me().Bases().back()->GetArea()->BWEMPart());
		return ChangeBehavior<Exploring>(this, me().Bases()[min(1, (int)me().Bases().size() - 1)]->GetArea()->BWEMPart());
	}

	if (him().StartingBase())
	{
		if (him().StartingBase()->GetArea()->AccessibleFrom(GetArea()))
		{
			if (!him().StartingBaseDestroyed())
			{
			///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
				return ChangeBehavior<Raiding>(this, GetRaidingTarget());
			}
			else
			{
				return ChangeBehavior<Exploring>(this, GetArea());
			}
		}
	}
	else
	{
	///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
		return ChangeBehavior<Scouting>(this);
	}
}




	
} // namespace iron



