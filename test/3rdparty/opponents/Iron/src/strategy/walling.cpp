//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "walling.h"
#include "../units/cc.h"
#include "../behavior/constructing.h"
#include "../behavior/mining.h"
#include "../behavior/fleeing.h"
#include "../behavior/walking.h"
#include "../behavior/chasing.h"
#include "../behavior/blocking.h"
#include "../behavior/sniping.h"
#include "../behavior/raiding.h"
#include "../behavior/supplementing.h"
#include "../behavior/defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Walling
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Walling::Walling()
{
}


Walling::~Walling()
{
	ai()->GetStrategy()->SetMinScoutingSCVs(1);
}


string Walling::StateDescription() const
{
	if (m_active) return "active";

	return "-";
}


Wall * Walling::GetWall() const
{
	return me().GetVBase(0)->GetWall();
}


void Walling::OnFrame_v()
{
	if (him().IsTerran())
		return Discard();

	if (bw->mapHash() == "8000dc6116e405ab878c14bb0f0cde8efa4d640c")		// Alchemist
		return Discard();

	if (bw->mapHash() == "450a792de0e544b51af5de578061cb8a2f020f32")		// Circuit Breaker
		return Discard();

	if (!(him().IsProtoss() || him().IsTerran()))
	{
		if (bw->mapHash() == "19f00ba3a407e3f13fb60bdd2845d8ca2765cf10")	// Neo Aztec
			return Discard();

		if (bw->mapHash() == "33527b4ce7662f83485575c4b1fcad5d737dfcf1")	// LunaTheFinal 2.3
			return Discard();
	}

	if (!GetWall()) return Discard();

	if (m_active)
	{
		if (him().IsZerg())
		{
			if (!GetWall()->Completed())
				for (const auto & u : him().Units())
					if (u->Type().isWorker())
						if (u->Chasers().empty())
							if (GetWall()->DistanceTo(u->Pos()) < 10*32)
								if (My<Terran_SCV> * pWorker = findFreeWorker_urgent(me().StartingVBase()))
									pWorker->ChangeBehavior<Chasing>(pWorker, u.get(), bool("insist"), 1000, bool("workerDefense"));

			int zerglingStrength = 1;
			if (!me().HasUpgraded(UpgradeTypes::Enum::Ion_Thrusters))
				if (him().SpeedLings()) zerglingStrength = 3;
				else					zerglingStrength = 2;

			if (me().CompletedUnits(Terran_Vulture) >= 6)
				if (me().HasUpgraded(UpgradeTypes::Enum::Ion_Thrusters))
				{
				///	bw << "stop Walling (y)" << endl;
				///	ai()->SetDelay(500);
					m_active = false;
				}

			if (me().CompletedUnits(Terran_Vulture) >= 1)
				if (me().CompletedUnits(Terran_Vulture) * 3 +
					me().CompletedUnits(Terran_Marine) * 2
					>= (int)him().AllUnits(Zerg_Zergling).size() * zerglingStrength)
					
					{
					///	bw << "stop Walling (z)" << endl;
					///	ai()->SetDelay(500);
						m_active = false;
					}
		}
		else
		{//return;
			const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

			if (!him().AllUnits(Protoss_Dark_Templar).empty())
			{
				if (me().Buildings(Terran_Missile_Turret).size() < 3) return;

				if (!me().HasResearched(TechTypes::Enum::Spider_Mines)) return;

				if (me().Units(Terran_Vulture).size() < 6) return;

				if (me().Units(Terran_Vulture).size() < him().AllUnits(Protoss_Dark_Templar).size()) return;
				
				if (me().Units(Terran_Vulture).size() < 10)
					if (me().Units(Terran_Vulture).size() < 1.5*him().AllUnits(Protoss_Dark_Templar).size()) return;
			}


			if (me().CompletedBuildings(Terran_Command_Center) >= 2)
				if ((him().Buildings(Protoss_Nexus).size() >= 2) ||
					((him().Buildings(Protoss_Nexus).size() == 1) &&
						!contains(ai()->GetMap().StartingLocations(), him().Buildings(Protoss_Nexus).front()->TopLeft())))
					if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 >= (int)him().AllUnits(Protoss_Dragoon).size()*3 ||
						me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 2)
						if (me().CompletedUnits(Terran_Vulture) >= (int)him().AllUnits(Protoss_Zealot).size())
						{
						///	bw << "stop Walling (fast nexus)" << endl;
						///	ai()->SetDelay(500);
							m_active = false;
						}

			if (activeBases >= 2)
			{
			///	bw << "stop Walling (de facto)" << endl;
			///	ai()->SetDelay(500);
				m_active = false;
			}
			else if (me().CompletedUnits(Terran_Vulture) >= 10)
			{
			///	bw << "stop Walling (enough vultures)" << endl;
			///	ai()->SetDelay(500);
				m_active = false;
			}
			else if (me().HasResearched(TechTypes::Enum::Tank_Siege_Mode))
				if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 1)
				if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 >= (int)(him().AllUnits(Protoss_Dragoon).size())*3 ||
					me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 4)
				if (me().CompletedUnits(Terran_Vulture) >= (int)him().AllUnits(Protoss_Zealot).size()*2 ||
						((me().CompletedUnits(Terran_Vulture) >= (int)him().AllUnits(Protoss_Zealot).size())
						&& (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 4)))
				{
					///	bw << "stop Walling" << endl;
					///	ai()->SetDelay(500);
						m_active = false;
				}
		}
	}
	else
	{
		if (me().SupplyUsed() < 8)
		{
			if (!him().IsTerran())
				m_active = true;
		}
		else return Discard();

	}
}


} // namespace iron



