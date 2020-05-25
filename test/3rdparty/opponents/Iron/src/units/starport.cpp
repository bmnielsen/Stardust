//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "starport.h"
#include "tower.h"
#include "production.h"
#include "factory.h"
#include "cc.h"
#include "army.h"
#include "../behavior/raiding.h"
#include "../behavior/exploring.h"
#include "../behavior/walking.h"
#include "../behavior/collecting.h"
#include "../strategy/strategy.h"
#include "../strategy/wraithRush.h"
#include "../strategy/terranFastExpand.h"
#include "../strategy/shallowTwo.h"
#include "../strategy/walling.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Starport>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInTraining<Terran_Wraith> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Wraith, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


void ExpertInTraining<Terran_Wraith>::UpdateTrainingPriority()
{
//	if (me().StartingBase()->Center().y < ai()->GetMap().Center().y) { m_priority = 0; return; }

	if (Interactive::moreWraiths)
		if (me().Production().GasAvailable() >= Cost(Terran_Wraith).Gas()*2/4)
			{ m_priority = 475; return; }

	if (ai()->GetStrategy()->Detected<WraithRush>())
		{ m_priority = 0; return; }

	if (ai()->GetStrategy()->Detected<TerranFastExpand>())
		if (me().Buildings(Terran_Command_Center).size() == 1)
			{ m_priority = 0; return; }

	int limit = me().Army().AirAttackOpportunity() + 2;
	limit = max(limit, 3 + (int)him().AllUnits(Terran_Wraith).size());
	if (him().IsProtoss()) limit = min(limit, me().SupplyUsed() / 20 - 1);

	if (him().IsProtoss())
		if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 < (int)him().AllUnits(Protoss_Dragoon).size()*3)
			if (!him().MayReaver())
				if ((Units() == 0) || (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 6))
				{
					m_priority = 0;
					if (me().GasAvailable() > 250)
						m_priority = 350;
					return;
				}

	if (me().Production().GasAvailable() >= Cost(Terran_Wraith).Gas()*2/4)
	{
		if (Units() + Where()->IdlePosition() < 3)			m_priority = 475;
		else if (Units() + Where()->IdlePosition() < limit)	m_priority = 420;
		if (him().ZerglingPressure()) m_priority += 150;
		return;
	}

	m_priority = 0;
}


template<>
class ExpertInTraining<Terran_Valkyrie> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Valkyrie, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


void ExpertInTraining<Terran_Valkyrie>::UpdateTrainingPriority()
{
//	if (me().StartingBase()->Center().y < ai()->GetMap().Center().y) { m_priority = 0; return; }

	if (him().AllUnits(Protoss_Carrier).size() > 0)
		if (me().Production().GasAvailable() >= Cost(Terran_Valkyrie).Gas()*3/4)
		{
			if (Units() < 1)
				m_priority = 485;
			else
				m_priority = 400;
			return;
		}

	m_priority = 0;
}


template<>
class ExpertInTraining<Terran_Science_Vessel> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Science_Vessel, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


void ExpertInTraining<Terran_Science_Vessel>::UpdateTrainingPriority()
{
	{
		int limit = 1;

		limit = max(limit, int(2*him().AllUnits(Protoss_Arbiter).size() + him().AllUnits(Protoss_Dark_Templar).size()));

		if (Units() < 1)
			m_priority = 490;
		else if (Units() < limit)
			m_priority = 400;
		return;
	}

	m_priority = 0;
}


template<>
class ExpertInTraining<Terran_Dropship> : public TrainingExpert
{
public:
						ExpertInTraining(MyBuilding * pWhere) : TrainingExpert(Terran_Dropship, pWhere) {}

	void				UpdateTrainingPriority() override;

private:
};


void ExpertInTraining<Terran_Dropship>::UpdateTrainingPriority()
{
//	if (me().StartingBase()->Center().y < ai()->GetMap().Center().y) { m_priority = 0; return; }

	m_priority = 0;
	return;
//drop

	if (him().IsProtoss())
	{
		if (Units() < 1)
			if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 2)
				if (me().Units(Terran_Wraith).size() >= 1)
					m_priority = 480;
	}

}


template<>
class ExpertInConstructing<Terran_Starport> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Starport) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Starport>::UpdateConstructingPriority()
{
	if (me().CompletedBuildings(Terran_Factory) == 0) { m_priority = 0; return; }

	if (Interactive::moreTanks || Interactive::moreGoliaths || Interactive::moreWraiths)
		if (Buildings() < 1)
			{ m_priority = 590; return; }

	if (ai()->GetStrategy()->Detected<WraithRush>())
		{ m_priority = 0; return; }

