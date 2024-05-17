//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "baseDefense.h"
#include "strategy.h"
#include "dragoonRush.h"
#include "../units/me.h"
#include "../units/him.h"
#include "../units/factory.h"
#include "../units/cc.h"
#include "../territory/stronghold.h"
#include "../behavior/raiding.h"
#include "../behavior/repairing.h"
#include "../behavior/chasing.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


static VBase * inMyBase(Position pos)
{
	if (const Area * area = ai()->GetMap().GetArea(WalkPosition(pos)))
		for (VBase * base : me().Bases())
			if (contains(base->GetArea()->EnlargedArea(), area))
				return base;

	return nullptr;
}


static VBase * nearMyBase(Position pos, int maxTileDistanceToArea)
{
	multimap<int, VBase *> MyBasesAround;
	int tileRadius = maxTileDistanceToArea;
	for (TilePosition delta : {	TilePosition(+tileRadius,  0), TilePosition(0, -tileRadius),  TilePosition(0, +tileRadius),  TilePosition(-tileRadius,  0),
								TilePosition(-tileRadius, -tileRadius), TilePosition(-tileRadius, +tileRadius), TilePosition(+tileRadius, -tileRadius), TilePosition(+tileRadius, +tileRadius)})
	{
		TilePosition t = TilePosition(pos) + delta;
		if (ai()->GetMap().Valid(t))
			if (VBase * base = inMyBase(center(t)))
				MyBasesAround.emplace(roundedDist(base->BWEMPart()->Center(), pos), base);
	}

	return MyBasesAround.empty() ? nullptr : MyBasesAround.begin()->second;
}


VBase * findMyClosestBase(Position pos, int maxTileDistanceToArea)
{

	if (VBase * base = inMyBase(pos))
		return base;

	if (VBase * base = nearMyBase(pos, maxTileDistanceToArea))
		return base;

	return nullptr;
}


static vector<MyUnit *> findDefensers(UnitType type, VBase * base)
{
	vector<MyUnit *> Res;

	Position basePos = base->BWEMPart()->Center();
	multimap<int, MyUnit *> Candidates;
	for (const auto & u : me().Units(type))
		if (u->Completed() && !u->Loaded())
		if (!u->GetBehavior()->IsRepairing())
		if (!u->GetBehavior()->IsVChasing())
		if (!u->GetBehavior()->IsDestroying())
		if (!u->GetBehavior()->IsKillingMine())
		if (!u->GetBehavior()->IsLaying())
		if (!u->GetBehavior()->IsFighting())
//		if (!(u->GetBehavior()->IsRaiding() && (u->GetBehavior()->IsRaiding()->Target() == base->BWEMPart()->Center())))
		{
			int dist = type.isFlyer() ? roundedDist(basePos, u->Pos()) : groundDist(basePos, u->Pos());
			if (dist < 10000)
			{
				Candidates.emplace(dist, u.get());
			}
		}

	for (const auto & cand : Candidates)
		Res.push_back(cand.second);

	return Res;
}




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class BaseDefense
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


BaseDefense::BaseDefense()
{
}


BaseDefense::~BaseDefense()
{
}


string BaseDefense::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


int BaseDefense::EvalVulturesNeededAgainst_Terran(const vector<const HisKnownUnit *> & Threats) const
{
	int scvs = 0;
	int marines = 0;
	int medics = 0;
	int firebats = 0;
	int ghosts = 0;
	int vultures = 0;
	int tanks = 0;
	int goliaths = 0;

	for (const auto & trace : Threats)
		switch(trace->Type())
		{
		case Terran_SCV:					++scvs; break;
		case Terran_Marine:					++marines; break;
		case Terran_Medic:					++medics; break;
		case Terran_Firebat:				++firebats; break;
		case Terran_Ghost:					++ghosts; break;
		case Terran_Vulture:				++vultures; break;
		case Terran_Siege_Tank_Tank_Mode:
		case Terran_Siege_Tank_Siege_Mode:	++tanks; break;
		case Terran_Goliath:				++goliaths; break;
		}

	if (medics)
	{
		marines *= 2;
		firebats *= 2;
		ghosts *= 2;
	}

	int vulturesNeeded = 0;
	vulturesNeeded += (scvs+2) / 4;
	vulturesNeeded += (marines+1) / 2;
	vulturesNeeded += (firebats+2) / 3;
	vulturesNeeded += (ghosts+1) / 2;
	vulturesNeeded += int(vultures * 1.5);
	vulturesNeeded += tanks * 5;
	vulturesNeeded += goliaths * 4;

	return vulturesNeeded;
}


