//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef MY_H
#define MY_H

#include <BWAPI.h>
#include "bwapiUnits.h"
#include "expert.h"
#include "faceOff.h"
#include "../defs.h"
#include <memory>
#include <array>


namespace iron
{

class Production;
class ConstructingAddonExpert;
class EarlyRunBy;
class Wall;
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MyUnit
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class MyUnit : public MyBWAPIUnit
{
public:
	static unique_ptr<MyUnit>		Make(BWAPI::Unit u);

									~MyUnit();

	void							Update() override;

	const MyUnit *					IsMyUnit() const override			{ return this; }
	MyUnit *						IsMyUnit() override					{ return this; }

	Healing *						GetHealer() const;

	void							StimPack(check_t checkMode = check_t::check);

	const vector<FaceOff> &			FaceOffs() const					{CI(this); return m_FaceOffs; }

	void							OnDangerHere();
	frame_t							LastDangerReport() const			{CI(this); return m_lastDangerReport; }

	bool							AllowedToChase() const;
	void							SetNoChaseFor(frame_t time);

	bool							StayInArea() const					{CI(this); return m_stayInArea; }
	void							SetStayInArea()						{CI(this); m_stayInArea = true; }
	void							UnsetStayInArea()					{CI(this); m_stayInArea = false; }

	frame_t							NoFightUntil() const				{CI(this); return m_noFightUntil; }
	void							SetNoFightUntil(frame_t f)			{CI(this); m_noFightUntil = f; }

	virtual bool					WorthBeingRepaired() const;

protected:
									MyUnit(BWAPI::Unit u, unique_ptr<IBehavior> pBehavior);
	Position						GetRaidingTarget() const;

private:
	vector<FaceOff>					m_FaceOffs;
	frame_t							m_lastDangerReport = 0;
	frame_t							m_noChaseUntil = 0;
	frame_t							m_noFightUntil = 0;
	bool							m_stayInArea = false;
};



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class MyBuilding
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class MyBuilding : public MyBWAPIUnit
{
public:
	static unique_ptr<MyBuilding>		Make(BWAPI::Unit u);

	using BWAPIUnit::JustLifted;
	using BWAPIUnit::JustLanded;

										~MyBuilding();

	void								Update() override;

	const MyBuilding *					IsMyBuilding() const override			{ return this; }
	MyBuilding *						IsMyBuilding() override					{ return this; }

	TilePosition						Size() const							{CI(this); return m_size; }

	void								Train(BWAPI::UnitType type, check_t checkMode = check_t::check);
	void								Research(TechType type, check_t checkMode = check_t::check);
	void								Research(UpgradeType type, check_t checkMode = check_t::check);
	void								BuildAddon(BWAPI::UnitType type, check_t checkMode = check_t::check);
	void								Lift(check_t checkMode = check_t::check);
	bool								Land(TilePosition location, check_t checkMode = check_t::check);
	void								CancelConstruction(check_t checkMode = check_t::check);
	void								CancelTrain(check_t checkMode = check_t::check);
	void								CancelResearch(check_t checkMode = check_t::check);

	void								RecordTrainingExperts(Production & Prod);
	void								RecordResearchingExperts(Production & Prod);

	virtual vector<ConstructingAddonExpert *>	ConstructingAddonExperts() { return {}; }
	virtual ConstructingAddonExpert *	ConstructingThisAddonExpert() { return nullptr; }

	const vector<unique_ptr<ResearchingExpert>> &	ResearchingExperts() const	{CI(this); return m_ResearchingExperts; }

	void								OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * pOther) override;

	int									IdlePosition() const;

	// Returns n in [0..10] that tells how often a unit was trained in this building the last 2000 frames.
	// Training is checked every 100 frames.
	int									Activity() const;

	Wall *								GetWall() const					{CI(this); return m_pWall; }
	int									IndexInWall() const				{CI(this); return m_indexInWall; }

	Constructing *						CurrentBuilder() const;

	frame_t								LiftedSince() const				{CI(this); return m_liftedSince; }

protected:
										MyBuilding(BWAPI::Unit u, unique_ptr<IBehavior> pBehavior);

	template<tid_t tid>
	void								AddTrainingExpert()				{CI(this); m_TrainingExperts.push_back(make_unique<ExpertInTraining<tid>>(this)); }

	template<TechTypes::Enum::Enum val>
	void								AddResearchingExpert()			{CI(this); m_ResearchingExperts.push_back(make_unique<ExpertInResearching<val>>(this)); }

	template<UpgradeTypes::Enum::Enum val>
	void								AddUpgragingExpert()			{CI(this); m_ResearchingExperts.push_back(make_unique<ExpertInUpgraging<val>>(this)); }

	bool								DefaultBehaviorOnFrame_common();

	double								MinLifePercentageToRepair() const override;
	double								MaxLifePercentageToRepair() const override;
	int									MaxRepairers() const override;

private:
	const TilePosition						m_size;
	vector<unique_ptr<TrainingExpert>>		m_TrainingExperts;
	vector<unique_ptr<ResearchingExpert>>	m_ResearchingExperts;
	uint_least32_t							m_activity = 0;
	Wall *									m_pWall = nullptr;
	int										m_indexInWall;
	frame_t									m_liftedSince = 0;
};




template<tid_t> class My;



} // namespace iron


#endif

