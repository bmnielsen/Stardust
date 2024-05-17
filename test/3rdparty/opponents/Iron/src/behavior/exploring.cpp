//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "exploring.h"
#include "walking.h"
#include "repairing.h"
#include "fleeing.h"
#include "kiting.h"
#include "sniping.h"
#include "harassing.h"
#include "razing.h"
#include "raiding.h"
#include "sieging.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/shallowTwo.h"
#include "../territory/vgridMap.h"
#include "../units/bunker.h"
#include "../units/army.h"
#include "../units/factory.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Exploring
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Exploring *> Exploring::m_Instances;


Exploring::Exploring(MyUnit * pAgent, const Area * pWhere)
	: Behavior(pAgent, behavior_t::Exploring), m_pWhere(pWhere)
{
	assert_throw(pAgent->Flying() || pWhere->AccessibleFrom(pAgent->GetArea()));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

	m_inStateSince = ai()->Frame();
}


Exploring::~Exploring()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void Exploring::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Exploring::StateName() const
{CI(this);
	switch(State())
	{
	case reachingArea:		return "reachingArea";
	case lastVisited:		return "lastVisited";
	case random:			return "random";
	default:				return "?";
	}
}


void Exploring::OnFrame_v()
{CI(this);
#if DEV
	if ((State() != reachingArea) && (m_target != Positions::None)) drawLineMap(Agent()->Pos(), m_target, GetColor());//1
#endif


	if (!Agent()->CanAcceptCommand()) return;

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

	if (Agent()->GroundRange() > 3*32)
	{
		if (Agent()->Is(Terran_Marine))
			if ((me().Bases().size() == 1) ||
				any_of(Agent()->FaceOffs().begin(), Agent()->FaceOffs().end(), [](const FaceOff & fo)
							{ return fo.His()->Is(Zerg_Mutalisk); }))
//				ai()->GetStrategy()->Detected<ZerglingRush>() ||
//				ai()->GetStrategy()->Detected<MarineRush>())
				if (My<Terran_Bunker> * pBunker = Sniping::FindBunker(Agent(), 25*32))
				{
					if (Sniping::Instances().empty())
						return Agent()->ChangeBehavior<Sniping>(Agent(), pBunker);

					if (dist(Agent()->Pos(), pBunker->Pos()) > 32*6)
					{
						return Agent()->ChangeBehavior<Raiding>(Agent(), pBunker->Pos());
					}
				}


		if (Kiting::KiteCondition(Agent()) || Kiting::AttackCondition(Agent()))
			return Agent()->ChangeBehavior<Kiting>(Agent());
	}
	else
	{
		auto Threats3 = findThreats(Agent(), 3*32);
		if (!Threats3.empty())
			return Agent()->ChangeBehavior<Fleeing>(Agent());
	}

	if (Agent()->Type().isMechanical())
		if (Agent()->Life() < Agent()->MaxLife())
			if (Agent()->RepairersCount() < Agent()->MaxRepairers())
				if (Repairing * pRepairer = Repairing::GetRepairer(Agent(),
							(Agent()->Life()*4 > Agent()->MaxLife()*3) ? 16*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*2) ? 32*32 :
							(Agent()->Life()*4 > Agent()->MaxLife()*1) ? 64*32 : 1000000))
					return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);

	if (State() != reachingArea)
	{
		const frame_t delayBeforeRazing = Agent()->Is(Terran_Vulture) ? 100 : 0;
		if (ai()->Frame() - m_inStateSince > delayBeforeRazing)
		{
			if (!(Agent()->Is(Terran_SCV) && (Harassing::Instances().size() == 0) && (Agent()->GetArea() == him().GetArea())))
				if (Razing::Condition(Agent()))
					return Agent()->ChangeBehavior<Razing>(Agent(), Agent()->FindArea());

			if (!him().StartingBaseDestroyed())
				if (him().FoundBuildingsInMain())
					if (Agent()->GetArea(check_t::no_check) == him().GetArea())
						if (none_of(him().Buildings().begin(), him().Buildings().end(),
								[](const unique_ptr<HisBuilding> & b){ return !b->Flying() && (b->GetArea() == him().GetArea()); }))
							him().SetStartingBaseDestroyed();

			if (!Agent()->Is(Terran_Marine) || !Agent()->GetStronghold())
			if (!Agent()->Is(Terran_Medic) || !Agent()->GetStronghold())
			if (!Agent()->Is(Terran_Siege_Tank_Tank_Mode) || !Agent()->GetStronghold())
			if (!Agent()->Is(Terran_Goliath) || !Agent()->GetStronghold())
			if (!Agent()->Is(Terran_SCV))
			{
				for (const auto & b : him().Buildings())
					if (Agent()->Flying() ||
						(!b->Flying() && b->GetArea()->AccessibleFrom(Agent()->GetArea())))
							return Agent()->ChangeBehavior<Raiding>(Agent(), b->Pos());

				if (!Agent()->Flying())
					for (const Area & area : ai()->GetMap().Areas())
						if (area.AccessibleFrom(Agent()->GetArea()))
							return Agent()->ChangeBehavior<Raiding>(Agent(), center(area.Top()));
			}
		}
	}

	if (Agent()->Is(Terran_Goliath) && me().Army().KeepGoliathsAtHome())
		if (Agent()->GetStronghold())
			for (const auto & b : me().Buildings(Terran_Missile_Turret))
				if (b->GetStronghold())
					if (contains(me().EnlargedAreas(), b->GetArea()))
						if (b->Life() < b->PrevLife(10))
							if (b->GetArea() != Agent()->GetArea())
							{
								return Agent()->ChangeBehavior<Exploring>(Agent(), b->GetArea());
							}


	switch (State())
	{
	case reachingArea:	OnFrame_reachingArea(); break;
	case lastVisited:	OnFrame_lastVisited(); break;
	case random:		OnFrame_random(); break;
	default: assert_throw(false);
	}
}