	if (ai()->GetStrategy()->Detected<TerranFastExpand>())
		if (me().Buildings(Terran_Command_Center).size() == 1)
			{ m_priority = 0; return; }

	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (him().IsTerran())
	{
		if (Buildings() == 0)
			if (me().CompletedBuildings(Terran_Factory) >= 2)
				if (me().CompletedUnits(Terran_Vulture) >= int(him().AllUnits(Terran_Marine).size() + 2*him().AllUnits(Terran_Medic).size()) / 2)
				{
				///	DO_ONCE ai()->SetDelay(500);
					m_priority = 500; return;
				}
	}

	if (him().IsProtoss())
	{//{ m_priority = 0; return; }
		if (Buildings() == 0)//drop
		{
			if (me().Buildings(Terran_Factory).size() >= 3)
			{
			///	DO_ONCE ai()->SetDelay(500);
				m_priority = 500; return;
			}

			if (him().MayReaver())
			{
			///	DO_ONCE ai()->SetDelay(500);
				m_priority = 595; return;
			}

			if (me().Army().KeepTanksAtHome() || (me().Army().MyGroundUnitsAhead() > me().Army().HisGroundUnitsAhead() + 4))
				if (me().Buildings(Terran_Command_Center).size() < 3)
					if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) < 8)
							{ m_priority = 0; return; }

			if (me().CompletedBuildings(Terran_Command_Center) >= 2)
				if (me().Buildings(Terran_Factory).size() >= 2)
					if (!ai()->GetStrategy()->Active<Walling>())
					{
					///	DO_ONCE ai()->SetDelay(500);
						m_priority = 500; return;
					}


			if (me().Buildings(Terran_Factory).size() >= 2)
				{ m_priority = 40; return; }
		}
	}
	
	if (him().IsProtoss())
	{
		if (Buildings() == 0)
		{
			/*
			if (me().Buildings(Terran_Command_Center).size() >= 2)
				if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1)
				{
					m_priority = 500;
					return;
				}*/

/*
			if (me().CompletedBuildings(Terran_Command_Center) >= 1)
			{
				if (ai()->GetStrategy()->Has<ShallowTwo>())
					if (ai()->GetStrategy()->Has<ShallowTwo>()->DelayExpansion() ||
						!ai()->GetStrategy()->Has<ShallowTwo>()->KeepMarinesAtHome())
					{
						if (!ai()->GetStrategy()->Has<ShallowTwo>()->KeepMarinesAtHome())
						{
							m_priority = 650;
							return;
						}

						if (him().MayReaver())
						{
							if ((him().AllUnits(Protoss_Reaver).size() >= 1) || (him().AllUnits(Protoss_Shuttle).size() >= 1))
								m_priority = 700;
							else
								m_priority = 500;
							return;
						}
					}
			}

			if (me().CompletedBuildings(Terran_Command_Center) >= 3)
				if (me().SupplyUsed() >= 150)
				{
				///	DO_ONCE ai()->SetDelay(500);
					m_priority = 500;
					return;
				}

			if (me().CompletedBuildings(Terran_Command_Center) >= 2)
//				if ((me().SupplyUsed() >= 80))
				{
				///	DO_ONCE ai()->SetDelay(500);
					if (him().AllUnits(Protoss_Carrier).size() > 0)
						m_priority = 500;
					else
						m_priority = min(500, me().Army().AirAttackOpportunity() * 100);
					return;
				}
*/
		}
	}
	
	if (him().IsZerg())
	{
		if (Buildings() == 0)
		{
			if (me().Army().HisInvisibleUnits() || (him().AllUnits(Zerg_Queen).size() > 0))
				if (me().CompletedBuildings(Terran_Command_Center) >= 2)
				{
					if (me().SupplyUsed() >= 70 - 2*me().Army().HisInvisibleUnits())
					{
					///	DO_ONCE ai()->SetDelay(500);
						m_priority = min(500, me().Army().AirAttackOpportunity() * 100);
						return;
					}
				}

			if (me().CompletedBuildings(Terran_Command_Center) >= 3)
				//if (me().SupplyUsed() >= 120)
				{
				///	DO_ONCE ai()->SetDelay(500);
					m_priority = 500;
					return;
				}

			if (him().ZerglingPressure())
				if (me().Buildings(Terran_Barracks).size() >= 2)
				if (me().Units(Terran_Marine).size() >= 12)
				if (me().Units(Terran_Vulture).size() >= 3)
				{
					m_priority = 700;
					return;
				}
/*
			if (him().HydraPressure_needVultures())
				if (me().CompletedBuildings(Terran_Factory) >= 4)
					if (him().AllUnits(Zerg_Hydralisk).size() <=
						me().Units(Terran_Vulture).size() + me().Units(Terran_Siege_Tank_Tank_Mode).size()*2)
					{
						m_priority = 500;
						return;
					}
*/
		}
	}

	m_priority = 0;
}


