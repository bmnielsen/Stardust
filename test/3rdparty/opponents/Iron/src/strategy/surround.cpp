//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "surround.h"
#include "strategy.h"
#include "baseDefense.h"
#include "berserker.h"
#include "../units/fightSim.h"
#include "../units/factory.h"
#include "../units/cc.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/fighting.h"
#include "../behavior/raiding.h"
#include "../behavior/layingBack.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{




class Surround::EnemyGroup
{
public:
	static void						ClearFighters()				{ m_Fighters.clear(); }
	static const vector<My<Terran_Vulture> *> &	Fighters()					{ return m_Fighters; }

									EnemyGroup(const HisKnownUnit * u) : m_Members(1, u) {}

	bool							ShouldAdd(const HisKnownUnit * u) const;
	void							Add(const HisKnownUnit * u)	{ push_back_assert_does_not_contain(m_Members, u); }

	void							AddFightingCandidate(My<Terran_Vulture> * u)
									{
										push_back_assert_does_not_contain(m_FightingCandidates, u);
										push_back_assert_does_not_contain(EnemyGroup::m_Fighters, u);
									}

	void							Eval();
	void							Process() const;

	const vector<const HisKnownUnit *> &	Members() const		{ return m_Members; }
	const vector<My<Terran_Vulture> *> &	FightingCandidates() const	{ return m_FightingCandidates;}

	// Tells if there is at least one visible unit (in m_zone) in this group
	bool							Visible() const				{ return m_visible; }

	bool							KiteOnly() const			{ return m_kiteOnly; }

	Position						Pos() const					{ return m_position; }

	int								Size() const				{ return (int)m_Members.size(); }

	// lowest value = highest priority
	int								Priority() const			{ return m_priority; }

//	int								Power() const				{ return m_power; }

	void							ComputeStats();

	void							Draw(BWAPI::Color col, Text::Enum textCol) const;


private:
									EnemyGroup() = delete;

	int								RemainingMines() const;
	bool							StartFightCondition() const;
	bool							LeaveFightCondition() const;
	int								DistanceToNearestVisibleMember(const My<Terran_Vulture> * u) const;
	void							ComputePosition();

	static vector<My<Terran_Vulture> *>			m_Fighters;

	vector<const HisKnownUnit *>	m_Members;
	vector<const HisKnownUnit *>	m_Dragoons;
	Position						m_position;
	VBase *							m_pMyBaseNearby;
	int								m_distToMyBase;
//	int								m_power;
	int								m_priority;
	bool							m_visible;
	bool							m_kiteOnly;
	bool							m_groundThreatBuildingNearby;
	bool							m_evalSuccess;
	vector<My<Terran_Vulture> *>	m_FightingCandidates;
};


vector<My<Terran_Vulture> *> Surround::EnemyGroup::m_Fighters;

bool Surround::EnemyGroup::ShouldAdd(const HisKnownUnit * u) const
{
	assert_throw(!contains(m_Members, u));

	int baseMaxTileDist = 6;
	if (u->Type() == Protoss_Zealot) baseMaxTileDist += 4;

	// There is at least one member in m_zone (Cf. the invariant in the constructor).
	for (const HisKnownUnit * m : m_Members)
	{
		int maxTileDist = baseMaxTileDist;
		if (m->Type() == Protoss_Zealot) maxTileDist += 4;
		
		int d = groundDist(m->LastPosition(), u->LastPosition());
		if (d > 32*maxTileDist) return false;
	}

	return true;
}


void Surround::EnemyGroup::ComputePosition()
{
	m_position = {0, 0};
	{
		int n = 0;
		for (const HisKnownUnit * m : m_Members)
		{
			m_position += m->LastPosition();
			++n;
		}
		m_position /= n;
	}

	if (!ai()->GetMap().GetArea(WalkPosition(m_position)))
		for (const HisKnownUnit * m : m_Members)
			if (ai()->GetMap().GetArea(WalkPosition(m->LastPosition())))
			{
				m_position = m->LastPosition();
				break;
			}
}


