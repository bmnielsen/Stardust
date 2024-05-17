//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "dropping.h"
#include "defaultBehavior.h"
#include "Dropping1T1V.h"
#include "airSniping.h"
#include "../units/cc.h"
#include "../units/starport.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Dropping
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Dropping *> Dropping::m_Instances;

Dropping::Dropping(MyUnit * pAgent)
	: Behavior(pAgent, behavior_t::Dropping)
{

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

///	ai()->SetDelay(100);
}


Dropping::~Dropping()
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


void Dropping::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


int Dropping::MinLifeUntilLoad() const
{
	if (Agent()->Is(Terran_SCV)) return 11;
	if (Agent()->Is(Terran_Vulture)) return 23;
	return 30;
}


MyUnit * Dropping::FindDamagedUnit() const
{
	map<int, MyUnit *> Candidates;


	for (Dropping1T1V * pDropping1T1V : Dropping1T1V::Instances())
		if (dist(pDropping1T1V->Agent()->Pos(), Agent()->Pos()) < 8*32)
			if (int damage = pDropping1T1V->Agent()->MaxLife() - pDropping1T1V->Agent()->Life())
				Candidates.emplace(damage, pDropping1T1V->Agent());

	for (Dropping * pOther : Dropping::Instances())
		if (pOther != this)
			if (!pOther->Agent()->Loaded())
				if (dist(pOther->Agent()->Pos(), Agent()->Pos()) < 8*32)
					if (int damage = pOther->Agent()->MaxLife() - pOther->Agent()->Life())
						Candidates.emplace(damage, pOther->Agent());

	return Candidates.empty() ? nullptr : Candidates.rbegin()->second;
}


void Dropping::OnFrame_v()
{CI(this);
	if (!Agent()->CanAcceptCommand()) return;

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}

	m_pRepairedUnit = nullptr;

	if (!Agent()->Loaded())
	{
		bool safePlace = false;
		for (Dropping1T1V * pDropping1T1V : Dropping1T1V::Instances())
			if (dist(pDropping1T1V->Agent()->Pos(), Agent()->Pos()) < 6*32)
				if (pDropping1T1V->SafePlace())
					safePlace = true;

		if (Agent()->Life() <= MinLifeUntilLoad())
			if (!safePlace)
			{
				for (Dropping1T1V * pDropping1T1V : Dropping1T1V::Instances())
					if (dist(pDropping1T1V->Agent()->Pos(), Agent()->Pos()) < 6*32)
						if (pDropping1T1V->Agent()->CanAcceptCommand())
						{
						///	bw << Agent()->NameWithId() << " too much damaged --> LOAD" << endl;
							return pDropping1T1V->ThisDropship()->Load(Agent());
						}

				return;
			}


		if (My<Terran_SCV> * pSCV = Agent()->IsMy<Terran_SCV>())
			if (MyUnit * pDamagedUnit = FindDamagedUnit())
			{
				m_pRepairedUnit = pDamagedUnit;
				if (!pSCV->Unit()->isRepairing() || (ai()->Frame() % 50 == 0))
				{
				///	bw << pSCV->NameWithId() << " repairing " << pDamagedUnit->NameWithId() << " (" << pDamagedUnit->MaxLife() - pDamagedUnit->Life() << ")" << endl;
				///	bw->drawLineMap(pSCV->Pos(), pDamagedUnit->Pos(), Colors::Yellow);
					if (ai()->Frame() % 50 == 0)
						if (m_pRepairedUnit->Is(Terran_SCV))
							if (m_pRepairedUnit->Life() == m_pRepairedUnit->PrevLife(10))
								if (m_pRepairedUnit->CanAcceptCommand())
									m_pRepairedUnit->Stop();

					return pSCV->Repair(pDamagedUnit);
				}
			}


		if (!Agent()->Is(Terran_SCV))
		{
			if (Agent()->Is(Terran_Vulture))
			{
				HisUnit * pTarget = nullptr;
				int minDist = 3*32;
				for (const FaceOff & fo : Agent()->FaceOffs())
					if (fo.His()->IsHisUnit())
						if (fo.DistanceToMyRange() < minDist)
						{
							minDist = fo.DistanceToMyRange();
							pTarget = fo.His()->IsHisUnit();
						}

				if (pTarget) { Agent()->Attack(pTarget); return; }
			}


			Agent()->HoldPosition();
		}

	}


//	if (My<Terran_Dropship> * pDropship = AirSniping::FindDropship(Agent(), 15*32))
//	return Agent()->ChangeBehavior<AirSniping>(Agent(), pDropship);

}



} // namespace iron