ExpertInConstructing<Terran_Starport>	My<Terran_Starport>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Starport>::GetConstructingExpert() { return &m_ConstructingExpert; }

vector<ConstructingAddonExpert *> My<Terran_Starport>::m_ConstructingAddonExperts {My<Terran_Control_Tower>::GetConstructingAddonExpert()};


My<Terran_Starport>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Starport);

	AddTrainingExpert<Terran_Wraith>();
	AddTrainingExpert<Terran_Valkyrie>();
	AddTrainingExpert<Terran_Science_Vessel>();
	AddTrainingExpert<Terran_Dropship>();//drop

	m_ConstructingExpert.OnBuildingCreated();
}




void My<Terran_Starport>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Wraith>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Wraith>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Wraith);
}


void My<Terran_Wraith>::Cloak(check_t checkMode)
{CI(this);
	assert_throw(me().HasResearched(TechTypes::Cloaking_Field));
	assert_throw(Unit()->canUseTechWithoutTarget(TechTypes::Cloaking_Field));
//	bw << NameWithId() << " cloak! " << endl;
	bool result = Unit()->useTech(TechTypes::Cloaking_Field);
	OnSpellSent(checkMode, result, NameWithId() + " cloak ");
}


void My<Terran_Wraith>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));

	///	DO_ONCE ai()->SetDelay(500); bw << "Wraith here" << endl;

	if (him().StartingBase())
	{
		if (!him().StartingBaseDestroyed())
		{
		///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
			return ChangeBehavior<Raiding>(this, GetRaidingTarget());
		}
		else
		{
			return ChangeBehavior<Exploring>(this, FindArea());
		}
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Valkyrie>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Valkyrie>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Valkyrie);
}


void My<Terran_Valkyrie>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));

	///	DO_ONCE ai()->SetDelay(500); bw << "Wraith here" << endl;

	if (him().StartingBase())
	{
		if (!him().StartingBaseDestroyed())
		{
		///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
			return ChangeBehavior<Raiding>(this, GetRaidingTarget());
		}
		else
		{
			return ChangeBehavior<Exploring>(this, FindArea());
		}
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Science_Vessel>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Science_Vessel>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Science_Vessel);
}


void My<Terran_Science_Vessel>::DefensiveMatrix(MyBWAPIUnit * target, check_t checkMode)
{CI(this);
//	bw << NameWithId() << " throws Defensive_Matrix on " << target->NameWithId() << endl;

	assert_throw(Energy() >= Cost(TechTypes::Defensive_Matrix).Energy());
	bool result = Unit()->useTech(TechTypes::Defensive_Matrix, target->Unit());
	OnSpellSent(checkMode, result, NameWithId() + " throws Defensive_Matrix on " + target->NameWithId());
}


void My<Terran_Science_Vessel>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));

	///	DO_ONCE ai()->SetDelay(500); bw << "Wraith here" << endl;

	if (him().StartingBase())
	{
		if (!him().StartingBaseDestroyed())
		{
		///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);
			return ChangeBehavior<Raiding>(this, GetRaidingTarget());
		}
		else
		{
			return ChangeBehavior<Exploring>(this, FindArea());
		}
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Dropship>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

My<Terran_Dropship>::damage_t dropshipDamage(const My<Terran_Dropship> * pDropship, const vector<MyUnit *> & ControlledUnits)
{
	if (pDropship->MaxLife() - pDropship->Life() > 50) return My<Terran_Dropship>::damage_t::require_repair;

	for (MyUnit * u : ControlledUnits)
		switch(u->Type())
		{
		case Terran_Siege_Tank_Tank_Mode:
		case Terran_Siege_Tank_Siege_Mode:
			if (u->MaxLife() - u->Life() > 50) return My<Terran_Dropship>::damage_t::require_repair;
			break;
		case Terran_Vulture:
			if (u->MaxLife() - u->Life() > 40) return My<Terran_Dropship>::damage_t::require_repair;
			break;
		case Terran_SCV:
			if (u->MaxLife() - u->Life() > 30) return My<Terran_Dropship>::damage_t::require_repair;
			break;
		default:
			if (u->MaxLife() - u->Life() > 30) return My<Terran_Dropship>::damage_t::require_repair;
			break;
	}
		
	if (pDropship->MaxLife() - pDropship->Life() > 0) return My<Terran_Dropship>::damage_t::allow_attack;

	for (MyUnit * u : ControlledUnits)
		if (u->MaxLife() - u->Life() > 0) return My<Terran_Dropship>::damage_t::allow_attack;
		
	return My<Terran_Dropship>::damage_t::none;
}



My<Terran_Dropship>::My(BWAPI::Unit u)
	: MyUnit(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Dropship);
}


