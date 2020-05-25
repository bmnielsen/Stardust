//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "bwapiUnits.h"
#include "../behavior/behavior.h"
#include "../behavior/chasing.h"
#include "../behavior/Vchasing.h"
#include "../behavior/repairing.h"
#include "../behavior/constructing.h"
#include "../territory/vgridMap.h"
#include "../territory/stronghold.h"
#include "../strategy/strategy.h"
#include "../strategy/freeTurrets.h"
#include "../strategy/expand.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class BWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

BWAPIUnit::BWAPIUnit(BWAPI::Unit u)
	: m_bwapiUnit(u), m_bwapiType(u->getType()),
	m_hovering((m_bwapiType == Terran_SCV) || (m_bwapiType == Terran_Vulture) ||
			   (m_bwapiType == Protoss_Probe) || (m_bwapiType == Protoss_Archon) || (m_bwapiType == Protoss_Dark_Archon) ||
			   (m_bwapiType == Zerg_Drone)),
	m_maxLife(u->getType().maxHitPoints()),
	m_maxShields(u->getType().maxShields()),
	m_regenerationTime(u->getType().getRace() == Races::Zerg ? 64 : u->getType().getRace() == Races::Protoss ? 37 : 0),
	m_prevDamage(0),
	m_completed(u->isCompleted()),
	m_remainingBuildTime(0),
	m_lastTimeCoolDownNotZero(0),
	m_timeToTrain(0),
	m_timeToResearch(0)
	
{
	m_burrowed = Unit()->isBurrowed();
	m_loaded = Unit()->isLoaded();
	m_flying = Unit()->isFlying();
	m_topLeft = Unit()->getTilePosition();
	fill(m_PrevPos, m_PrevPos + sizePrevPos, Unit()->getPosition());
	fill(m_PrevLife, m_PrevLife + sizePrevLife, Unit()->getHitPoints());
	m_remainingBuildTime = CalculateRemainingBuildTime();
	m_pArea = Flying() || m_loaded ? nullptr : ai()->GetMap().GetNearestArea(WalkPosition(Pos()));
}


BWAPIUnit::~BWAPIUnit()
{
}


frame_t BWAPIUnit::CalculateRemainingBuildTime() const
{CI(this);
	if (Unit()->getPlayer() == me().Player())
		return Unit()->getRemainingBuildTime();

	if (Completed())
		return 0;

	if ((Type().getRace() != Races::Zerg) ||
		Is(Zerg_Creep_Colony) || Is(Zerg_Spire) || Is(Zerg_Extractor) ||
		Is(Zerg_Spawning_Pool) || Is(Zerg_Hydralisk_Den) || Is(Zerg_Queens_Nest) ||
		Is(Zerg_Ultralisk_Cavern) || Is(Zerg_Nydus_Canal) || Is(Zerg_Defiler_Mound))
		return Type().buildTime() * (MaxLifeWithShields() - LifeWithShields()) / MaxLifeWithShields();

	return Type().buildTime() / 2;
}


