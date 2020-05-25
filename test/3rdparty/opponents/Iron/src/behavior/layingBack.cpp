//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "layingBack.h"
#include "kiting.h"
#include "walking.h"
#include "laying.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../territory/vgridMap.h"
#include "../units/army.h"
#include "../units/factory.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


bool LayingBack::Allowed(My<Terran_Vulture> * pVulture)
{
//	return false;

	if (him().IsZerg()) return false;

	if (him().IsProtoss())
	{
		if (me().Bases().size() >= 3) return false;

		if (contains(me().EnlargedAreas(), pVulture->GetArea(check_t::no_check)))
			return false;
			
		if (ai()->GetStrategy()->Active<Walling>() || me().Army().KeepTanksAtHome())
			return true;

		return false;
		
	}

	if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) || me().CompletedUnits(Terran_Goliath))
		return false;

	return true;
}

/*
int LayingBack::Condition(const MyUnit * u, const vector<const FaceOff *> & Threats, int & coverMines)
{
	assert_throw(u->Is(Terran_Vulture));

	if (!Allowed()) return 0;

	int dangerousUnits = 0;
	int hisVultureScore = 0;
	coverMines = 2;
	bool tryCoverMines = false;

	for (const auto * pFaceOff : Threats)
		if (HisUnit * pHisUnit = pFaceOff->His()->IsHisUnit())
			if (!pHisUnit->Type().isWorker())
				switch (pHisUnit->Type())
				{
				case Terran_Vulture:				hisVultureScore += 2;	++dangerousUnits;	coverMines += 1; break;
				case Terran_Marine:					hisVultureScore += 1;						coverMines += 1; break;
				case Terran_Siege_Tank_Tank_Mode:	hisVultureScore += 6;	++dangerousUnits;	coverMines += 1; tryCoverMines = true; break;
				case Terran_Goliath:				hisVultureScore += 4;						coverMines += 1; tryCoverMines = true; break;
				case Protoss_Dragoon:				hisVultureScore += 6;	++dangerousUnits;	coverMines += 1; tryCoverMines = true; break;
				case Protoss_Zealot:				hisVultureScore += 1;						coverMines += 1; break;
				}
	hisVultureScore /= 2;

	if (!tryCoverMines) coverMines = 0;

	if (dangerousUnits == 0) return 0;

	int myVultureScore = 0;
	for (const auto & v : me().Units(Terran_Vulture))
		if (v->Completed())
			if ((v->Life()*3 > v->MaxLife()) || !v->WorthBeingRepaired())
				if (!v->GetBehavior()->IsRetraiting())
					if (dist(u->Pos(), v->Pos()) < 12*32)
						++myVultureScore;

	if (myVultureScore <= hisVultureScore) return hisVultureScore + 1;

	return 0;
}
*/

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class LayingBack
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<LayingBack *> LayingBack::m_Instances;

LayingBack::LayingBack(My<Terran_Vulture> * pAgent, Position target)
	: Behavior(pAgent, behavior_t::LayingBack), m_target(target)
{
	assert_throw(Agent()->RemainingMines() >= 2);
	assert_throw(pathExists(pAgent->Pos(), target));

	m_pTargetArea = ai()->GetMap().GetNearestArea(WalkPosition(Target()));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

///	bw << pAgent->NameWithId() << " layingBack" << endl;
///	ai()->SetDelay(50);
}


LayingBack::~LayingBack()
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


void LayingBack::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string LayingBack::StateName() const
{CI(this);
	switch(State())
	{
	case layingBack:
	{
		string s = "layingBack";
		if (m_layingCoverMinesTime) s += " (laying in = " + to_string(m_layingCoverMinesTime - ai()->Frame()) + ")";
		s += ", time = " + to_string(ai()->Frame() - InStateSince()) + ")";
		return s;
	}
	default:				return "?";
	}
}