void Surround::EnemyGroup::ComputeStats()
{
	assert_throw(Size() > 0);

	ComputePosition();

	int zealotsHere = 0;
	
	m_visible = false;

	for (const auto * info : m_Members)
	{
		if (info->GetHisUnit()) m_visible = true;

		switch (info->Type())
		{
		case Protoss_Zealot:				++zealotsHere;					break;
		case Protoss_Dragoon:				m_Dragoons.push_back(info);		break;
		}
	}

	m_groundThreatBuildingNearby = any_of(him().Buildings().begin(), him().Buildings().end(), [this](const unique_ptr<HisBuilding> & b)
											{ return b->GroundThreatBuilding() && dist(b->Pos(), Pos()) < 10*32; });

	m_kiteOnly = m_groundThreatBuildingNearby;

	m_pMyBaseNearby = findMyClosestBase(m_position);
	m_distToMyBase = groundDist(m_position, m_pMyBaseNearby ? m_pMyBaseNearby->BWEMPart()->Center() : me().StartingBase()->Center());

	m_priority = m_pMyBaseNearby ? 0 : m_distToMyBase / (32*8);
//	m_priority -= m_power;
}


void Surround::EnemyGroup::Draw(BWAPI::Color col, Text::Enum textCol) const
{
#if DEV
	assert_throw(Size() > 0);

	Position topLeft     = {numeric_limits<int>::max(), numeric_limits<int>::max()};
	Position bottomRight = {numeric_limits<int>::min(), numeric_limits<int>::min()};
	for (const HisKnownUnit * m : m_Members)
	{
		makeBoundingBoxIncludePoint(topLeft, bottomRight, m->LastPosition());
		makeBoundingBoxIncludePoint(topLeft, bottomRight, m->LastPosition());
	}

	topLeft = topLeft - 30;
	bottomRight = bottomRight + 30;
	Position center = (topLeft + bottomRight) / 2;
/*

	bw->drawBoxMap(topLeft, bottomRight, col);
	bw->drawTextMap(topLeft + Position(2, -12), "%c[%d] (%d) %s", textCol, m_fightSimResult, m_fightSimEfficiency, m_fightSimFinalState.c_str());
*/
	bw->drawEllipseMap(center, center.x - topLeft.x, center.y - topLeft.y, col);


	for (const HisKnownUnit * m1 : m_Members)
	for (const HisKnownUnit * m2 : m_Members)
		if (m1 != m2)
			drawLineMap(m1->LastPosition(), m2->LastPosition(), col);

	for (const My<Terran_Vulture> * u : m_FightingCandidates)
		drawLineMap(u->Pos(), center, col);

#endif
}


int Surround::EnemyGroup::DistanceToNearestVisibleMember(const My<Terran_Vulture> * u) const
{
	int dist = numeric_limits<int>::max();
	for (const HisKnownUnit * m : m_Members)
		if (m->GetHisUnit())
			dist = min(dist, roundedDist(u->Pos(), m->LastPosition()));

	assert_throw(dist != numeric_limits<int>::max());
	return dist;
}


int Surround::EnemyGroup::RemainingMines() const
{
	int n = 0;
	for (My<Terran_Vulture> * u : FightingCandidates())
		n += u->RemainingMines();
	return n;
}


void Surround::EnemyGroup::Eval()
{
	assert_throw(Visible());
	assert_throw(Size() > 0);
	assert_throw(FightingCandidates().size() > 0);

	m_evalSuccess = RemainingMines() > 2 * (int)m_Dragoons.size();
}


bool Surround::EnemyGroup::StartFightCondition() const
{
	return m_evalSuccess;
}


bool Surround::EnemyGroup::LeaveFightCondition() const
{
	return !m_evalSuccess;
}