int BaseDefense::EvalVulturesNeededAgainst_Protoss(const vector<const HisKnownUnit *> & Threats) const
{
	int probes = 0;
	int zealots = 0;
	int dragoons = 0;
	int reavers = 0;
	int highTemplars = 0;
	int darkTemplars = 0;
	int archons = 0;
	int darkArchons = 0;

	for (const auto & trace : Threats)
		switch(trace->Type())
		{
		case Protoss_Probe:					++probes; break;
		case Protoss_Zealot:				++zealots; break;
		case Protoss_Dragoon:				++dragoons; break;
		case Protoss_Reaver:				++reavers; break;
		case Protoss_High_Templar:			++highTemplars; break;
		case Protoss_Dark_Templar:			++darkTemplars; break;
		case Protoss_Archon:				++archons; break;
		case Protoss_Dark_Archon:			++darkArchons; break;
		}

	int vulturesNeeded = 0;
	vulturesNeeded += (probes+1) / 4;
	vulturesNeeded += zealots;
	vulturesNeeded += dragoons * 4;
	vulturesNeeded += reavers * 5;
	vulturesNeeded += highTemplars * 2;
	vulturesNeeded += darkTemplars * 2;
	vulturesNeeded += archons;
	vulturesNeeded += darkArchons * 2;

	return vulturesNeeded;
}


int BaseDefense::EvalVulturesNeededAgainst_Zerg(const vector<const HisKnownUnit *> & Threats) const
{
	int drones = 0;
	int defilers = 0;
	int hydralisks = 0;
	int ultralisks = 0;
	int zerglings = 0;
	int lurkers = 0;

	for (const auto & trace : Threats)
		switch(trace->Type())
		{
		case Zerg_Drone:					++drones; break;
		case Zerg_Defiler:					++defilers; break;
		case Zerg_Hydralisk:				++hydralisks; break;
		case Zerg_Ultralisk:				++ultralisks; break;
		case Zerg_Zergling:					++zerglings; break;
		case Zerg_Lurker:					++lurkers; break;
		}

	int vulturesNeeded = 0;
	vulturesNeeded += (drones+1) / 4;
	vulturesNeeded += (zerglings+1) / 2;
	vulturesNeeded += lurkers * 3;
	vulturesNeeded += hydralisks * 2;
	vulturesNeeded += defilers * 2;
	vulturesNeeded += ultralisks * 6;

	return vulturesNeeded;
}


int BaseDefense::EvalVulturesNeededAgainst(const vector<const HisKnownUnit *> & Threats) const
{
	if (him().IsTerran()) return EvalVulturesNeededAgainst_Terran(Threats);
	if (him().IsProtoss()) return EvalVulturesNeededAgainst_Protoss(Threats);
	if (him().IsZerg()) return EvalVulturesNeededAgainst_Zerg(Threats);
	assert_throw(false);
	return 0;
}