void BWAPIUnit::Update()
{CI(this);
	m_burrowed = Unit()->isBurrowed();
	m_loaded = Unit()->isLoaded();

	m_justLifted = m_justLanded = false;
	if (m_flying != Unit()->isFlying())
	{
		if (m_flying)	m_justLanded = true;
		else			m_justLifted = true;

		m_flying = Unit()->isFlying();
	}

	m_topLeft = Unit()->getTilePosition();

	m_acceleration = Vect(Unit()->getVelocityX(), Unit()->getVelocityY());

	{
		for (int i = sizePrevPos-1 ; i > 0 ; --i)
			m_PrevPos[i] = m_PrevPos[i-1];
	
		m_PrevPos[0] = Unit()->getPosition();
	}

	{
		int life64framesAgo = m_PrevLife[sizePrevLife-1];
		for (int i = sizePrevLife-1 ; i > 0 ; --i)
			m_PrevLife[i] = m_PrevLife[i-1];
	
		m_PrevLife[0] = Unit()->getHitPoints();
		if (Unit()->isDefenseMatrixed()) m_PrevLife[0] += 250;

		int delta1 = m_PrevLife[0] - m_PrevLife[1];
		if (delta1 < 0) m_prevDamage = -delta1;

		int delta64 = m_PrevLife[0] - life64framesAgo;
		m_deltaLifePerFrame = float(delta64 / 64.0);
	}
	
	m_shields = Unit()->getShields();
	m_prevCoolDown = m_coolDown;
	m_coolDown = Unit()->getGroundWeaponCooldown();
	if (m_coolDown) m_lastTimeCoolDownNotZero = ai()->Frame();
	m_avgCoolDown = avgCoolDown(Type(), Unit()->getPlayer());
	m_sight = Unit()->getPlayer()->sightRange(Type());
	m_speed = Unit()->getPlayer()->topSpeed(Type());
	m_groundAttack = groundAttack(Type(), Unit()->getPlayer());
	m_airAttack = airAttack(Type(), Unit()->getPlayer());
	m_groundRange = groundRange(Type(), Unit()->getPlayer());
	m_airRange = airRange(Type(), Unit()->getPlayer());
	m_armor = armor(Type(), Unit()->getPlayer());
	m_maxEnergy = Unit()->getPlayer()->maxEnergy(Type());
	m_energy = Unit()->getEnergy();

	m_completed = Unit()->isCompleted();
	m_remainingBuildTime = CalculateRemainingBuildTime();
	m_timeToTrain = Unit()->getRemainingTrainTime();
	m_timeToResearch = Unit()->isResearching() ? Unit()->getRemainingResearchTime() : Unit()->getRemainingUpgradeTime();

	if (Unit()->isMoving()) m_lastFrameMoving = ai()->Frame();
	if (m_PrevPos[0] != m_PrevPos[1]) m_lastFrameMoved = ai()->Frame();

	m_pArea = Flying() || Loaded() ? ai()->GetMap().GetArea(WalkPosition(Pos())) : ai()->GetMap().GetNearestArea(WalkPosition(Pos()));
}



void BWAPIUnit::PutBuildingOnTiles()
{CI(this);
	assert_throw(Type().isBuilding());

	for (int dy = 0 ; dy < Type().tileSize().y ; ++dy)
	for (int dx = 0 ; dx < Type().tileSize().x ; ++dx)
	{
		CHECK_POS(TopLeft() + TilePosition(dx, dy));
		auto & tile = ai()->GetMap().GetTile(TopLeft() + TilePosition(dx, dy));
		assert_throw_plus(!Type().isRefinery() == !tile.GetNeutral(), Type().getName() + " " + my_to_string(TopLeft()) + my_to_string(TilePosition(dx, dy)) + " " + bw->mapName());
		assert_throw(!ai()->GetVMap().GetBuildingOn(tile));
		ai()->GetVMap().SetBuildingOn(tile, this);
		ai()->GetVMap().SetNearBuilding(tile);
	}


	vector<TilePosition> Border = outerBorder(TopLeft(), Type().tileSize(), bool("noCorner"));
	for (auto t : Border)
		if (ai()->GetMap().Valid(t))
			ai()->GetVMap().SetNearBuilding(ai()->GetMap().GetTile(t, check_t::no_check));


	if (Unit()->getPlayer() == me().Player())
		if (Type().canBuildAddon())
			for (int dy = 0 ; dy < Type().tileSize().y ; ++dy)
			for (int dx = 0 ; dx < 2 ; ++dx)
			{
				CHECK_POS(TopLeft() + TilePosition(Type().tileSize().x + dx, dy));
				auto & tile = ai()->GetMap().GetTile(TopLeft() + TilePosition(Type().tileSize().x + dx, dy));
				ai()->GetVMap().SetAddonRoom(tile);
			}
}