void Surround::EnemyGroup::Process() const
{
	assert_throw(Visible());
	assert_throw(Size() > 0);
	assert_throw(FightingCandidates().size() > 0);

/*
	if (StartFightCondition())
	{
		MyUnit * pFarestFightingCandidate = nullptr;
		int distFarestFightingCandidate = numeric_limits<int>::min();

		MyUnit * pFarestNonFightingCandidate = nullptr;
		int distFarestNonFightingCandidate = numeric_limits<int>::min();

		bool firstShootOccured = false;
		for (MyUnit * u : m_FightingCandidates)
		{
			const int d = zoneDist(Pos(), u->Pos(), most<zone_t::air>(m_zone, u->Zone()));

			if (u->GetBehavior()->IsFighting() && (u->GetBehavior()->IsFighting()->Zone() == m_zone))
			{
				if (u->GetBehavior()->IsFighting()->Shooted()) firstShootOccured = true;

				if (d > distFarestFightingCandidate)
				{
					distFarestFightingCandidate = d;
					pFarestFightingCandidate = u;
				}
			}
			else if (u->CanMove() && !(u->Is(Terran_Siege_Tank_Siege_Mode) || u->GetBehavior()->IsSieging()))
				if (!u->GetBehavior()->IsLaying())
				if (!u->GetBehavior()->IsDestroying())
				{
					if (d > distFarestNonFightingCandidate)
					{
						distFarestNonFightingCandidate = d;
						pFarestNonFightingCandidate = u;
					}
				}
		}

		if (pFarestFightingCandidate)
		{
			for (MyUnit * u : m_FightingCandidates)
				if (!(u->GetBehavior()->IsFighting() && (u->GetBehavior()->IsFighting()->Zone() == m_zone)))
					if (u->CanMove() && !(u->Is(Terran_Siege_Tank_Siege_Mode) || u->GetBehavior()->IsSieging()))
						if (!u->GetBehavior()->IsLaying())
						{
							const int d = zoneDist(Pos(), u->Pos(), most<zone_t::air>(m_zone, u->Zone()));

							if (firstShootOccured || (d > distFarestFightingCandidate - 2*32))
								u->ChangeBehavior<Fighting>(u, Pos(), m_zone);
						}
		}
		else
		{
			if (pFarestNonFightingCandidate)
				pFarestNonFightingCandidate->ChangeBehavior<Fighting>(pFarestNonFightingCandidate, Pos(), m_zone);
		}
	}
	else if (LeaveFightCondition())
	{
		for (MyUnit * u : m_FightingCandidates)
			if (u->GetBehavior()->IsFighting() && (u->GetBehavior()->IsFighting()->Zone() == m_zone))
				u->ChangeBehavior<DefaultBehavior>(u);
	}
*/
}





//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Surround
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Surround::Surround()
{
}


Surround::~Surround()
{
}


string Surround::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


