//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef EXPERT_H
#define EXPERT_H

#include <BWAPI.h>
#include "cost.h"
#include "../defs.h"


namespace iron
{

class MyBuilding;
class VBase;

template<tid_t tid>
class My;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Expert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Expert
{
public:
	enum state_t
	{
		unselected,
		selected,
		started
	};

	virtual						~Expert() = default;

	void						UpdatePriority();
	int							Priority() const				{ return m_priority; }
	virtual bool				PriorityLowerThanOtherExperts() const	{ return false; }

	virtual Cost				TaskCost() const				{ return m_taskCost; }
	const Cost &				ReservedCost() const			{ return m_reservedCost; }
	virtual string				ToString() const = 0;

	state_t						State() const					{ return m_state; }

	bool						Unselected() const				{ return m_state == unselected; }
	bool						Started() const					{ return m_state == started; }
	bool						Selected() const				{ return m_state == selected; }
	virtual bool				Ready() const					{ return true; }

	void						Select()						{ assert_throw(m_state == unselected); m_state = selected; m_reservedCost = TaskCost(); }
	void						SetStarted() 					{ assert_throw(m_state == selected); m_state = started; }
	void						Unselect()						{ assert_throw(m_state != unselected); m_state = unselected; }

	virtual void				OnFrame() = 0;

protected:
								Expert(const Cost & taskCost) : m_taskCost(taskCost) {}

	void						OnFinished();

	int							m_priority;

private:
	virtual void				UpdatePriority_specific() = 0;
	Cost						m_taskCost;
	Cost						m_reservedCost;
	state_t						m_state = unselected;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class TrainingExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class TrainingExpert : public Expert
{
public:
								~TrainingExpert();

	void						OnFrame() override;
	string						ToString() const override;

	bool						Ready() const override;

	bool						PriorityLowerThanOtherExperts() const override;

	int							Units() const;

protected:
								TrainingExpert(tid_t tid, MyBuilding * pWhere);

	MyBuilding *				Where() const	{ return m_pWhere; }

private:
	void						UpdatePriority_specific() override final;
	virtual void				UpdateTrainingPriority() = 0;

	tid_t						m_tid;
	MyBuilding *				m_pWhere;
};


template<tid_t tid>
class ExpertInTraining;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ResearchingExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class ResearchingExpert : public Expert
{
public:
	enum research_t {tech, upgrade};

								~ResearchingExpert();

	void						OnFrame() override;
	string						ToString() const override;

	bool						Ready() const override;

	bool						PriorityLowerThanOtherExperts() const override;


protected:
								ResearchingExpert(TechTypes::Enum::Enum val, MyBuilding * pWhere) : m_type(tech), Expert(Cost(TechType(val))), m_techType(val), m_pWhere(pWhere) {}
								ResearchingExpert(UpgradeTypes::Enum::Enum val, MyBuilding * pWhere) : m_type(upgrade), Expert(Cost(UpgradeType(val))), m_upgradeType(val), m_pWhere(pWhere) {}

	MyBuilding *				Where() const	{ return m_pWhere; }

private:
	void						UpdatePriority_specific() override final;
	virtual void				UpdateResearchingPriority() = 0;
	research_t					m_type;
	TechType					m_techType;
	UpgradeType					m_upgradeType;
	frame_t						m_startingFrame;
	MyBuilding *				m_pWhere;
};


template<TechTypes::Enum::Enum techType>
class ExpertInResearching;

template<UpgradeTypes::Enum::Enum upgradeType>
class ExpertInUpgraging;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ConstructingExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class ConstructingExpert : public Expert
{
public:
	void						OnFrame() override;
	string						ToString() const override;
	Cost						TaskCost() const override;

	UnitType					Type() const				{ return m_tid; }

	bool						PriorityLowerThanOtherExperts() const override;

	void						OnConstructionAborted()		{ m_pBuilder = nullptr; OnFinished(); }
	void						OnNoMoreLocation()			{ m_noMoreLocation = true; }
	void						OnBuildingCreated();

	void						OnBuilderChanged(My<Terran_SCV> * pNewBuilder);
	bool						CanChangeBuilder() const	{ return m_builderChanges < 10; }

	int							Buildings() const;
	int							BuildingsCompleted() const;
	int							BuildingsUncompleted() const	{ return Buildings() - BuildingsCompleted(); }
	int							Builders() const;

	virtual TilePosition		GetFreeLocation() const			{ return TilePositions::None; }
	VBase *						GetBase_() const					{ assert_throw(!Free()); return m_pBase_; }
	void						SetBase(VBase * pBase)			{ assert_throw(!Free()); m_pBase_ = pBase; }
	virtual bool				Free() const					{ return false; }

protected:
								ConstructingExpert(tid_t tid) : Expert(Cost(tid)), m_tid(tid) {}


private:
	void						UpdatePriority_specific() override final;
	virtual void				UpdateConstructingPriority() = 0;
	virtual My<Terran_SCV> *	GetFreeBuilder() const			{ return nullptr; }
	VBase *						DefaultBase() const;

	tid_t						m_tid;
//	Cost						m_buildingCost;
	mutable My<Terran_SCV> *	m_pBuilder;
	mutable int					m_builderChanges;
	bool						m_buildingCreated;
	mutable bool				m_noMoreLocation = false;
	mutable VBase *				m_pBase_ = nullptr;
};


template<tid_t tid>
class ExpertInConstructing;

template<tid_t tid>
class ExpertInConstructingFree;


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ConstructingAddonExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class ConstructingAddonExpert : public Expert
{
public:
	void						OnFrame() override;
	string						ToString() const override;

	bool						Ready() const override;

//	void						OnConstructionAborted()		{ m_pBuilder = nullptr; OnFinished(); }
	void						OnBuildingCreated();

	void						OnMainBuildingDestroyed(MyBuilding * pDestroyed);

	UnitType					MainBuildingType() const	{ return UnitType(m_tid).whatBuilds().first; }

protected:
								ConstructingAddonExpert(tid_t tid) : Expert(Cost(tid)), m_tid(tid) {}

private:
	void						UpdatePriority_specific() override final;
	virtual void				UpdateConstructingAddonPriority() = 0;
	void						ConstructionAborted()		{ m_pMainBuilding = nullptr; OnFinished(); }

	tid_t						m_tid;
	mutable MyBuilding *		m_pMainBuilding = nullptr;
	bool						m_buildingCreated;
	frame_t						m_startingFrame;
};


template<tid_t tid>
class ExpertInConstructingAddon;



} // namespace iron


#endif

