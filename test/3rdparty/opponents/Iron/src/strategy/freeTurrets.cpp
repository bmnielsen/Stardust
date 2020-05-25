//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "freeTurrets.h"
#include "strategy.h"
#include "wraithRush.h"
#include "../behavior/fleeing.h"
#include "../units/cc.h"
#include "../units/army.h"
#include "../territory/vgridMap.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class FreeTurrets
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


FreeTurrets::FreeTurrets()
{
}


string FreeTurrets::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


void FreeTurrets::CheckNewConstruction()
{
	if (ai()->Frame() - m_lastTimeSucceed < 10) return;

	m_nextLocation = TilePositions::None;
	m_pBuilder = nullptr;

	multimap<int, pair<int, int>> Candidates;

	int bestScore = 0;

	for (int j = 0 ; j < ai()->GetGridMap().Height() ; ++j)
	for (int i = 0 ; i < ai()->GetGridMap().Width() ; ++i)
		//if (!contains(me().EnlargedAreas(), ai()->GetMap().GetArea(ai()->GetGridMap().GetCenter(i, j))))
		{
			const GridMapCell & Cell = ai()->GetGridMap().GetCell(i, j);
			if ((Cell.AvgHisUnitsAndBuildingsLast256Frames() == 0) && (Cell.HisUnits.size() == 0))
			if ((Cell.AvgMyFreeUnitsAndBuildingsLast256Frames() >= 3) && (Cell.MyUnits.size() >= 3) && (Cell.MyUnits.front()->GetArea(check_t::no_check)))
			{
				int turrets = count_if(Cell.MyBuildings.begin(), Cell.MyBuildings.end(), [](MyBuilding * b){ return b->Is(Terran_Missile_Turret); });
				if (turrets < (NeedManyManyTurrets() ? 6 : NeedManyTurrets() ? 5 : 4))
				{
				//	int mines = count_if(Cell.MyBuildings.begin(), Cell.MyBuildings.end(), [](MyBuilding * b){ return b->Is(Terran_Vulture_Spider_Mine); });
					int score = Cell.AvgMyFreeUnitsAndBuildingsLast256Frames() - turrets;// - mines;
					bestScore = max(bestScore, score);
					if (score >= 2) Candidates.emplace(score, make_pair(i, j));
				}
			}
		}

	if (Candidates.size() == 0) return;
	if (bestScore == 2)
	{
		if (Candidates.size() == 1) return;

		if (Candidates.size() >= 2)
			if (Candidates.rbegin()->first == 2)
			{
				int i1, j1, i2, j2;
				tie(i1, j1) = Candidates.rbegin()->second;
				tie(i2, j2) = (++Candidates.rbegin())->second;
				if (!((abs(i1 - i2) <= 1) && (abs(j1 - j2) <= 1)))
					return;
			}
	}

	int iCand, jCand;
	tie(iCand, jCand) = Candidates.rbegin()->second;
	TilePosition focusTile = ai()->GetGridMap().GetCenter(iCand, jCand);
	const GridMapCell & Cand = ai()->GetGridMap().GetCell(iCand, jCand);

	if (!Cand.MyUnits.front()->GetArea(check_t::no_check)) return;
	vector<const Area *> Areas = {Cand.MyUnits.front()->GetArea()};
	for (const Area * a : Areas.front()->AccessibleNeighbours())
		Areas.push_back(a);

	const int radius = 10;

	vector<MyUnit *> MyUnitsAround = ai()->GetGridMap().GetMyUnits(ai()->GetMap().Crop(focusTile-(radius+2)), ai()->GetMap().Crop(focusTile+(radius+2)));
	vector<MyBuilding *> MyBuildingsAround = ai()->GetGridMap().GetMyBuildings(ai()->GetMap().Crop(focusTile-(radius+6)), ai()->GetMap().Crop(focusTile+(radius+6)));
	
	vector<MyUnit *> MySiegedTanksAround;
	for (MyUnit * u : MyUnitsAround)
		if (u->GetBehavior()->IsSieging())
			MySiegedTanksAround.push_back(u);

	vector<HisBuilding *> HisBuildingsAround;
	for (const auto & b : him().Buildings())
		if (dist(b->Pos(), center(focusTile)) < (radius + 10)*32)
			HisBuildingsAround.push_back(b.get());

	vector<HisUnit *> HisUnitsAround;
	for (const auto & u : him().Units())
		if (dist(u->Pos(), center(focusTile)) < (radius + 16)*32)
			if (u->GroundAttack())
				HisUnitsAround.push_back(u.get());

	for (const auto & u : me().Units(Terran_SCV))
		if (!u->GetStronghold())
			if (dist(u->Pos(), center(focusTile)) < (radius + 10)*32)
				if (u->GetBehavior()->IsHarassing() ||
					u->GetBehavior()->IsRepairing() ||
					u->GetBehavior()->IsDefaultBehavior() ||
					u->GetBehavior()->IsScouting() ||
					u->GetBehavior()->IsFleeing() 
						&& none_of(u->FaceOffs().begin(), u->FaceOffs().end(), [](const FaceOff & fo)
								{
									return
										fo.HisAttack() &&
										fo.His()->IsHisUnit() &&
										!fo.His()->InFog() &&
										!fo.His()->Type().isWorker() &&
										(fo.DistanceToHisRange() < 5*32);
								}))
					m_pBuilder = u->IsMy<Terran_SCV>();

	if (!m_pBuilder) return;


	const int countFreeTurrets = count_if(me().Buildings(Terran_Missile_Turret).begin(), me().Buildings(Terran_Missile_Turret).end(),
				[](const unique_ptr<MyBuilding> & b) { return !b->GetStronghold(); });

	const int countTurretsAround = count_if(MyBuildingsAround.begin(), MyBuildingsAround.end(), [](MyBuilding * b)
			{ return b->Is(Terran_Missile_Turret); });

	const int basePriority =
		NeedManyManyTurrets() ? 530 :
		NeedManyTurrets() || (him().IsZerg() && me().CompletedBuildings(Terran_Command_Center) <= 1) ? 480 :
		m_needTurrets ? 450 :
		400;

	m_priority = max(basePriority - countTurretsAround*(NeedManyManyTurrets() ? 20 : NeedManyTurrets() ? 30 : 40), 100);

	if (him().MayMuta())
//	if (him().IsZerg())
		if (countFreeTurrets < 3)
			if (me().CompletedBuildings(Terran_Command_Center) <= 1)
				m_priority = 490;

	map<int, TilePosition> Locations;
	TilePosition boundingBoxTopLeft = ai()->GetMap().Crop(focusTile - radius);
	TilePosition boundingBoxBottomRight = ai()->GetMap().Crop(focusTile + radius) - 1;
	for (int y = boundingBoxTopLeft.y ; y <= boundingBoxBottomRight.y ; ++y)
	for (int x = boundingBoxTopLeft.x ; x <= boundingBoxBottomRight.x ; ++x)
	{
		TilePosition t(x, y);
		CHECK_POS(t);
		Position pos(t+1);
		if (contains(Areas, ai()->GetMap().GetArea(t)))
		{
			bool canBuild = true;
			for (int dy = 0 ; dy < 2 ; ++dy) if (canBuild)
			for (int dx = 0 ; dx < 2 ; ++dx) if (canBuild)
			{
				CHECK_POS(t + TilePosition(dx, dy));
				const Tile & tile = ai()->GetMap().GetTile(t + TilePosition(dx, dy), check_t::no_check);
				if (!tile.Buildable() || tile.GetNeutral() ||
					ai()->GetVMap().GetBuildingOn(tile) || ai()->GetVMap().AddonRoom(tile) ||
					ai()->GetVMap().CommandCenterWithAddonRoom(tile) ||
					ai()->GetVMap().FirstTurretsRoom(tile) ||
					(tile.MinAltitude() < 32) || (bw->isVisible(t) && bw->hasCreep(t)))
					canBuild = false;
			}
			if (!canBuild) continue;

			if (any_of(MyUnitsAround.begin(), MyUnitsAround.end(), [pos](MyUnit * u)
						{ return (u->Is(Terran_Vulture_Spider_Mine) && (squaredDist(u->Pos(), pos) < 32*32))
							  || (u->Is(Terran_Siege_Tank_Siege_Mode) && (squaredDist(u->Pos(), pos) < 56*56)); }))
				continue;
			if (any_of(MyBuildingsAround.begin(), MyBuildingsAround.end(), [t, this, countTurretsAround](MyBuilding * b)
						{ return b->Is(Terran_Missile_Turret) && (squaredDist(b->TopLeft(), t) <
								(NeedManyManyTurrets() ? 3*3 :
								NeedManyTurrets() || him().MayMuta() && countTurretsAround ? 4*4 :
								6*6));
						}))
				continue;

			if (any_of(HisBuildingsAround.begin(), HisBuildingsAround.end(), [pos](HisBuilding * b)
						{ return (squaredDist(b->Pos(), pos) < (32*9)*(32*9)); }))
				continue;

			if (any_of(HisUnitsAround.begin(), HisUnitsAround.end(), [t](HisUnit * u)
						{ return (distToRectangle(u->Pos(), t, UnitType(Terran_Missile_Turret).tileSize()) < u->GroundRange() + 16); }))
				continue;

#if DEV
		///	bw->drawBoxMap(Position(t)+3, Position(t + 1)-4 , Colors::Orange);
#endif
			int closeToMySiegedTanksFactor = 0;
			for (MyUnit * u : MySiegedTanksAround)
				closeToMySiegedTanksFactor += max(0, radius*32 - roundedDist(Position(t), u->Pos()));

			int closeToMyTurretsFactor = 0;
			if (him().MayMuta())
				for (MyBuilding * b : MyBuildingsAround)
					if (b->Is(Terran_Missile_Turret))
						closeToMyTurretsFactor += max(0, radius*32 - roundedDist(Position(t), b->Pos()));

			int score =
				  roundedDist(Position(t), him().StartingBase()->Center())
				+ ai()->GetMap().GetTile(t).MinAltitude()
				+ closeToMySiegedTanksFactor*2
				+ closeToMyTurretsFactor*2;
			Locations[score] = t;
		}
	}

	if (Locations.empty())
	{
		m_nextLocation = TilePositions::None;
		m_priority = 0;
		m_pBuilder = nullptr;
	}
	else
	{
#if DEV
	///	bw->drawBoxMap(Position(Locations.rbegin()->second)+3, Position(Locations.rbegin()->second + 1)-4 , Colors::Orange, true);
#endif
		m_nextLocation = Locations.rbegin()->second;
		m_lastTimeSucceed = ai()->Frame();
	}

#if DEV
///	bw->drawBoxMap(	Position(ai()->GetGridMap().GetTopLeft(iCand, jCand)), Position(ai()->GetGridMap().GetBottomRight(iCand, jCand) + 1), Colors::Yellow);
#endif
}


