//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef BEHAVIOR_H
#define BEHAVIOR_H

#include <BWAPI.h>
#include "behaviorType.h"
#include "../defs.h"
#include "../utils.h"
#include "../debug.h"


namespace iron
{

class BWAPIUnit;
class MyBWAPIUnit;
class HisUnit;
class DefaultBehavior;
class Executing;
class Walking;
class Mining;
class Refining;
class Supplementing;
class Scouting;
class PatrollingBases;
class Exploring;
class Guarding;
class Razing;
class Harassing;
class Raiding;
class Fleeing;
class Laying;
class KillingMine;
class Chasing;
class VChasing;
class Destroying;
class Kiting;
class Fighting;
class Retraiting;
class LayingBack;
class Repairing;
class Healing;
class Constructing;
class Sieging;
class Blocking;
class Sniping;
class AirSniping;
class Checking;
class Collecting;
class Dropping;
class Dropping1T;
class Dropping1T1V;


bool canWalkOnTile(const TilePosition & t);


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class IBehavior
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class IBehavior : public CheckableInstance
{
public:
	virtual							~IBehavior();

	virtual string					Name() const = 0;
	virtual string					StateName() const = 0;
	virtual string					FullName() const				{CI(this); return Name() + ": " + StateName(); }

	virtual BWAPI::Color			GetColor() const = 0;
	virtual Text::Enum				GetTextColor() const = 0;

	behavior_t						GetBehaviorType() const			{CI(this); return m_behaviorType; }

	virtual const DefaultBehavior *	IsDefaultBehavior() const		{ return nullptr; }
	virtual DefaultBehavior *		IsDefaultBehavior()				{ return nullptr; }

	virtual const Walking *			IsWalking() const				{ return nullptr; }
	virtual Walking *				IsWalking()						{ return nullptr; }

	virtual const Executing *		IsExecuting() const				{ return nullptr; }
	virtual Executing *				IsExecuting()					{ return nullptr; }

	virtual const Mining *			IsMining() const				{ return nullptr; }
	virtual Mining *				IsMining()						{ return nullptr; }

	virtual const Refining *		IsRefining() const				{ return nullptr; }
	virtual Refining *				IsRefining()					{ return nullptr; }

	virtual const Supplementing *	IsSupplementing() const			{ return nullptr; }
	virtual Supplementing *			IsSupplementing()				{ return nullptr; }

	virtual const Scouting *		IsScouting() const				{ return nullptr; }
	virtual Scouting *				IsScouting()					{ return nullptr; }

	virtual const PatrollingBases *	IsPatrollingBases() const		{ return nullptr; }
	virtual PatrollingBases *		IsPatrollingBases()				{ return nullptr; }

	virtual const Exploring *		IsExploring() const				{ return nullptr; }
	virtual Exploring *				IsExploring()					{ return nullptr; }

	virtual const Guarding *		IsGuarding() const				{ return nullptr; }
	virtual Guarding *				IsGuarding()					{ return nullptr; }

	virtual const Razing *			IsRazing() const				{ return nullptr; }
	virtual Razing *				IsRazing()						{ return nullptr; }

	virtual const Harassing *		IsHarassing() const				{ return nullptr; }
	virtual Harassing *				IsHarassing()					{ return nullptr; }

	virtual const Raiding *			IsRaiding() const				{ return nullptr; }
	virtual Raiding *				IsRaiding()						{ return nullptr; }

	virtual const Fleeing *			IsFleeing() const				{ return nullptr; }
	virtual Fleeing *				IsFleeing()						{ return nullptr; }
	
	virtual const Laying *			IsLaying() const				{ return nullptr; }
	virtual Laying *				IsLaying()						{ return nullptr; }
	
	virtual const KillingMine *		IsKillingMine() const			{ return nullptr; }
	virtual KillingMine *			IsKillingMine()					{ return nullptr; }
	
	virtual const Chasing *			IsChasing() const				{ return nullptr; }
	virtual Chasing *				IsChasing()						{ return nullptr; }
	
	virtual const VChasing *		IsVChasing() const				{ return nullptr; }
	virtual VChasing *				IsVChasing()					{ return nullptr; }
	
	virtual const Destroying *		IsDestroying() const			{ return nullptr; }
	virtual Destroying *			IsDestroying()					{ return nullptr; }
	
	virtual const Kiting *			IsKiting() const				{ return nullptr; }
	virtual Kiting *				IsKiting()						{ return nullptr; }
	
	virtual const Fighting *		IsFighting() const				{ return nullptr; }
	virtual Fighting *				IsFighting()					{ return nullptr; }
	
	virtual const Retraiting *		IsRetraiting() const			{ return nullptr; }
	virtual Retraiting *			IsRetraiting()					{ return nullptr; }
	
	virtual const LayingBack *		IsLayingBack() const			{ return nullptr; }
	virtual LayingBack *			IsLayingBack()					{ return nullptr; }
	
	virtual const Repairing *		IsRepairing() const				{ return nullptr; }
	virtual Repairing *				IsRepairing()					{ return nullptr; }
	
	virtual const Healing *			IsHealing() const				{ return nullptr; }
	virtual Healing *				IsHealing()						{ return nullptr; }
	
	virtual const Constructing *	IsConstructing() const			{ return nullptr; }
	virtual Constructing *			IsConstructing()				{ return nullptr; }
	
	virtual const Sieging *			IsSieging() const				{ return nullptr; }
	virtual Sieging *				IsSieging()						{ return nullptr; }
	
	virtual const Blocking *		IsBlocking() const				{ return nullptr; }
	virtual Blocking *				IsBlocking()					{ return nullptr; }
	
	virtual const Sniping *			IsSniping() const				{ return nullptr; }
	virtual Sniping *				IsSniping()						{ return nullptr; }
	
	virtual const AirSniping *		IsAirSniping() const			{ return nullptr; }
	virtual AirSniping *			IsAirSniping()					{ return nullptr; }
	
	virtual const Checking *		IsChecking() const				{ return nullptr; }
	virtual Checking *				IsChecking()					{ return nullptr; }
	
	virtual const Collecting *		IsCollecting() const			{ return nullptr; }
	virtual Collecting *			IsCollecting()					{ return nullptr; }
	
	virtual const Dropping *		IsDropping() const				{ return nullptr; }
	virtual Dropping *				IsDropping()					{ return nullptr; }
	
	virtual const Dropping1T *		IsDropping1T() const			{ return nullptr; }
	virtual Dropping1T *			IsDropping1T()					{ return nullptr; }
	
	virtual const Dropping1T1V *	IsDropping1T1V() const			{ return nullptr; }
	virtual Dropping1T1V *			IsDropping1T1V()				{ return nullptr; }
	
	virtual bool					CanRepair(const MyBWAPIUnit * pTarget, int distance) const = 0;
	virtual bool					CanChase(const HisUnit * pTarget) const = 0;

	IBehavior *						GetSubBehavior() const			{CI(this); return m_pSubBehavior.get(); }

	void							OnOtherBWAPIUnitDestroyed(BWAPIUnit * other)	{CI(this); if (m_pSubBehavior) m_pSubBehavior->OnOtherBWAPIUnitDestroyed(other); OnOtherBWAPIUnitDestroyed_v(other); }

	MyBWAPIUnit *					Agent() const;

	void							OnFrame()						{CI(this); if (m_pSubBehavior) m_pSubBehavior->OnFrame(); OnFrame_v(); }
	void							OnAgentBeingDestroyed()			{CI(this); if (m_pSubBehavior) m_pSubBehavior->OnAgentBeingDestroyed(); m_agentBeingDestroyed = true; }

	frame_t							Since() const					{CI(this); return m_since; }

	IBehavior &						operator=(const IBehavior &) = delete;

protected:
									IBehavior(MyBWAPIUnit * pAgent, behavior_t behaviorType);

	bool							JustArrivedInState() const		{CI(this); bool result = m_justArrivedInState; m_justArrivedInState = false; return result; }
	void							OnStateChanged() const			{CI(this); m_justArrivedInState = true; }


	template<class TBehavior, class... Args>
	void							SetSubBehavior(Args&&... args)	{CI(this); assert_throw(!m_pSubBehavior); m_pSubBehavior = make_unique<TBehavior>(std::forward<Args>(args)...); }

	void							ResetSubBehavior();

	bool							AgentBeingDestroyed() const		{CI(this); return m_agentBeingDestroyed; }

	virtual void					OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * )	{}

	bool							CheckStuckBuilding();
	frame_t							GoAwayFromStuckingBuildingUntil() const		{CI(this); return m_goAwayFromStuckingBuildingUntil; }

private:
	virtual void					OnFrame_v() = 0;

	behavior_t						m_behaviorType;
	unique_ptr<IBehavior>			m_pSubBehavior;
	MyBWAPIUnit *					m_pAgent;
	mutable bool					m_justArrivedInState;
	bool							m_agentBeingDestroyed = false;
	frame_t							m_goAwayFromStuckingBuildingUntil = 0;
	frame_t							m_since;
};


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Behavior
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<class TAgent>
class Behavior : public IBehavior
{
public:
	TAgent *						Agent() const	{CI(this); return static_cast<TAgent *>(IBehavior::Agent()); }

protected:
									Behavior(TAgent * pAgent, behavior_t behaviorType) : IBehavior(pAgent, behaviorType) {}


private:
};





} // namespace iron


#endif