/*
bool LayingBack::TryCounterNow()
{CI(this);
	if (me().HasResearched(TechTypes::Spider_Mines))
		if (*m_pCoverMines > 0)
			if (Agent()->IsMy<Terran_Vulture>())
			{
				int minDistToTargetInGroup = numeric_limits<int>::max();
				const LayingBack * pMostAdvancedCandidate = nullptr;;

				int minesAvailableInGroup = 0;
				for (LayingBack * r : *m_pGroup)
				{
					if (const My<Terran_Vulture> * pVulture = r->Agent()->IsMy<Terran_Vulture>())
//						if (pVulture->RemainingMines() >= 2)
						if (pVulture->RemainingMines() >= 1)
						{
							if (!r->Agent()->FaceOffs().empty()) r->m_layingCoverMinesTime = 0;
							else if (m_layingCoverMinesTime == 0)
							{
								m_layingCoverMinesTime = ai()->Frame() + 50;
							///	ai()->SetDelay(1000);
							///	bw << Agent()->NameWithId() << "will lay cover mines in " << m_layingCoverMinesTime - ai()->Frame() << endl;
							}

							minesAvailableInGroup += pVulture->RemainingMines();

							int d = groundDist(r->Agent()->Pos(), Target());
							if (d < minDistToTargetInGroup)
							{
								minDistToTargetInGroup = d;
								pMostAdvancedCandidate = r;
							}
						}
						else r->m_layingCoverMinesTime = 0;

				}

//				if (minesAvailableInGroup >= *m_pCoverMines)
					if (pMostAdvancedCandidate == this)
						if (Agent()->FaceOffs().empty())
							if (ai()->Frame() >= m_layingCoverMinesTime)
							{
								const int remainingMines = Agent()->IsMy<Terran_Vulture>()->RemainingMines();
								if (remainingMines >= 2) *m_pCoverMines -= remainingMines;
								m_releaseGroupWhenLeaving = false;
								Agent()->ChangeBehavior<Laying>(Agent());
								return true;
							}
			}

	return false;
}
*/

void LayingBack::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), m_target, GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	assert_throw(Agent()->RemainingMines() > 0);

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		return SetSubBehavior<Walking>(Agent(), Target(), __FILE__ + to_string(__LINE__));
	}
	
	if (ai()->Frame() - m_inStateSince > 60)
		if (Agent()->Life() < Agent()->PrevLife(10))
			return Agent()->ChangeBehavior<Kiting>(Agent());

	if ((dist(Agent()->Pos(), Target()) < 32*5) && (Agent()->GetArea(check_t::no_check) == TargetArea()))
//		||		(m_layingCoverMinesTime == 0) && (ai()->Frame() - m_inStateSince > 400) ||
//		(m_layingCoverMinesTime == 0) && (ai()->Frame() - m_inStateSince > 200) && m_growed)
		{
			ResetSubBehavior();
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}


	if (him().IsProtoss())
		if (contains(me().EnlargedAreas(), Agent()->GetArea(check_t::no_check)))
		{
			ResetSubBehavior();
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}


	if (!Agent()->FaceOffs().empty()) m_layingCoverMinesTime = 0;
	
	if (m_layingCoverMinesTime == 0)
	{
		m_layingCoverMinesTime = ai()->Frame() + 50;
	///	ai()->SetDelay(1000);
	///	bw << Agent()->NameWithId() << "will lay cover mines in " << m_layingCoverMinesTime - ai()->Frame() << endl;
	}

	if (ai()->Frame() - m_inStateSince > 15)
	{
		if (!GetSubBehavior())
			if (ai()->Frame() - Agent()->LastFrameMoving() > 10)
				return SetSubBehavior<Walking>(Agent(), Target(), __FILE__ + to_string(__LINE__));

		if (ai()->Frame() - Agent()->LastFrameMoving() > 50)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	if (me().HasResearched(TechTypes::Spider_Mines))
		if (ai()->Frame() >= m_layingCoverMinesTime)
		{
			if (contains(me().EnlargedAreas(), Agent()->GetArea(check_t::no_check)))
				return;

			int spacing = 8*32;
			for (const auto & u : me().Units(Terran_Vulture_Spider_Mine))
				if (dist(u->Pos(), Agent()->Pos()) < spacing)
					return;

			for (Laying * pLayer : Laying::Instances())
				if (dist(pLayer->Agent()->Pos(), Agent()->Pos()) < spacing)
					return;

			return Agent()->ChangeBehavior<Laying>(Agent());
		}
}



} // namespace iron