void FreeTurrets::OnFrame_v()
{
	if (!me().CompletedBuildings(Terran_Engineering_Bay) ||
		him().HydraPressure_needVultures() ||
		him().HydraPressure() ||
		me().Army().KeepTanksAtHome())
	{
		m_active = false;
		return;
	}

	if (m_active)
	{
		const int mutas = him().AllUnits(Zerg_Mutalisk).size();
		const int lurkers = him().AllUnits(Zerg_Lurker).size();
		const int carriers = him().AllUnits(Protoss_Carrier).size();
		const int scouts = him().AllUnits(Protoss_Scout).size();
		const int wraiths = him().AllUnits(Terran_Wraith).size();
//		const int shuttles = him().AllUnits(Protoss_Shuttle).size();
//		const int observers = him().AllUnits(Protoss_Observer).size();


		if ((wraiths >= 10) || (scouts >= 10) || (mutas >= 7)) SetNeedManyManyTurrets(true);
		else
		{
			SetNeedManyManyTurrets(false);

			if (//ai()->GetStrategy()->Detected<WraithRush>() ||
				(wraiths >= 4) ||
				(scouts >= 6) ||
				(mutas >= 4) ||
				//(shuttles + observers >= 3) ||
				(lurkers >= 1) ||
				(carriers >= 1))
				SetNeedManyTurrets(true);
			else SetNeedManyTurrets(false);
		}

	//	SetNeedManyTurrets(true);

		CheckNewConstruction();
	}
	else
	{
		if (!him().IsZerg() || me().Buildings(Terran_Command_Center).size() >= 2)
		{
	///			ai()->SetDelay(100);
			m_active = true;
			m_activeSince = ai()->Frame();
			return;
		}
	}
}


} // namespace iron