void BWAPIUnit::RemoveBuildingFromTiles()
{CI(this);
	assert_throw(Type().isBuilding());

	for (int dy = 0 ; dy < Type().tileSize().y ; ++dy)
	for (int dx = 0 ; dx < Type().tileSize().x ; ++dx)
	{
		CHECK_POS(TopLeft() + TilePosition(dx, dy));
		auto & tile = ai()->GetMap().GetTile(TopLeft() + TilePosition(dx, dy));
		assert_throw(ai()->GetVMap().GetBuildingOn(tile) == this);
		ai()->GetVMap().SetBuildingOn(tile, nullptr);
		ai()->GetVMap().UnsetNearBuilding(tile);
	}


	vector<TilePosition> Border = outerBorder(TopLeft(), Type().tileSize(), bool("noCorner"));
	for (auto t : Border)
		if (ai()->GetMap().Valid(t))
			ai()->GetVMap().UnsetNearBuilding(ai()->GetMap().GetTile(t, check_t::no_check));


	if (Unit()->getPlayer() == me().Player())
		if (Type().canBuildAddon())
			for (int dy = 0 ; dy < Type().tileSize().y ; ++dy)
			for (int dx = 0 ; dx < 2 ; ++dx)
			{
				CHECK_POS(TopLeft() + TilePosition(Type().tileSize().x + dx, dy));
				auto & tile = ai()->GetMap().GetTile(TopLeft() + TilePosition(Type().tileSize().x + dx, dy));
				ai()->GetVMap().UnsetAddonRoom(tile);
			}
}


frame_t BWAPIUnit::NotFiringFor() const
{CI(this);
	return ai()->Frame() - m_lastTimeCoolDownNotZero;
}


const Area * BWAPIUnit::FindArea() const
{CI(this);
	if (m_pArea) return m_pArea;
	
	return ai()->GetMap().GetNearestArea(WalkPosition(Pos()));
}


void BWAPIUnit::AddChaser(Chasing * pChaser)
{CI(this);
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Chasers, pChaser);
}


void BWAPIUnit::RemoveChaser(Chasing * pChaser)
{CI(this);
	assert_throw(contains(m_Chasers, pChaser));
	really_remove(m_Chasers, pChaser);
}


frame_t BWAPIUnit::FramesToBeKilledByChasers() const
{CI(this);
	double rate = 0;
	for (Chasing * pChaser : m_Chasers)
		rate += 1.0 / max(1, pChaser->GetFaceOff().FramesToKillHim());

	return frame_t(0.5 + 1.0/rate);
}


void BWAPIUnit::AddVChaser(VChasing * pVChaser)
{CI(this);
	PUSH_BACK_UNCONTAINED_ELEMENT(m_VChasers, pVChaser);
}


void BWAPIUnit::RemoveVChaser(VChasing * pVChaser)
{CI(this);
	assert_throw(contains(m_VChasers, pVChaser));
	really_remove(m_VChasers, pVChaser);
}


frame_t BWAPIUnit::FramesToBeKilledByVChasers() const
{CI(this);
	double rate = 0;
	for (VChasing * pVChaser : m_VChasers)
		rate += 1.0 / max(1, pVChaser->GetFaceOff().FramesToKillHim());

	return abs(rate) < 0.000001 ? 1000000 : frame_t(0.5 + 1.0/rate);
}


// see UnitInterface::getDistance
int BWAPIUnit::GetDistanceToTarget(const BWAPIUnit * pTarget) const
{CI(this);
	/////// Compute distance

	// retrieve left/top/right/bottom values for calculations
	int left, right, top, bottom;
	left    = pTarget->GetLeft() - 1;
	top     = pTarget->GetTop() - 1;
	right   = pTarget->GetRight() + 1;
	bottom  = pTarget->GetBottom() + 1;

	// compute x distance
	int xDist = GetLeft() - right;
	if ( xDist < 0 )
	{
		xDist = left - GetRight();
		if ( xDist < 0 )
			xDist = 0;
	}

	// compute y distance
	int yDist = GetTop() - bottom;
	if ( yDist < 0 )
	{
		yDist = top - GetBottom();
		if ( yDist < 0 )
			yDist = 0;
	}

	// compute actual distance
	return Positions::Origin.getApproxDistance(Position(xDist, yDist));
}


// see UnitInterface::getDistance
int BWAPIUnit::GetDistanceToTarget(Position targetPos, UnitType targetType) const
{CI(this);
	/////// Compute distance

	// retrieve left/top/right/bottom values for calculations
	int left, right, top, bottom;
	left    = targetPos.x - targetType.dimensionLeft() - 1;
	top     = targetPos.y - targetType.dimensionUp() - 1;
	right   = targetPos.x + targetType.dimensionRight() + 1;
	bottom  = targetPos.y + targetType.dimensionDown() + 1;

	// compute x distance
	int xDist = GetLeft() - right;
	if ( xDist < 0 )
	{
		xDist = left - GetRight();
		if ( xDist < 0 )
			xDist = 0;
	}

	// compute y distance
	int yDist = GetTop() - bottom;
	if ( yDist < 0 )
	{
		yDist = top - GetBottom();
		if ( yDist < 0 )
			yDist = 0;
	}

	// compute actual distance
	return Positions::Origin.getApproxDistance(Position(xDist, yDist));
}


