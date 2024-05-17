//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "wall.h"
#include "../behavior/constructing.h"
#include "../behavior/exploring.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../units/him.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }

namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Wall
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Wall::Wall(const ChokePoint * cp)
: ExampleWall(ai()->GetMap(), cp, him().IsProtoss() || him().IsTerran() ? tight_t::zealot : tight_t::zergling)
{
	m_innerArea = m_cp->GetAreas().first;
	m_outerArea = m_cp->GetAreas().second;
	if (ai()->GetMap().GetPath(Position(m_innerArea->Top()), Position(me().Player()->getStartLocation())).size() >
		ai()->GetMap().GetPath(Position(m_outerArea->Top()), Position(me().Player()->getStartLocation())).size())
		swap(m_innerArea, m_outerArea);
}


void Wall::Update()
{
	if (ai()->GetStrategy()->Active<Walling>())
	{
		m_distToClosestGroundEnemy = 15*32;
		const HisUnit * pClosestGroundEnemy = nullptr;
		if (auto * s = ai()->GetStrategy()->Active<Walling>())
			if (s->GetWall() == this)
			{
				for (const auto & u : him().Units())
					if (!u->Flying())
						if (!u->Type().isWorker())
							if (dist(u->Pos(), Center()) < m_distToClosestGroundEnemy + 10*32)
							{
								int d = DistanceTo(u->Pos());
								if (d < m_distToClosestGroundEnemy)
								{
									m_distToClosestGroundEnemy = d;
									pClosestGroundEnemy = u.get();
								}
							}
			}

		if (m_distToClosestGroundEnemy == 15*32)
			m_distToClosestGroundEnemy = numeric_limits<int>::max();


		m_openRequest = m_closeRequest = false;
	
		bool enemyNearby = DistToClosestGroundEnemy() < 12*32;

		if (enemyNearby)
		{
			if (him().IsProtoss())
				if (him().AllUnits(Protoss_Dark_Templar).empty())
				if ((int)him().AllUnits(Protoss_Dragoon).size() <= me().CompletedUnits(Terran_Siege_Tank_Tank_Mode))
				if ((int)him().AllUnits(Protoss_Zealot).size()+1 < me().CompletedUnits(Terran_Vulture) * 3/2)
				{
					enemyNearby = false;
				///	bw << "no more enemyNearby" << endl;
				}
		}

		if (enemyNearby) m_lastTimeEnemyNearby = ai()->Frame();
		bool threat = ai()->Frame() - m_lastTimeEnemyNearby < 250;

		if (Open())
		{
			if (enemyNearby)
			{
			///	bw << "close the wall because " << pClosestGroundEnemy->NameWithId() << endl;
				m_closeRequest = true;
			}
		}
		else
		{

			MyUnit * pUnitShouldGetOut = nullptr;
			MyUnit * pUnitShouldGetIn = nullptr;
			for (MyUnit * u : me().Units())
				if (u->Completed() && !u->Loaded())
					if (!u->Flying() && !u->Loaded())
					{
						if (u->Is(Terran_Marine))
							if (me().CreationCount(Terran_Vulture) + me().CreationCount(Terran_Siege_Tank_Tank_Mode) > 0)
								continue;

						int dist_unit_wall = DistanceTo(u->Pos());
						if ((dist_unit_wall > 1*32) && (dist_unit_wall < 12*32))
						{
							const Tile & tile = ai()->GetMap().GetTile(TilePosition(u->Pos()));
							if (!ai()->GetVMap().InsideWall(tile) && !ai()->GetVMap().OutsideWall(tile)) continue;

							if (ai()->GetVMap().InsideWall(tile))
							{
								if (u->GetBehavior()->IsScouting() ||
									u->GetBehavior()->IsHarassing() ||
									u->GetBehavior()->IsPatrollingBases() ||
									u->GetBehavior()->IsKiting() && u->Is(Terran_Vulture) && him().IsProtoss() ||
									u->GetBehavior()->IsRaiding())
									pUnitShouldGetOut = u;
							}
							else
							{
								if (u->GetBehavior()->IsMining() ||
									u->GetBehavior()->IsRefining() ||
									u->GetBehavior()->IsSupplementing() ||
									u->GetBehavior()->IsExploring())
									pUnitShouldGetIn = u;
							}
						}
					}

			if (pUnitShouldGetIn || pUnitShouldGetOut)
			{
			///	if (pUnitShouldGetIn)  bw << "let " << pUnitShouldGetIn->NameWithId()  << " get in" << endl;
			///	if (pUnitShouldGetOut)  bw << "let " << pUnitShouldGetOut->NameWithId()  << " get out" << endl;

				if (threat)
				{
				///	bw << "CANNOT because threat" << endl;
				}
				else
				{
					m_openRequest = true;
					if (pUnitShouldGetIn) pUnitShouldGetIn->ChangeBehavior<DefaultBehavior>(pUnitShouldGetIn);
					if (pUnitShouldGetOut) pUnitShouldGetOut->ChangeBehavior<DefaultBehavior>(pUnitShouldGetOut);
				}

			///	if (pUnitShouldGetIn) bw->drawLineMap(Center(), pUnitShouldGetIn->Pos(), Colors::Green);
			///	if (pUnitShouldGetOut) bw->drawLineMap(Center(), pUnitShouldGetOut->Pos(), Colors::Green);
			}
		}

	///	if (enemyNearby) bw->drawLineMap(Center(), pClosestGroundEnemy->Pos(), Colors::Red);
	///	if (enemyNearby) bw->drawLineMap(Center(), pClosestGroundEnemy->Pos(), Colors::Red);

	}
}


