//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BWAPI_UNITS_H
#define BWAPI_UNITS_H

#include <BWAPI.h>
#include "../behavior/behaviorType.h"
#include "../behavior/defaultBehavior.h"
#include "../defs.h"
#include "../vect.h"
#include "../utils.h"
#include "../debug.h"
#include <memory>


namespace iron
{

class IBehavior;
class Stronghold;
class MyBWAPIUnit;
class MyUnit;
class MyBuilding;
class HisBWAPIUnit;
class HisUnit;
class HisBuilding;
class Chasing;
class VChasing;
class Repairing;

template<tid_t> class My;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class BWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class BWAPIUnit : public CheckableInstance
{
public:
	enum {sizePrevLife = 64};
	enum {sizePrevPos = 64};

									BWAPIUnit(BWAPI::Unit u);
	virtual							~BWAPIUnit();

	virtual string					NameWithId() const			{CI(this); return Type().getName() + " #" + to_string(Unit()->getID()); }

	virtual void					Update();

	// Returns the BWAPI::Unit this BWAPIUnit is wrapping around.
	BWAPI::Unit						Unit() const				{CI(this); return m_bwapiUnit; }

	// Returns the BWAPI::UnitType of the BWAPI::Unit this BWAPIUnit is wrapping around.
	BWAPI::UnitType					Type() const				{CI(this); return m_bwapiType; }

	zone_t							Zone() const				{CI(this); return zone(Type()); }

	const Area *					GetArea(check_t checkMode = check_t::check) const		{CI(this); assert_throw((checkMode == check_t::no_check) || m_pArea); return m_pArea; }
	const Area *					FindArea() const;

	bool							Burrowed() const			{CI(this); return m_burrowed; }
	bool							Loaded() const				{CI(this); return m_loaded; }
	bool							Flying() const				{CI(this); return m_flying; }
	bool							Hovering() const			{CI(this); return m_hovering; }
	bool							CanMove() const;
	bool							MovingCloserTo(Position pos) const;
	bool							MovingAwayFrom(Position pos) const;

	bool							Completed() const			{CI(this); return m_completed; }
	frame_t							RemainingBuildTime() const	{CI(this); return m_remainingBuildTime; }
	frame_t							TimeToTrain() const			{CI(this); return m_timeToTrain; }
	frame_t							TimeToResearch() const		{CI(this); return m_timeToResearch; }

	// Returns the center of this BWAPIUnit, in pixels (same as Unit()->getPosition()).
	Position						Pos() const					{CI(this); return m_PrevPos[0]; }
	Position						PrevPos(int i) const		{CI(this); assert_throw((0 <= i) && (i < sizePrevPos)); return m_PrevPos[i]; }

	int								GetLeft() const				{CI(this); return Pos().x - Type().dimensionLeft(); }
	int								GetRight() const			{CI(this); return Pos().x + Type().dimensionRight(); }
	int								GetTop() const				{CI(this); return Pos().y - Type().dimensionUp(); }
	int								GetBottom() const			{CI(this); return Pos().y + Type().dimensionDown(); }

	int								GetDistanceToTarget(const BWAPIUnit * pTarget) const;
	int								GetDistanceToTarget(Position targetPos, UnitType targetType) const;

	const Vect &					Acceleration() const		{CI(this); return m_acceleration; }

	frame_t							LastFrameMoving() const		{CI(this); return m_lastFrameMoving; }
	frame_t							LastFrameMoved() const		{CI(this); return m_lastFrameMoved; }

	// Returns the topLeft tile of this BWAPIUnit (same as Unit()->getTilePosition()).
	BWAPI::TilePosition				TopLeft() const				{CI(this); return m_topLeft; }

	int								LifeWithShields() const		{CI(this); return Life() + Shields(); }
	int								MaxLifeWithShields() const	{CI(this); return MaxLife() + MaxShields(); }

	int								Life() const				{CI(this); return m_PrevLife[0]; }
	int								MaxLife() const				{CI(this); return m_maxLife; }
	int								PrevLife(int i) const		{CI(this); assert_throw((0 <= i) && (i < sizePrevLife)); return m_PrevLife[i]; }
	int								PrevDamage() const			{CI(this); return m_prevDamage; }
	float							DeltaLifePerFrame() const	{CI(this); return m_deltaLifePerFrame; }

	int								Shields() const				{CI(this); return m_shields; }
	int								MaxShields() const			{CI(this); return m_maxShields; }

	// Number of frames to recover 1 pt of life / shields
	// 0 means no regeneration
	frame_t							RegenerationTime() const	{CI(this); return m_regenerationTime; }

	int								Energy() const				{CI(this); return m_energy; }
	int								MaxEnergy() const			{CI(this); return m_maxEnergy; }

	int								GroundAttack() const		{CI(this); return m_groundAttack; }
	int								AirAttack() const			{CI(this); return m_airAttack; }
	bool							CanAttack() const			{CI(this); return m_groundAttack + m_airAttack > 0; }
	bool							ThreatBuilding() const		{CI(this); return Type().isBuilding() && (CanAttack() || Is(Terran_Bunker)); }
	bool							GroundThreatBuilding() const{CI(this); return Type().isBuilding() && (GroundAttack() || Is(Terran_Bunker)); }
	bool							AirThreatBuilding() const	{CI(this); return Type().isBuilding() && (AirAttack() || Is(Terran_Bunker)); }
	bool							DetectorBuilding() const	{CI(this); return Type().isBuilding() && (Type().isDetector()); }

	int								GroundRange() const			{CI(this); return m_groundRange; }
	int								AirRange() const			{CI(this); return m_airRange; }
	int								Range(zone_t zone) const	{CI(this); return zone == zone_t::air ? m_airRange : m_groundRange; }

	int								Armor() const				{CI(this); return m_armor; }

	int								CoolDown() const			{CI(this); return m_coolDown; }
	int								PrevCoolDown() const		{CI(this); return m_prevCoolDown; }
	int								AvgCoolDown() const			{CI(this); return m_avgCoolDown; }
	frame_t							NotFiringFor() const;

	int								Sight() const				{CI(this); return m_sight; }
	double							Speed() const				{CI(this); return m_speed; }

	// The Chasing units that are targeting this.
	const vector<Chasing *> &		Chasers() const				{CI(this); return m_Chasers; }
	void							AddChaser(Chasing * pChaser);
	void							RemoveChaser(Chasing * pChaser);
	frame_t							FramesToBeKilledByChasers() const;

	// The VChasing units that are targeting this.
	const vector<VChasing *> &		VChasers() const			{CI(this); return m_VChasers; }
	void							AddVChaser(VChasing * pVChaser);
	void							RemoveVChaser(VChasing * pVChaser);
	frame_t							FramesToBeKilledByVChasers() const;

	void							SetSiegeModeType()			{CI(this); assert_throw(m_bwapiType = Terran_Siege_Tank_Tank_Mode); m_bwapiType = Terran_Siege_Tank_Siege_Mode; }
	void							SetTankModeType()			{CI(this); assert_throw(m_bwapiType = Terran_Siege_Tank_Siege_Mode); m_bwapiType = Terran_Siege_Tank_Tank_Mode; }


	bool							Is(BWAPI::UnitType type) const	{CI(this); return Type() == type; }

	virtual const MyBWAPIUnit *		IsMyBWAPIUnit() const		{ return nullptr; }
	virtual MyBWAPIUnit *			IsMyBWAPIUnit()				{ return nullptr; }

	virtual const MyUnit *			IsMyUnit() const			{ return nullptr; }
	virtual MyUnit *				IsMyUnit()					{ return nullptr; }

	virtual const MyBuilding *		IsMyBuilding() const		{ return nullptr; }
	virtual MyBuilding *			IsMyBuilding()				{ return nullptr; }

	virtual const HisBWAPIUnit *	IsHisBWAPIUnit() const		{ return nullptr; }
	virtual HisBWAPIUnit *			IsHisBWAPIUnit()			{ return nullptr; }

	virtual const HisUnit *			IsHisUnit() const			{ return nullptr; }
	virtual HisUnit *				IsHisUnit()					{ return nullptr; }

	virtual const HisBuilding *		IsHisBuilding() const		{ return nullptr; }
	virtual HisBuilding *			IsHisBuilding()				{ return nullptr; }

	BWAPIUnit &						operator=(const BWAPIUnit &) = delete;

protected:
	void							PutBuildingOnTiles();
	void							RemoveBuildingFromTiles();

	bool							JustLifted() const			{CI(this); return m_justLifted; }
	bool							JustLanded() const			{CI(this); return m_justLanded; }

private:
	frame_t							CalculateRemainingBuildTime() const;

	const BWAPI::Unit				m_bwapiUnit;
	BWAPI::UnitType					m_bwapiType;
	const bool						m_hovering;

	bool							m_burrowed;
	bool							m_loaded;
	bool							m_flying;
	bool							m_justLifted;
	bool							m_justLanded;
	bool							m_completed;
	frame_t							m_remainingBuildTime;
	frame_t							m_timeToTrain;
	frame_t							m_timeToResearch;
	TilePosition					m_topLeft;
	int								m_life;
	const int						m_maxLife;
	int								m_shields;
	const int						m_maxShields;
	const frame_t					m_regenerationTime;
	int								m_energy;
	int								m_maxEnergy;
	int								m_groundAttack;
	int								m_airAttack;
	int								m_groundRange;
	int								m_airRange;
	int								m_coolDown;
	int								m_prevCoolDown;
	int								m_avgCoolDown;
	int								m_armor;
	int								m_sight;
	double							m_speed;
	frame_t							m_lastFrameMoving;
	frame_t							m_lastFrameMoved;
	Vect							m_acceleration;
	const Area *					m_pArea;
	vector<Chasing *>				m_Chasers;
	vector<VChasing *>				m_VChasers;

	int								m_PrevLife[sizePrevLife];
	int								m_prevDamage;
	float							m_deltaLifePerFrame;
	frame_t							m_lastTimeCoolDownNotZero;

	Position						m_PrevPos[sizePrevPos];
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MyBWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class MyBWAPIUnit : public BWAPIUnit
{
public:
									MyBWAPIUnit(BWAPI::Unit u, unique_ptr<IBehavior> pBehavior);
	virtual							~MyBWAPIUnit();

	const MyBWAPIUnit *				IsMyBWAPIUnit() const override		{ return this; }
	MyBWAPIUnit *					IsMyBWAPIUnit() override			{ return this; }

	virtual void					Update();

	template<tid_t tid> My<tid> *		As()		{ assert_throw(Type() == tid); return static_cast<My<tid> *>(this); }
	template<tid_t tid> const My<tid> *	As() const	{ assert_throw(Type() == tid); return static_cast<const My<tid> *>(this); }
	template<tid_t tid> My<tid> *		IsMy()		{ return (Type() == tid) ? static_cast<My<tid> *>(this) : nullptr; }
	template<tid_t tid> const My<tid> *	IsMy() const{ return (Type() == tid) ? static_cast<const My<tid> *>(this) : nullptr; }

	bool							HasBehavior() const					{CI(this); return m_pBehavior.get() != nullptr; }
	IBehavior *						GetBehavior() const;
	behavior_t						GetLastBehaviorType() const			{CI(this); return m_lastBehaviorType; }
	void							SetLastBehaviorType(behavior_t t)	{CI(this); m_lastBehaviorType = t; }

	template<class TBehavior, class... Args>
	void							ChangeBehavior(Args&&... args)
									{CI(this);
										m_pBehavior.reset();
										try
										{
											m_pBehavior = make_unique<TBehavior>(std::forward<Args>(args)...);
											#if DEV
												bw->drawCircleMap(Pos(), Type().width(), m_pBehavior->GetColor(), bool("isSolid"));
											#endif
										}
										catch (const ::Exception &)
										{
											m_pBehavior = make_unique<DefaultBehavior>(this);
											throw;
										}
										catch (const std::exception &)
										{
											m_pBehavior = make_unique<DefaultBehavior>(this);
											throw;
										}
										catch (...)
										{
											m_pBehavior = make_unique<DefaultBehavior>(this);
											throw;
										}
										assert_throw(m_pBehavior.get());
									}

	virtual void					DefaultBehaviorOnFrame() {}


	void							OnOtherBWAPIUnitDestroyed(BWAPIUnit * other);

	Stronghold *					GetStronghold() const				{CI(this); return m_pStronghold; }
	void							InitializeStronghold();
	void							EnterStronghold(Stronghold * sh);
	void							LeaveStronghold();

	int								DistToHisStartingBase() const		{CI(this); return m_distToHisStartingBase; }

	void							RightClick(BWAPIUnit * u, check_t checkMode = check_t::check);
	void							Move(Position pos, check_t checkMode = check_t::check);
	void							Patrol(Position pos, check_t checkMode = check_t::check);
	bool							Attack(BWAPIUnit * u, check_t checkMode = check_t::check);
	void							Stop(check_t checkMode = check_t::check);
	void							HoldPosition(check_t checkMode = check_t::check);

	virtual double					MinLifePercentageToRepair() const	{ return 0.999; }
	virtual double					MaxLifePercentageToRepair() const	{ return 1.0; }
	virtual int						MaxRepairers() const				{ return 1; }
	vector<Repairing *>				Repairers() const;
	int								RepairersCount() const				{ return (int)Repairers().size(); }

	bool							CanAcceptCommand() const;

protected:
	void							OnCommandSent(check_t checkMode, bool result, const string & message) const;
	void							OnSpellSent(check_t checkMode, bool result, const string & message) const;

private:
	bool							LastCommandExecuted() const;
	virtual void					OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * )	{}

	void							SetStronghold(Stronghold * sh);

	unique_ptr<IBehavior>			m_pBehavior;
	Stronghold *					m_pStronghold = nullptr;
	int								m_distToHisStartingBase;
	behavior_t						m_lastBehaviorType = behavior_t::none;
	mutable frame_t					m_lastCommandExecutionFrame = -1;
	mutable string					m_previousCommand;
};




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class HisBWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class HisBWAPIUnit : public BWAPIUnit
{
public:
									HisBWAPIUnit(BWAPI::Unit u);

	const HisBWAPIUnit *			IsHisBWAPIUnit() const override	{ return this; }
	HisBWAPIUnit *					IsHisBWAPIUnit() override		{ return this; }

	void							Update() override;

	bool							InFog() const					{CI(this); return m_inFog; }
	void							SetInFog();

	frame_t							LastFrameNoVisibleTile() const	{CI(this); assert_throw(m_inFog); return m_lastFrameNoVisibleTile; }

protected:
	void							UpdatedLastFrameNoVisibleTile();

private:
	bool							m_inFog = false;

	frame_t							m_lastFrameNoVisibleTile;
};




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class NeutralBWAPIUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class NeutralBWAPIUnit : public BWAPIUnit
{
public:
									NeutralBWAPIUnit(BWAPI::Unit u) : BWAPIUnit(u) {}

private:
};





} // namespace iron


#endif