bool BWAPIUnit::CanMove() const
{CI(this);
	if (Type().isBuilding() || Is(Terran_Siege_Tank_Siege_Mode) || Is(Terran_Vulture_Spider_Mine)) return false;
	
	if (IsHisUnit() && IsHisUnit()->InFog()) return true;

	if (Unit()->isBurrowed()) return false;
	if (Unit()->isLockedDown()) return false;
	if (Unit()->isMaelstrommed()) return false;
	if (Unit()->isStasised()) return false;

	return true;
}


bool BWAPIUnit::MovingCloserTo(Position pos) const
{CI(this);
	return squaredDist(Pos(), pos) < squaredDist(PrevPos(1), pos);
}


bool BWAPIUnit::MovingAwayFrom(Position pos) const
{CI(this);
	return squaredDist(Pos(), pos) > squaredDist(PrevPos(1), pos);
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MyBWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


MyBWAPIUnit::MyBWAPIUnit(BWAPI::Unit u, unique_ptr<IBehavior> pBehavior)
  : BWAPIUnit(u),
	m_pBehavior(move(pBehavior))
{
	assert_throw(m_pBehavior.get());
}


MyBWAPIUnit::~MyBWAPIUnit()
{
#if !DEV
	try //3
#endif
	{
		if (m_pBehavior) m_pBehavior->OnAgentBeingDestroyed();
	}
#if !DEV
	catch(...){} //3
#endif
}


IBehavior * MyBWAPIUnit::GetBehavior() const
{CI(this);
	return CI(m_pBehavior.get());
}


void MyBWAPIUnit::InitializeStronghold()
{CI(this);
	if (ai()->Frame() == 0)
		SetStronghold(me().StartingVBase()->GetStronghold());
	else
	{
		if (IsMyBuilding())
		{
			if (Is(Terran_Command_Center))
			{
				for (VBase * base : me().Bases())
					if (!base->GetCC())
						if (base->GetArea()->BWEMPart() == GetArea(check_t::no_check))
							return SetStronghold(base->GetStronghold());

				bool LiftCC = false;
				if (auto s = ai()->GetStrategy()->Active<Expand>())
					if (s->LiftCC())
						LiftCC = true;

				assert_throw(LiftCC);
			}

			if (Type().isAddon())
			{
				CHECK_POS(TopLeft() - 1);
				if (BWAPIUnit * b = ai()->GetVMap().GetBuildingOn(ai()->GetMap().GetTile(TopLeft() - 1)))
					if (b->IsMyBuilding())
						return SetStronghold(b->IsMyBuilding()->GetStronghold());
			}

			My<Terran_SCV> * pBuilder = nullptr;
			for (Constructing * c : Constructing::Instances())
				if (c->Agent()->Unit() == Unit()->getBuildUnit())
					pBuilder = c->Agent();

			if (pBuilder)
			{
				if (pBuilder->GetStronghold())
					return SetStronghold(pBuilder->GetStronghold());
			}
			else
			{
				// TODO
				// assert_throw_plus(false, NameWithId());

				for (VBase * base : me().Bases())
					if (contains(base->GetArea()->EnlargedArea(), GetArea(check_t::no_check)))
					{
						SetStronghold(base->GetStronghold());
						return;
					}
			}
		}
		else
		{
			if (!Is(Terran_Vulture_Spider_Mine))
			{
				for (MyBuilding * b : ai()->GetGridMap().GetCell(TilePosition(Pos())).MyBuildings)
					if (b->Pos() == Pos())
					{
						SetStronghold(b->GetStronghold());
						return;
					}

				assert_throw(false);
			}
		}
	}
}


void MyBWAPIUnit::EnterStronghold(Stronghold * sh)
{CI(this);
	assert_throw(sh);
	SetStronghold(sh);
}


void MyBWAPIUnit::LeaveStronghold()
{CI(this);
	SetStronghold(nullptr);
}


void MyBWAPIUnit::Update()
{CI(this);
	BWAPIUnit::Update();

	if (him().StartingBase())
	{
		if (Flying() || Loaded())
			m_distToHisStartingBase = roundedDist(Pos(), him().StartingBase()->Center());
		else
		{
			ai()->GetMap().GetPath(Pos(), him().StartingBase()->Center(), &m_distToHisStartingBase);
			if (m_distToHisStartingBase < 0) m_distToHisStartingBase = 1000000;
		}
	}
}


void MyBWAPIUnit::OnOtherBWAPIUnitDestroyed(BWAPIUnit * other)
{CI(this);
	GetBehavior()->OnOtherBWAPIUnitDestroyed(other);
	OnOtherBWAPIUnitDestroyed_v(other);
}


void MyBWAPIUnit::SetStronghold(Stronghold * sh)
{CI(this);
	assert_throw_plus(!sh != !m_pStronghold, NameWithId() + (sh ? " a" : " b"));
	
	if (sh)
	{
		m_pStronghold = sh;
		if (MyUnit * u = IsMyUnit()) m_pStronghold->OnUnitIn(u);
		else                         m_pStronghold->OnBuildingIn(IsMyBuilding());
	}
	else
	{
		if (MyUnit * u = IsMyUnit()) m_pStronghold->OnUnitOut(u);
		else                         m_pStronghold->OnBuildingOut(IsMyBuilding());
		m_pStronghold = sh;
	}
}


vector<Repairing *> MyBWAPIUnit::Repairers() const
{CI(this);

	vector<Repairing *> List;
	for (Repairing * r : Repairing::Instances())
		if (r->TargetX() == this)
			List.push_back(r);
	return List;
}

/*
0	1	2	3	4	5	6	7	8	9	10	11	12
T-----------------------------------------------t
					d-----------------------D
					M-------------------m
*/

void MyBWAPIUnit::OnCommandSent(check_t checkMode, bool result, const string & message) const
{CI(this);
	assert_throw_plus(LastCommandExecuted(), message + "(last = " + m_previousCommand + ")");
	m_previousCommand = message;
//	if (GetBehavior() && GetBehavior()->IsBlocking())
//	bw << ai()->Frame() << ") " << message << endl;

	if (result) m_lastCommandExecutionFrame = ai()->Frame() + bw->getRemainingLatencyFrames();
	if (!result && (checkMode == check_t::check)) reportCommandError(message);
}


void MyBWAPIUnit::OnSpellSent(check_t checkMode, bool result, const string & message) const
{CI(this);
	assert_throw_plus(LastCommandExecuted(), message + "(last = " + m_previousCommand + ")");
	m_previousCommand = message;
//	if (GetBehavior() && GetBehavior()->IsBlocking())
//	bw << ai()->Frame() << ") " << message << endl;

	if (result) m_lastCommandExecutionFrame = ai()->Frame() + 5+2*bw->getRemainingLatencyFrames();
	if (!result && (checkMode == check_t::check)) reportCommandError(message);
}


bool MyBWAPIUnit::LastCommandExecuted() const
{CI(this);
	return ai()->Frame() > m_lastCommandExecutionFrame;
}


bool MyBWAPIUnit::CanAcceptCommand() const
{CI(this);
	if (!LastCommandExecuted()) return false;
	if (!Unit()->isInterruptible()) return false;

	return true;
}




void MyBWAPIUnit::RightClick(BWAPIUnit * u, check_t checkMode)
{CI(this);
	bool result = Unit()->rightClick(u->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " rightClicks " + u->NameWithId());
}


void MyBWAPIUnit::Move(Position pos, check_t checkMode)
{CI(this);
//	if (GetBehavior()->IsAttacking()) 
//	bw << NameWithId() << " move!" << endl;
	bool result = Unit()->move(pos);
	OnCommandSent(checkMode, result, NameWithId() + " move to " + my_to_string(pos));
}


void MyBWAPIUnit::Patrol(Position pos, check_t checkMode)
{CI(this);
	bool result = Unit()->patrol(pos);
	OnCommandSent(checkMode, result, NameWithId() + " Patrols " + my_to_string(pos));
}


bool MyBWAPIUnit::Attack(BWAPIUnit * u, check_t checkMode)
{CI(this);
//	if (GetBehavior()->IsAttacking()) 
//	bw << NameWithId() << " attack! " << u->NameWithId() << endl;
	bool result = Unit()->attack(u->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " attack " + u->NameWithId());
	return result;
}


void MyBWAPIUnit::Stop(check_t checkMode)
{CI(this);
//	bw << NameWithId() << " stop! " << endl;
	bool result = Unit()->stop();
	OnCommandSent(checkMode, result, NameWithId() + " " + (m_pBehavior ? GetBehavior()->Name() : string("no_behavior")) + " stop ");
}


void MyBWAPIUnit::HoldPosition(check_t checkMode)
{CI(this);
//	bw << NameWithId() << " holdPosition! " << endl;
	bool result = Unit()->holdPosition();
	OnCommandSent(checkMode, result, NameWithId() + " holdPosition ");
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisBWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


HisBWAPIUnit::HisBWAPIUnit(BWAPI::Unit u)
	: BWAPIUnit(u)
{
	if (auto * pFreeTurrets = ai()->GetStrategy()->Has<FreeTurrets>())
	{
		switch (Type())
		{
		case Terran_Siege_Tank_Tank_Mode:
		case Terran_Siege_Tank_Siege_Mode:
		case Terran_Ghost:
		case Terran_Science_Vessel:
		case Terran_Wraith:
		case Terran_Valkyrie:
		case Terran_Starport:
		case Protoss_Carrier:
		case Protoss_Arbiter:
		case Protoss_Scout:
		case Protoss_Observer:
		case Protoss_Shuttle:
		case Protoss_Corsair:
		case Protoss_Dark_Templar:
		case Protoss_Stargate:
		case Zerg_Guardian:
		case Zerg_Mutalisk:
		case Zerg_Overlord:
		case Zerg_Queen:
		case Zerg_Scourge:
		case Zerg_Devourer:
		case Zerg_Spire:
			pFreeTurrets->SetNeedTurrets(); break;
		}
	}

	switch (Type())
	{
	case Protoss_Dark_Templar:
	case Protoss_Citadel_of_Adun:
	case Protoss_Templar_Archives:
		him().SetMayDarkTemplar();
	};

	switch (Type())
	{
	case Zerg_Hydralisk:
	case Zerg_Hydralisk_Den:
	case Zerg_Lurker:
	case Zerg_Lurker_Egg:
		him().SetMayHydraOrLurker();
	};

	switch (Type())
	{
	case Zerg_Spire:
	case Zerg_Lair:
	case Zerg_Mutalisk:
		him().SetMayMuta();
	};

	switch (Type())
	{
	case Protoss_Reaver:
	case Protoss_Robotics_Facility:
	case Protoss_Robotics_Support_Bay:
	case Protoss_Shuttle:
		him().SetMayReaver();
	};

	switch (Type())
	{
	case Protoss_Robotics_Facility:
	case Protoss_Observer:
	case Protoss_Shuttle:
		him().SetMayShuttleOrObserver();
	};

	switch (Type())
	{
	case Protoss_Carrier:
	case Protoss_Fleet_Beacon:
	case Protoss_Stargate:
		him().SetMayCarrier();
	};

	switch (Type())
	{
	case Protoss_Dragoon:
	case Protoss_Cybernetics_Core:
		him().SetMayDragoon();
	};

	switch (Type())
	{
	case Protoss_Photon_Cannon:
		him().SetHasCannons();
	};
	
	switch (Type())
	{
	case Terran_Wraith:
	case Terran_Starport:
		him().SetMayWraith();
	};
}

void HisBWAPIUnit::Update()
{CI(this);
	BWAPIUnit::Update();

}


void HisBWAPIUnit::SetInFog()
{CI(this);
	assert_throw(!InFog());
	
	m_inFog = true;
	UpdatedLastFrameNoVisibleTile();

	if (HisBuilding * b = IsHisBuilding())
	{
		if (!Flying()) RemoveBuildingFromTiles();
		ai()->GetGridMap().Remove(b);
	}
	else
		ai()->GetGridMap().Remove(IsHisUnit());
}


void HisBWAPIUnit::UpdatedLastFrameNoVisibleTile()
{CI(this);
	m_lastFrameNoVisibleTile = ai()->Frame();
}

	

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class NeutralBWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////



	
} // namespace iron