void Wall::DrawLocations() const
{
	const BWAPI::Color colorWall = BWAPI::Colors::Purple;

	ExampleWall::DrawLocations();

	for (VBase & base : ai()->GetVMap().Bases())
		if (base.GetWall() == this)
			bw->drawLineMap(Center(), base.BWEMPart()->Center(), colorWall);

	if (Possible()) bw->drawCircleMap(Center(), 5, colorWall, true);
}


void Wall::MapSpecificApply(int xCP, int yCP, int xBarrack, int yBarrack, int xDepot1, int yDepot1, int xDepot2, int yDepot2)
{
	if (TilePosition(m_cp->Center()) == TilePosition(xCP, yCP))
	{
		m_Locations.clear();
		m_Locations.emplace_back(xBarrack, yBarrack);
		if (xDepot1 != -1) m_Locations.emplace_back(xDepot1, yDepot1);
		if (xDepot2 != -1) m_Locations.emplace_back(xDepot2, yDepot2);

		m_BuildingTypes = {Terran_Barracks, Terran_Supply_Depot, Terran_Supply_Depot};
		m_BuildingTypes.resize(m_Locations.size());
	}
}


void Wall::MapSpecificRemove(int xCP, int yCP)
{
	if (TilePosition(m_cp->Center()) == TilePosition(xCP, yCP))
	{
		m_Locations.clear();
		m_BuildingTypes.clear();
	}
}