void Surround::OnFrame_v()
{
	if (!him().IsProtoss()) return;
	if (!me().HasResearched(TechTypes::Spider_Mines)) return;

//	bw << ai()->Frame() << ") process " << Name() << endl;

	EnemyGroup::ClearFighters();

	vector<const HisKnownUnit *> Enemies;
	for (const auto & info : him().AllUnits())
		if (!info.second.NoMoreHere3Tiles())
			if (ShouldWatchEnemy(info.second.Type()))
				Enemies.push_back(&info.second);

	const Position myBasePos = me().StartingBase()->Center();
	sort(Enemies.begin(), Enemies.end(), [myBasePos](const HisKnownUnit * a, const HisKnownUnit * b)
			{
//				if (!a->Type().isFlyer() && b->Type().isFlyer()) return Zone() == zone_t::ground;
//				if (a->Type().isFlyer() && !b->Type().isFlyer()) return Zone() != zone_t::ground;

				if (a->GetHisUnit() && !b->GetHisUnit()) return true;
				if (!a->GetHisUnit() && b->GetHisUnit()) return false;

				return roundedDist(a->LastPosition(), myBasePos) < roundedDist(b->LastPosition(), myBasePos);
			});

	vector<EnemyGroup> EnemyGroups;
	for (const HisKnownUnit * u : Enemies)
	{
		bool merged = false;
		for (EnemyGroup & Group : EnemyGroups)
			if (Group.ShouldAdd(u))
			{
				Group.Add(u);
				merged = true;
				break;
			}
		if (merged) continue;

		if (u->Type() == Protoss_Dragoon)	// so enemy groups contain at least one Dragoon
			EnemyGroups.emplace_back(u);
	}

	for (EnemyGroup & Group : EnemyGroups)
		Group.ComputeStats();

	sort(EnemyGroups.begin(), EnemyGroups.end(), [](const EnemyGroup & a, const EnemyGroup & b)
													{ return a.Priority() < b.Priority(); });

//	if (VisibleEnemyGroups.empty()) return;

	// Assigns candidates to nearest visible EnemyGroup

	vector<My<Terran_Vulture> *> Candidates = FindCandidates();
	for (My<Terran_Vulture> * cand : Candidates)
	{
		EnemyGroup *	pNearestVisibleEnemyGroup = nullptr;


		int minDist = 32*15;

		for (EnemyGroup & Group : EnemyGroups)
			if (Group.Visible())
			{
				int d = groundDist(cand->Pos(), Group.Pos());
				if (d < minDist)
				{
					minDist = d;
					pNearestVisibleEnemyGroup = &Group;
				}
			}

		if (pNearestVisibleEnemyGroup)
			pNearestVisibleEnemyGroup->AddFightingCandidate(cand);
	}

	for (EnemyGroup & Group : EnemyGroups)
		if (Group.Visible())
			if (!Group.FightingCandidates().empty())
			{
				Group.Eval();
				Group.Process();
			}

	for (EnemyGroup & Group : EnemyGroups)
		Group.Draw(GetColor(), GetTextColor());

/*
	// Finally, release all Fighting units that were not processed through an EnemyGroup:
	vector<Fighting *> Fighters = Fighting::Instances();
	for (Fighting * pFighter : Fighters)
		if (!pFighter->ProtectTank())
			if (pFighter->Zone() == Zone())
				if (!contains(EnemyGroup::Fighters(), pFighter->Agent()))
					pFighter->Agent()->ChangeBehavior<DefaultBehavior>(pFighter->Agent());
*/
}


bool Surround::ShouldWatchEnemy(BWAPI::UnitType type) const
{
	if (!type.isWorker())
		switch (type)
		{
//		case Protoss_Archon:
//		case Protoss_Dark_Archon:
//		case Protoss_Dark_Templar:
		case Protoss_Dragoon:
//		case Protoss_High_Templar:
//		case Protoss_Reaver:
		case Protoss_Zealot:
//		case Protoss_Carrier:
//		case Protoss_Interceptor:
//		case Protoss_Arbiter:
//		case Protoss_Scout:
//		case Protoss_Corsair:
			return true;
		}

	return false;
}


vector<My<Terran_Vulture> *> Surround::FindCandidates() const
{
	vector<My<Terran_Vulture> *> FightingCandidates;

	for (const auto & u : me().Units(Terran_Vulture))
		if (u->Completed())
		if (!u->Loaded())
			if (My<Terran_Vulture> * pVulture = u->IsMy<Terran_Vulture>())
				if (pVulture->RemainingMines() > 0)
//						if ((u->Life() > u->MaxLife()*1/4) || !u->WorthBeingRepaired())
					if (!u->GetBehavior()->IsBlocking())
					if (!u->GetBehavior()->IsChecking())
					if (!u->GetBehavior()->IsDestroying())
					if (!u->GetBehavior()->IsKillingMine())
//						if (!u->GetBehavior()->IsLaying())
					if (!u->GetBehavior()->IsSniping())
					if (!u->GetBehavior()->IsWalking())
					if (!u->GetBehavior()->IsRetraiting())
					if (!u->GetBehavior()->IsLayingBack())
					if (!(u->GetBehavior()->IsFighting() &&
							(u->GetBehavior()->IsFighting()->ProtectTank() || (u->GetBehavior()->IsFighting()->Zone() == zone_t::air))))
					{
						FightingCandidates.push_back(pVulture);
					}

	return FightingCandidates;
}


} // namespace iron



