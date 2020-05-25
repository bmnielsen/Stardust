//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "wraithAttack.h"
#include "../units/factory.h"
#include "../behavior/kiting.h"
#include "../behavior/raiding.h"
#include "strategy.h"
#include "baseDefense.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{




void wraithAttack()
{
	if (ai()->Frame() % 50 != 0) return;

	vector<MyUnit *> MyWraiths;
	for (const auto & u : me().Units(Terran_Wraith))
		if (u->Completed())
			if (u->GetBehavior()->IsRaiding() ||
				u->GetBehavior()->IsKiting() &&
					( ai()->Frame() - u->GetBehavior()->IsKiting()->LastAttack() > 50 ||
					  ai()->GetStrategy()->Active<BaseDefense>()
					))
				MyWraiths.push_back(u.get());

	if (MyWraiths.empty()) return;

	multimap<int, HisUnit *> Targets;
	for (const auto & u : him().Units())
		if (!u->InFog())
//			if (u->Chasers().empty())
//			if (u->VChasers().empty())
//			if (u->Destroyers().empty())
			if (!u->AirAttack())
				if (!u->Is(Protoss_Zealot))
				if (!u->Flying() && u->GroundAttack() ||
					u->Flying() ||
					u->Is(Protoss_High_Templar) ||
					u->Is(Protoss_Dark_Archon))
					if (!(u->Is(Zerg_Zergling) && him().HydraPressure_needVultures()))
					{
						int minDistToBase = numeric_limits<int>::max();
						for (VBase * base : me().Bases())
						{
							int d = groundDist(u->Pos(), base->BWEMPart()->Center());
							if (d < minDistToBase)
							{
								minDistToBase = d;
							}
						}

						int score = minDistToBase;
						switch(u->Type())
						{
						case Protoss_Reaver:		score -= 50000; break;
						case Protoss_Scarab:		score -= 50000; break;
						case Protoss_High_Templar:	score -= 60000; break;
						case Protoss_Shuttle:
						case Terran_Dropship:
						case Zerg_Overlord:			score -= 100000; break;
						}

						int hisOtherUnitsAround = 0;
						for (const auto & v : him().Units())
							if (!v->InFog())
								if (v->Is(Protoss_Dragoon) || v->Is(Zerg_Hydralisk) || v->Is(Terran_Marine) || v->Is(Terran_Goliath))
									if (roundedDist(v->Pos(), u->Pos()) < 5*32)
										++hisOtherUnitsAround;
								
						if (hisOtherUnitsAround >= 2) continue;

						Targets.emplace(score, u.get());
					}

	if (Targets.empty()) return;

	for (MyUnit * u : MyWraiths)
	{
		u->ChangeBehavior<Raiding>(u, Targets.begin()->second->Pos());
	}

///	ai()->SetDelay(499);
}



void valkyrieAttack()
{
	if (ai()->Frame() % 50 != 0) return;

	vector<MyUnit *> MyValkyries;
	for (const auto & u : me().Units(Terran_Valkyrie))
		if (u->Completed())
			if (u->GetBehavior()->IsRaiding() ||
				u->GetBehavior()->IsKiting() &&
					( ai()->Frame() - u->GetBehavior()->IsKiting()->LastAttack() > 50 ||
					  ai()->GetStrategy()->Active<BaseDefense>()
					))
				MyValkyries.push_back(u.get());

	if (MyValkyries.empty()) return;

	multimap<int, HisUnit *> Targets;
	for (const auto & u : him().Units())
		if (!u->InFog())
//			if (u->Chasers().empty())
//			if (u->VChasers().empty())
//			if (u->Destroyers().empty())
			if (u->Flying())
				{
					int minDistToBase = numeric_limits<int>::max();
					for (VBase * base : me().Bases())
					{
						int d = groundDist(u->Pos(), base->BWEMPart()->Center());
						if (d < minDistToBase)
						{
							minDistToBase = d;
						}
					}

					int score = minDistToBase;
					switch(u->Type())
					{
					case Protoss_Carrier:		score -= 50000; break;
					}

					Targets.emplace(score, u.get());
				}

	if (Targets.empty()) return;

	for (MyUnit * u : MyValkyries)
	{
		u->ChangeBehavior<Raiding>(u, Targets.begin()->second->Pos());
	}

///	ai()->SetDelay(499);
}



void vesselAttack()
{
	if (ai()->Frame() % 50 != 0) return;

	vector<MyUnit *> MyVessels;
	for (const auto & u : me().Units(Terran_Science_Vessel))
		if (u->Completed())
			if (u->GetBehavior()->IsRaiding() ||
				u->GetBehavior()->IsKiting())
				MyVessels.push_back(u.get());

	if (MyVessels.empty()) return;

	multimap<int, HisUnit *> Targets;
	for (const auto & u : him().Units())
		if (!u->InFog())
//			if (u->Chasers().empty())
//			if (u->VChasers().empty())
//			if (u->Destroyers().empty())
			if (!u->Flying())
			if (u->Unit()->isCloaked() || u->Unit()->isBurrowed() || !u->Unit()->isVisible())
				{
					int minDistToBase = numeric_limits<int>::max();
					for (VBase * base : me().Bases())
					{
						int d = groundDist(u->Pos(), base->BWEMPart()->Center());
						if (d < minDistToBase)
						{
							minDistToBase = d;
						}
					}

					int score = minDistToBase;
					switch(u->Type())
					{
					}

					Targets.emplace(score, u.get());
				}

	if (Targets.empty()) return;

	for (MyUnit * u : MyVessels)
	{
		u->ChangeBehavior<Raiding>(u, Targets.begin()->second->Pos());
	}

///	ai()->SetDelay(499);
}




} // namespace iron