void Exploring::OnFrame_reachingArea()
{CI(this);
//	const bool inHisArea = (Agent()->GetArea() == HisBase()->BWEMPart()->GetArea());

	if (const Area * pAgentArea = Agent()->GetArea(check_t::no_check))
		if (pAgentArea == Where())
		{
			ResetSubBehavior();
			return ChangeState(lastVisited);
		}

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		if (Agent()->Flying())
			return Agent()->Move(Position(Where()->Top()));
		else
			return SetSubBehavior<Walking>(Agent(), Position(Where()->Top()), __FILE__ + to_string(__LINE__));
	}

	if (ai()->Frame() - m_inStateSince > 5)
		if (Agent()->Unit()->isIdle())
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


void Exploring::OnFrame_lastVisited()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}

	if (him().StartingBaseDestroyed() && Agent()->Flying())
		return ChangeState(random);
	
	if (Agent()->Is(Terran_SCV) && (Agent()->GetArea() == him().GetArea()))
		return ChangeState(random);

	if (Agent()->Is(Terran_Marine) && ai()->GetStrategy()->Active<Walling>())
		return ChangeState(random);

	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
		return ChangeState(random);

	if (Target() != Positions::None)
		if (ai()->Frame() - m_inStateSince > 10)
			if (ai()->Frame() - Agent()->LastFrameMoving() > 5)
			{
				ai()->GetGridMap().GetCell(TilePosition(Target())).lastFrameVisible = ai()->Frame();
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
			}

	{
		int i1, j1, i2, j2;

		tie(i1, j1) = ai()->GetGridMap().GetCellCoords(Where()->TopLeft());
		tie(i2, j2) = ai()->GetGridMap().GetCellCoords(Where()->BottomRight());

		multimap<int, Position> Targets;

		vector<HisUnit *> ThreatsInArea;

		for (int j = j1 ; j <= j2 ; ++j)
		for (int i = i1 ; i <= i2 ; ++i)
		{
			Position cellCenter = Position(TilePosition(i*VGridMap::cell_width_in_tiles, j*VGridMap::cell_width_in_tiles)) +
								  Position(TilePosition(VGridMap::cell_width_in_tiles, VGridMap::cell_width_in_tiles)) / 2;
			Position target = nearestEmpty(cellCenter);
			CHECK_POS(target);
			if (ai()->GetMap().GetArea(WalkPosition(target)) == Where())
			{
				auto & Cell = ai()->GetGridMap().GetCell(i, j);
				ThreatsInArea.insert(ThreatsInArea.end(), Cell.HisUnits.begin(), Cell.HisUnits.end());

				if (none_of(Exploring::Instances().begin(), Exploring::Instances().end(),
							[target, this](const Exploring * e){ return (e != this) && (e->Target() == target); }))
					if (int time = ai()->Frame() - Cell.lastFrameVisible)
						Targets.emplace(time, target);
			}
		}

		if (Targets.size() < 5) return ChangeState(random);

		for (HisUnit * u : ThreatsInArea)
			if (FaceOff(Agent(), u).MyAttack())
				return Agent()->Move(m_target = u->Pos());

		Position newTarget = Targets.rbegin()->second;
		if (newTarget != m_target)
			return Agent()->Move(m_target = newTarget);
	}
}