void BaseDefense::OnFrame_v()
{
	if (!him().IsZerg()) return;

	if (ai()->Frame() % 25 != 0) return;

	map<VBase *, vector<const HisKnownUnit *>> TreatsByBase;
	for (const auto & info : him().AllUnits())
		if (info.first->exists())
			if (ai()->Frame() - info.second.LastTimeVisible() < 50)
				if (VBase * base = findMyClosestBase(info.second.LastPosition()))
					if (base->BWEMPart()->GetArea()->Bases().size() == 1)
						TreatsByBase[base].push_back(&info.second);

	multimap<int, VBase *> map_vulturesNeeded_base;
	for (const auto & e : TreatsByBase)
		if (int vulturesNeeded = EvalVulturesNeededAgainst(e.second))
			map_vulturesNeeded_base.emplace(vulturesNeeded, e.first);

	m_active = !map_vulturesNeeded_base.empty();
	m_pAttackedBase = nullptr;
	if (m_active)
	{
	///	DO_ONCE ai()->SetDelay(500);

		// only consider the base which needs vultures the most:
		m_pAttackedBase = map_vulturesNeeded_base.rbegin()->second;
		int vulturesNeeded = map_vulturesNeeded_base.rbegin()->first;
		assert_throw(vulturesNeeded);

		vector<MyUnit *> Candidates = findDefensers(Terran_Vulture, m_pAttackedBase);
		if (him().IsProtoss())
		{
			vector<MyUnit *> MarinesCandidates = findDefensers(Terran_Marine, m_pAttackedBase);
			Candidates.insert(Candidates.end(), MarinesCandidates.begin(), MarinesCandidates.end());
			vulturesNeeded += 3;
		}

		for (MyUnit * cand : Candidates)
		{
			if (!(cand->GetBehavior()->IsRaiding() && (cand->GetBehavior()->IsRaiding()->Target() == m_pAttackedBase->BWEMPart()->Center())))
				cand->ChangeBehavior<Raiding>(cand, m_pAttackedBase->BWEMPart()->Center());
			if (--vulturesNeeded == 0) break;
		}

		const vector<const HisKnownUnit *> & Threats = TreatsByBase[m_pAttackedBase];

		// recrut SCVs to defend the attacked Base (TODO: refactor with DragoonRush)
		if (ai()->GetStrategy()->Detected<DragoonRush>())
			if (vulturesNeeded > 0)
				if (ai()->Frame() - m_activeSince > 200)
					if (Threats.size()*4 < m_pAttackedBase->GetStronghold()->SCVs().size())
						if (all_of(Threats.begin(), Threats.end(), [this](const HisKnownUnit * trace)
								{ return trace->GetHisUnit() &&
										(dist(m_pAttackedBase->BWEMPart()->Center(), trace->LastPosition()) < 14*32) &&
										(	trace->Type() == Terran_Goliath ||
											trace->Type() == Terran_Siege_Tank_Tank_Mode ||
											trace->Type() == Terran_Siege_Tank_Siege_Mode ||
											trace->Type() == Protoss_Dragoon ||
											trace->Type() == Zerg_Hydralisk);  }))
							for (const HisKnownUnit * trace : Threats)
							{
								HisUnit * u = trace->GetHisUnit();
								if (u->Chasers().empty())
								{
									multimap<int, My<Terran_SCV> *> Candidates;
									for (My<Terran_SCV> * pSCV : m_pAttackedBase->GetStronghold()->SCVs())
										if (pSCV->Completed() && !pSCV->Loaded())
											if (pSCV->Life() >= 40)
												if (!pSCV->GetBehavior()->IsConstructing())
												if (!pSCV->GetBehavior()->IsChasing())
												if (!pSCV->GetBehavior()->IsWalking())
												if (!(pSCV->GetBehavior()->IsRepairing() &&
													  pSCV->GetBehavior()->IsRepairing()->TargetX() &&
													  pSCV->GetBehavior()->IsRepairing()->TargetX()->Is(Terran_Siege_Tank_Siege_Mode)
													))
													Candidates.emplace(squaredDist(u->Pos(), pSCV->Pos()), pSCV);

									if (Candidates.size() >= 4)
									{
										auto end = Candidates.begin();
										advance(end, 4);
										for (auto it = Candidates.begin() ; it != end ; ++it)
											it->second->ChangeBehavior<Chasing>(it->second, u, bool("insist"));
									}
								}
							}
	}
	else
	{
		m_activeSince = ai()->Frame();
	}
}




} // namespace iron