void Wall::MapSpecific()
{
	if (bw->mapHash() == "c8386b87051f6773f6b2681b0e8318244aa086a6")	//Neo Moon Glaive : depot could overlap CC
	{
		MapSpecificApply(49,16,		44,17,		43,15,		45,13);
	}

	if (bw->mapHash() == "d2f5633cc4bb0fca13cd1250729d5530c82c7451")	// Fighting Spirit : depot could overlap CC
	{
		MapSpecificApply(99,11,		92,16,		92,14,		93, 12);	// NE
	}

	if (bw->mapHash() == "5731c103687826de48ba3cc7d6e37e2537b0e902")	// Fighting Spirit 1.3 : depot could overlap CC
	{
		MapSpecificApply(99,11,		92,16,		92,14,		93, 12);	// NE
	}

	if (bw->mapHash() == "0a41f144c6134a2204f3d47d57cf2afcd8430841")	// Match Point : just minimize external side
	{
		MapSpecificApply(2,92,		4,93,		0,89,		2, 91);		// NE
	}

	if (!(him().IsProtoss() || him().IsTerran()))
	{
		if (bw->mapHash() == "af618ea3ed8a8926ca7b17619eebcb9126f0d8b1")	// Benzene
		{
			MapSpecificApply(21,78,		23,85,		20,81,		22, 83);	// just minimize external side
			MapSpecificApply(107,33,	104,28,		103,26,		101, 24);	// just minimize external side
		}

		if (bw->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b")	// Destination
		{
			MapSpecificApply(54,12,		48,10,		47,8,		47, 6);		// just minimize external side
			MapSpecificApply(41,115,	44,118,		46,116,		46, 114);	// just minimize external side
		}

		if (bw->mapHash() == "d9757c0adcfd61386dff8fe3e493e9e8ef9b45e3")	// neo Heartbreak Ridge
		{
			MapSpecificApply(113,40,		112,44,		110,42,		115, 42);		// just minimize external side

			MapSpecificRemove(14,58);
			MapSpecificRemove(8,59);
			MapSpecificRemove(0,71);
			MapSpecificApply(22,70,		17,69,		21,65,		19,67);

		}

		if (bw->mapHash() == "6f8da3c3cc8d08d9cf882700efa049280aedca8c")	// Heartbreak Ridge : wall the natural against Zerg
		{
			MapSpecificRemove(14,58);
			MapSpecificRemove(8,59);
			MapSpecificRemove(0,62);
			MapSpecificApply(22,70,		17,69,		21,65,		19,67);
		}
		
		if (bw->mapHash() == "9bfc271360fa5bab3707a29e1326b84d0ff58911")	// Tau cross
		{
			MapSpecificApply(13,26,		10,30,		6,26,		8, 28);
			MapSpecificApply(67,115,	58,115,		61,113);
		}
		
		if (bw->mapHash() == "d2f5633cc4bb0fca13cd1250729d5530c82c7451")	// Fighting Spirit
		{
			MapSpecificApply(36,107,	34,108,		37,106,		32, 106);	// SO

			MapSpecificRemove(115,98); MapSpecificRemove(85,115);			// SE (not zergling-proof)
		}
		
		if (bw->mapHash() == "5731c103687826de48ba3cc7d6e37e2537b0e902")	// Fighting Spirit 1.3
		{
			MapSpecificApply(36,107,	34,108,		37,106,		32, 106);	// SO

			MapSpecificRemove(115,98); MapSpecificRemove(85,115);			// SE (not zergling-proof)
		}
		
		if (bw->mapHash() == "0409ca0d7fe0c7f4083a70996a8f28f664d2fe37")	// Icarus
		{
			MapSpecificRemove(58,118);
			MapSpecificApply(50,112,	48,113,		51,111,		46,111);	// S

			MapSpecificApply(78,16,		78,16,		76,14,		74,12);		// N (just minimize external side)

			MapSpecificApply(111,75,	110,76,		112,74,		113,72);	// E (just minimize external side)

			MapSpecificApply(16,54,		11,54,		14,52,		15,50);		// O (unblock marines)
		}

		if (bw->mapHash() == "df21ac8f19f805e1e0d4e9aa9484969528195d9f")	// Jade
		{
			MapSpecificApply(26,15,		24,19,		22,17,		21,15);		// NO (just optimize construction order)
			MapSpecificApply(105,39,	109,40,		110,38,		110,36);	// NE (just minimize external side)
			MapSpecificApply(17,94,		20,101,		18,99,		16,97);		// SO (just minimize external side)
		}

		if (bw->mapHash() == "e47775e171fe3f67cc2946825f00a6993b5a415e")	// La Mancha
		{
			MapSpecificApply(18,38,		17,39,		18,37,		18,35);
			MapSpecificApply(91,19,		92,21,		90,19,		88,17);
			MapSpecificApply(109,90,	107,91,		107,89,		107,87);
			MapSpecificApply(35,109,	36,111,		32,107,		34,109);
		}

		if (bw->mapHash() == "de2ada75fbc741cfa261ee467bf6416b10f9e301")	// Python
		{
			MapSpecificApply(61,6,		64,4,		65,2,		65,0);		// N (just minimize external side)
		}

		if (bw->mapHash() == "86afe0f744865befb15f65d47865f9216edc37e5")	// Python 1.3
		{
			MapSpecificApply(61,6,		64,4,		65,2,		65,0);		// N (just minimize external side)
		}

		if (bw->mapHash() == "b10e73a252d5c693f19829871a01043f0277fd58")	// Pathfinder
		{
			MapSpecificApply(53,32,		58,30,		58,28,		57,26);		// N (marines good side)
			MapSpecificApply(99,67,		90,65,		91,63,		91,61);		// SE (just minimize external side)
		}

		if (bw->mapHash() == "9a4498a896b28d115129624f1c05322f48188fe0")	// Roadrunner
		{
			MapSpecificApply(110,61,	109,62,		108,60,		107,58);	// E (just minimize external side)
			
			MapSpecificApply(14,67,		20,66,		18,64,		16,62);		// O
			
//			MapSpecificApply(81,114,	74,116,		75,114);				// S
			MapSpecificRemove(81,114);										// S
			MapSpecificRemove(72,113);
			MapSpecificRemove(57,115);
		}

		if (bw->mapHash() == "3506e6d942f9721dc99495a141f41c5555e8eab5")	// Great Barrier Reef
		{
			MapSpecificRemove(105,101); // E
		}

		if (bw->mapHash() == "442e456721c94fd085ecd10230542960d57928d9")	// Arcadia
		{
			MapSpecificRemove(107,43); // NE
			MapSpecificRemove(114,39); // NE

			MapSpecificApply(22,46,		18,47,		19,45,		21,43);		// NO (unblock marines)
			MapSpecificApply(14,98,		8,96,		8,94,		8,92);		// SO
		}
	}
}

void Wall::Compute()
{
	if (!Possible()) ExampleWall::Compute(m_innerArea);
	if (!Possible()) ExampleWall::Compute(m_outerArea);
	if (!Possible()) ExampleWall::Compute(nullptr);

	MapSpecific();

	ExampleWall::PostCompute();
}


int Wall::DistanceTo(Position pos) const
{
	TilePosition dimDepot(UnitType(Terran_Supply_Depot).tileSize());
	TilePosition dimBarrack(UnitType(Terran_Barracks).tileSize());
	vector<TilePosition> BuildingDims = {dimBarrack, dimDepot, dimDepot};

	int minDist = numeric_limits<int>::max();

	for (int i = 0 ; i < (int)m_Locations.size() ; ++i)
	{
		TilePosition loc = m_Locations[i];
		TilePosition dim = m_BuildingTypes[i].tileSize();

		minDist = min(minDist, distToRectangle(pos, loc, dim));
	}

	return minDist;
}


bool Wall::UnderWall(Position pos) const
{
	TilePosition dimDepot(UnitType(Terran_Supply_Depot).tileSize());
	TilePosition dimBarrack(UnitType(Terran_Barracks).tileSize());
	vector<TilePosition> BuildingDims = {dimBarrack, dimDepot, dimDepot};

	for (int i = 0 ; i < (int)m_Locations.size() ; ++i)
	{
		TilePosition loc = m_Locations[i];
		TilePosition dim = m_BuildingTypes[i].tileSize();

		if (distToRectangle(pos, loc, dim) == 0)
			return true;
	}

	return false;
}


bool Wall::Completed() const
{
	switch(Size())
	{
	case 1:	return me().CompletedBuildings(Terran_Barracks) >= 1;
	case 2:	return me().CompletedBuildings(Terran_Barracks) >= 1 && me().CompletedBuildings(Terran_Supply_Depot) >= 1;
	case 3:	return me().CompletedBuildings(Terran_Barracks) >= 1 && me().CompletedBuildings(Terran_Supply_Depot) >= 2;
	default: bwem_assert_throw(false);
	}

	return true;
}


bool Wall::Open() const
{
	for (const TilePosition & t : m_Locations)
		if (!ai()->GetVMap().GetBuildingOn(ai()->GetMap().GetTile(t)))
			return true;

	return false;
}


} // namespace iron