void My<Terran_Dropship>::Update()
{CI(this);
	MyUnit::Update();

	m_LoadedUnits.clear();
	for (MyUnit * u : me().Units())
		if (contains(Unit()->getLoadedUnits(), u->Unit()))
			m_LoadedUnits.push_back(u);
}


void My<Terran_Dropship>::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);

	if (MyUnit * u = other->IsMyUnit())
		really_remove(m_LoadedUnits, u);
}


void My<Terran_Dropship>::Load(BWAPIUnit * u, check_t checkMode)
{CI(this);
	assert_throw(!u->Loaded());
	assert_throw(!contains(m_LoadedUnits, u->IsMyUnit()));

///	bw << NameWithId() << " load! " << u->NameWithId() << endl;
	bool result = u->Unit()->load(Unit());
//	if (result) m_LoadedUnits.push_back(u->IsMyUnit());
	OnCommandSent(checkMode, result, NameWithId() + " load " + u->NameWithId());
}


void My<Terran_Dropship>::Unload(BWAPIUnit * u, check_t checkMode)
{CI(this);
	assert_throw(u->Loaded());
	assert_throw(contains(m_LoadedUnits, u->IsMyUnit()));

	m_lastUnload = ai()->Frame();
///	bw << NameWithId() << " unload! " << u->NameWithId() << endl;
	bool result = Unit()->unload(u->Unit());
//	if (result) really_remove(m_LoadedUnits, u->IsMyUnit());
	OnCommandSent(checkMode, result, NameWithId() + " unload " + u->NameWithId());
}


void My<Terran_Dropship>::UnloadAll(check_t checkMode)
{CI(this);
	assert_throw(!m_LoadedUnits.empty());

	m_lastUnload = ai()->Frame();
///	bw << NameWithId() << " unloadAll!" << endl;
	bool result = Unit()->unloadAll();
//	if (result) really_remove(m_LoadedUnits, u->IsMyUnit());
	OnCommandSent(checkMode, result, NameWithId() + " unloadAll");
}


bool My<Terran_Dropship>::CanUnload() const
{
	if (Unit()->canUnloadAll())
	{
		const MiniTile & miniTile = ai()->GetMap().GetMiniTile(WalkPosition(Pos()));
		if (miniTile.Walkable() && miniTile.Altitude() >= 48)
			return true;
	}

	return false;
}


My<Terran_Dropship>::damage_t My<Terran_Dropship>::GetDamage() const
{
	return dropshipDamage(this, m_LoadedUnits);
}


int My<Terran_Dropship>::GetCargoCount(UnitType type) const
{
	int count = 0;
	for (MyUnit * u : LoadedUnits())
		if (u->Is(type))
			++count;

	return count;
}


MyUnit * My<Terran_Dropship>::GetCargo(UnitType type) const
{
	for (MyUnit * u : LoadedUnits())
		if (u->Is(type))
			return u;

	return nullptr;
}


My<Terran_Siege_Tank_Tank_Mode> * My<Terran_Dropship>::GetTank() const
{
	for (MyUnit * u : LoadedUnits())
		if (My<Terran_Siege_Tank_Tank_Mode> * tank = u->IsMy<Terran_Siege_Tank_Tank_Mode>())
			return tank;

	return nullptr;
}


My<Terran_Vulture> * My<Terran_Dropship>::GetVulture() const
{
	for (MyUnit * u : LoadedUnits())
		if (My<Terran_Vulture> * vulture = u->IsMy<Terran_Vulture>())
			return vulture;

	return nullptr;
}


vector<My<Terran_SCV> *> My<Terran_Dropship>::GetSCVs() const
{
	vector<My<Terran_SCV> *> List;
	for (MyUnit * u : LoadedUnits())
		if (My<Terran_SCV> * scv = u->IsMy<Terran_SCV>())
			List.push_back(scv);

	return List;
}


bool My<Terran_Dropship>::WorthBeingRepaired() const
{CI(this);
	return false;
}


void My<Terran_Dropship>::DefaultBehaviorOnFrame()
{CI(this);
//	return ChangeBehavior<Walking>(this, Pos(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Walking>(this, ai()->GetMap().Center(), __FILE__ + to_string(__LINE__));
//	return ChangeBehavior<Dropping1T>(this);//drop

	if (Collecting::EnterCondition(this))
		return ChangeBehavior<Collecting>(this);		//drop


	if (him().StartingBase())
	{
		if (!him().StartingBaseDestroyed())
		{
		///	bw << NameWithId() << " go to his base!" << endl; ai()->SetDelay(2000);

			return ChangeBehavior<Raiding>(this, GetRaidingTarget());
		}
		else
		{
			return ChangeBehavior<Exploring>(this, FindArea());
		}
	}
}





	
} // namespace iron