void Exploring::OnFrame_random()
{CI(this);
	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}

	if (ai()->Frame() - m_inStateSince > 10)
		if (ai()->Frame() - Agent()->LastFrameMoving() > 5)
		{
			return Agent()->ChangeBehavior<Exploring>(Agent(), Where());
		}


	if (Agent()->Is(Terran_Goliath) || Agent()->Is(Terran_Siege_Tank_Tank_Mode) || Agent()->Is(Terran_Medic))
		if (Agent()->GetStronghold())
			if (!me().Army().KeepGoliathsAtHome())
			if (!me().Army().KeepTanksAtHome())
				return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
		if (Agent()->GetStronghold())
			if (!ai()->GetStrategy()->Active<Walling>())
				if (Where() == me().GetArea())
					if (rand() % 200 == 0)
					{
					///	bw << Agent()->NameWithId() << " DefaultBehavior" << endl;
						return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
					}

	int minDistToReachTarget = 4*32;
	if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
		if (me().Army().KeepTanksAtHome())
			minDistToReachTarget = 1*32;

	if ((Target() == Positions::None) || (Agent()->Pos().getApproxDistance(Target()) < minDistToReachTarget))
	{
		if (him().StartingBaseDestroyed() && Agent()->Flying())
		{
			m_target = ai()->GetMap().RandomPosition();
			return Agent()->Move(m_target);
		}

		const Tile & AgentTile = ai()->GetMap().GetTile(TilePosition(Agent()->Pos()));
		if (Target() != Positions::None)
			if (auto pTank = Agent()->IsMy<Terran_Siege_Tank_Tank_Mode>())
				if (me().Army().KeepTanksAtHome())
					if (me().HasResearched(TechTypes::Tank_Siege_Mode))
					{
						bool groundTargetsNearby = false;
						for (UnitType t : {Protoss_Dragoon, Protoss_Reaver})
							for (HisUnit * u : him().Units(t))
								if (roundedDist(u->Pos(), Agent()->Pos()) < 22*32)
									groundTargetsNearby = true;

						if (ai()->GetStrategy()->Active<Walling>() && !groundTargetsNearby ||
							ai()->GetVMap().InsideWall(AgentTile) && !groundTargetsNearby ||
							any_of(me().Bases().begin(), me().Bases().end(), [pTank, AgentTile](const VBase * base)
								{ return base->Active() && (dist(base->BWEMPart()->Center(), pTank->Pos()) < 4*32) &&
										!ai()->GetVMap().CommandCenterWithAddonRoom(AgentTile) &&
										(ai()->GetVMap().SCVTraffic(AgentTile) || (dist(base->BWEMPart()->Center(), pTank->Pos()) < 3*32)) ; }))
						return pTank->ChangeBehavior<Sieging>(pTank);
					}

		TilePosition topLeftAreas     = {numeric_limits<int>::max(), numeric_limits<int>::max()};
		TilePosition bottomRightAreas = {numeric_limits<int>::min(), numeric_limits<int>::min()};
		
		vector<const Area *> Areas{Where()};
		for (VArea & area : ai()->GetVMap().Areas())
			if (contains(area.EnlargedArea(), Where()))
				Areas = area.EnlargedArea();
			
		for (const Area * area : Areas)
		{
			makeBoundingBoxIncludePoint(topLeftAreas, bottomRightAreas, area->TopLeft());
			makeBoundingBoxIncludePoint(topLeftAreas, bottomRightAreas, area->BottomRight());
		}

		topLeftAreas = ai()->GetMap().Crop(topLeftAreas - 5);
		bottomRightAreas = ai()->GetMap().Crop(bottomRightAreas + 5);


		int minDistToNaturalBase = numeric_limits<int>::max();
		if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
			//if (auto s = ai()->GetStrategy()->Active<Walling>())
			if (me().Army().KeepTanksAtHome())
				if (ai()->GetStrategy()->Active<Walling>() ||
					ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(Agent()->Pos()))))
			{
				for (int tries = 0 ; tries < 300 ; ++tries)
				{
					TilePosition t = topLeftAreas + TilePosition(rand() % (1 + bottomRightAreas.x - topLeftAreas.x),
																 rand() % (1 + bottomRightAreas.y - topLeftAreas.y));

					if (!contains(Areas, ai()->GetMap().GetArea(t))) continue;

					Position target = nearestEmpty(center(t));
					CHECK_POS(target);
					if (!contains(Areas, ai()->GetMap().GetArea(WalkPosition(target)))) continue;

					if (!ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(target)))) continue;

					minDistToNaturalBase = min(minDistToNaturalBase, roundedDist(target, me().Bases()[min(1, (int)me().Bases().size()-1)]->BWEMPart()->Center()));
				}
			}

		for (int tries = 0 ; tries < 300 ; ++tries)
		{
			TilePosition t = topLeftAreas + TilePosition(rand() % (1 + bottomRightAreas.x - topLeftAreas.x),
													     rand() % (1 + bottomRightAreas.y - topLeftAreas.y));

			if (!contains(Areas, ai()->GetMap().GetArea(t))) continue;

			Position target = nearestEmpty(center(t));
			CHECK_POS(target);
			if (!contains(Areas, ai()->GetMap().GetArea(WalkPosition(target)))) continue;

			if (tries < 250)
			{
				if (Agent()->Is(Terran_Marine))
					if (auto s = ai()->GetStrategy()->Active<Walling>())
					{
						if (s->GetWall()->DistanceTo(target) > 4*32) continue;
						if (!ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(target)))) continue;
					}

				if (Agent()->Is(Terran_Siege_Tank_Tank_Mode))
					//if (auto s = ai()->GetStrategy()->Active<Walling>())
					if (me().Army().KeepTanksAtHome())
					{
						if (ai()->GetStrategy()->Active<Walling>() ||
							ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(Agent()->Pos()))))

						{
						//	bw->drawCircleMap(target, 10, Colors::Red, true);
							if (!ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(target)))) continue;
						//	bw->drawCircleMap(target, 10, Colors::Orange, true);
							if (roundedDist(target, me().Bases()[min(1, (int)me().Bases().size()-1)]->BWEMPart()->Center()) > minDistToNaturalBase + 4*32) continue;
						//	bw->drawCircleMap(target, 10, Colors::White, true);
						}
						else
						{
							if (me().Units(Terran_Siege_Tank_Tank_Mode).size() < 10)
								if (dist(me().Bases().back()->BWEMPart()->Center(), target) > 4*32)
									continue;
						}
					}


				if (ai()->GetStrategy()->Has<ZerglingRush>())
					if (him().StartingBase())
						if (groundDist(me().GetBase(0)->Center(), target) - 3*32 >
							groundDist(me().GetBase(0)->Center(), him().StartingBase()->Center()))
							continue;
			}
			else
				if (Agent()->Is(Terran_Marine))
					if (auto s = ai()->GetStrategy()->Active<Walling>()) return;
			
			return Agent()->Move(m_target = target);
		}
	}
}



} // namespace iron



