//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "fight.h"
#include "strategy.h"
#include "baseDefense.h"
#include "walling.h"
#include "berserker.h"
#include "shallowTwo.h"
#include "expand.h"
#include "../units/fightSim.h"
#include "../units/factory.h"
#include "../units/cc.h"
#include "../units/army.h"
#include "../behavior/defaultBehavior.h"
#include "../behavior/fighting.h"
#include "../behavior/kiting.h"
#include "../behavior/raiding.h"
#include "../behavior/layingBack.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{




class EnemyGroup
{
public:
	static void						ClearFighters()				{ m_Fighters.clear(); }
	static const vector<MyUnit *> &	Fighters()					{ return m_Fighters; }

									EnemyGroup(const HisKnownUnit * u) : m_zone(iron::zone(u->Type())), m_Members(1, u) {}

	bool							ShouldAdd(const HisKnownUnit * u) const;
	void							Add(const HisKnownUnit * u)	{ push_back_assert_does_not_contain(m_Members, u); }

	void							AddFightingCandidate(MyUnit * u)
									{
										push_back_assert_does_not_contain(m_FightingCandidates, u);
										push_back_assert_does_not_contain(EnemyGroup::m_Fighters, u);

										if (u->Flying()) ++m_countFlyingCandidates;
										else             ++m_countNonFlyingCandidates;
									}

	void							Eval();
	void							Process() const;

	const vector<const HisKnownUnit *> &	Members() const		{ return m_Members; }
	const vector<MyUnit *> &		FightingCandidates() const	{ return m_FightingCandidates;}

	// Tells if there is at least one visible unit (in m_zone) in this group
	bool							Visible() const				{ return m_visibleUnits > 0; }

	bool							Dangerous() const			{ return m_dangerous; }
	bool							KiteOnly() const			{ return m_kiteOnly; }
	bool							AntiAir() const				{ return m_antiAir; }
	bool							AntiGround() const			{ return m_antiGround; }
	bool							FearMines() const			{ return m_fearMines; }
	bool							AllInMain() const			{ return m_allInMain; }
	int								VisibleZealots() const		{ return m_visibleZealots; }
	int								VisibleDragoons() const		{ return m_visibleDragoons; }

	Position						Pos() const					{ return m_position; }

	int								Size() const				{ return (int)m_Members.size(); }
	int								VisibleUnits() const		{ return m_visibleUnits; }

	// lowest value = highest priority
	int								Priority() const			{ return m_priority; }

//	int								Power() const				{ return m_power; }

	void							ComputeStats();

	void							Draw(BWAPI::Color col, Text::Enum textCol) const;


private:
									EnemyGroup() = delete;

	bool							StartFightCondition() const;
	bool							LeaveFightCondition() const;
	int								DistanceToNearestVisibleMember(const MyUnit * u) const;
	void							ComputePosition();

	static vector<MyUnit *>			m_Fighters;

	zone_t							m_zone;
	vector<const HisKnownUnit *>	m_Members;
	Position						m_position;
	VBase *							m_pMyBaseNearby;
	int								m_distToMyBase;
//	int								m_power;
	int								m_priority;
	int								m_visibleUnits;
	bool							m_hasGroundAttack;
	bool							m_hasAirAttack;
	bool							m_dangerous;
	bool							m_kiteOnly;
	bool							m_antiAir;
	bool							m_antiGround;
	bool							m_fearMines;
	bool							m_groundThreatBuildingNearby;
	bool							m_airThreatBuildingNearby;
	bool							m_allInMain;
	int								m_reavers;
	int								m_visibleZealots;
	int								m_visibleDragoons;

	vector<MyUnit *>				m_FightingCandidates;
	int								m_countFlyingCandidates = 0;
	int								m_countNonFlyingCandidates = 0;

	string							m_fightSimFinalState = "";
	int								m_fightSimEfficiency = 0;
	int								m_fightSimResult = 0;
};


vector<MyUnit *> EnemyGroup::m_Fighters;

bool EnemyGroup::ShouldAdd(const HisKnownUnit * u) const
{
	assert_throw(!contains(m_Members, u));

	int baseMaxTileDist = 12;
	if (u->Type() == Terran_Vulture) baseMaxTileDist += 4;
	if (u->Type() == Terran_Wraith) baseMaxTileDist += 4;
	if (u->Type() == Zerg_Mutalisk) baseMaxTileDist += 4;

	// There is at least one member in m_zone (Cf. the invariant in the constructor).
	for (const HisKnownUnit * m : m_Members)
		if (zone(m->Type()) == m_zone)
		{
			int maxTileDist = baseMaxTileDist;
			if (m->Type() == Terran_Vulture) maxTileDist += 4;
		
			int d = zoneDist(m->LastPosition(), u->LastPosition(), most<zone_t::air>(m_zone, zone(u->Type())));
			if (d > 32*maxTileDist) return false;
		}

	return true;
}


void EnemyGroup::ComputePosition()
{
	m_position = {0, 0};
	{
		int n = 0;
		// If m_zone == zone_t::ground there is at least one non flying member (Cf. the invariant in the constructor).
		for (const HisKnownUnit * m : m_Members)
			if ((m_zone == zone_t::air) || !m->Type().isFlyer())
			{
				m_position += m->LastPosition();
				++n;
			}
		m_position /= n;
	}

	if (m_zone == zone_t::ground)
		if (!ai()->GetMap().GetArea(WalkPosition(m_position)))
			for (const HisKnownUnit * m : m_Members)
				if (!m->Type().isFlyer())
					if (ai()->GetMap().GetArea(WalkPosition(m->LastPosition())))
					{
						m_position = m->LastPosition();
						break;
					}
}


void EnemyGroup::ComputeStats()
{
	assert_throw(Size() > 0);

	ComputePosition();

	int tanksHere = 0;
	int siegedTanksHere = 0;
	int goliathsHere = 0;
	int marinesHere = 0;
	int ghostsHere = 0;
	int medicsHere = 0;
	int wraithsHere = 0;
	int valkyriesHere = 0;
	int cruisersHere = 0;

	int zealotsVisibleHere = 0;
	int dragoonsHere = 0;
	int archonsVisibleHere = 0;
	int dangerousDarkTemplarsHere = 0;
	m_reavers = 0;
	int arbitersHere = 0;
	int interceptorsHere = 0;
	int scoutsHere = 0;
	int corsairsHere = 0;

	int devourersHere = 0;
	int hydrasHere = 0;
	int lurkersHere = 0;
	int mutasHere = 0;
	int scourgesHere = 0;
	int ultrasHere = 0;
	int zerglingsVisibleHere = 0;
	
	m_visibleUnits = 0;
	m_hasGroundAttack = false;
	m_hasAirAttack = false;
	m_allInMain = him().GetArea() != nullptr;

	for (const auto * info : m_Members)
	{
		if (m_allInMain)
			if (m_zone == zone_t::ground)
				if (info->GetHisUnit() && !info->GetHisUnit()->Flying())
					if (!contains(ext(him().GetArea())->EnlargedArea(), info->GetHisUnit()->GetArea()))
						m_allInMain = false;

		if (info->GetHisUnit() && (zone(info->Type()) == m_zone)) ++m_visibleUnits;
		if (groundAttack(info->Type(), him().Player())) m_hasGroundAttack = true;
		if (airAttack(info->Type(), him().Player())) m_hasAirAttack = true;

		switch (info->Type())
		{
		case Terran_Siege_Tank_Tank_Mode:	++tanksHere;			break;
		case Terran_Siege_Tank_Siege_Mode:	++tanksHere; ++siegedTanksHere;		break;
		case Terran_Goliath:				++goliathsHere;			break;
		case Terran_Marine:					++marinesHere;			break;
		case Terran_Ghost:					++ghostsHere;			break;
		case Terran_Medic:					++medicsHere;			break;
		case Terran_Wraith:					++wraithsHere;			break;
		case Terran_Valkyrie:				++valkyriesHere;		break;
		case Terran_Battlecruiser:			++cruisersHere;			break;

		case Protoss_Zealot:				if (info->GetHisUnit()) ++zealotsVisibleHere;	break;
		case Protoss_Dragoon:				++dragoonsHere;			break;
		case Protoss_Archon:				if (info->GetHisUnit()) ++archonsVisibleHere;			break;
		case Protoss_Dark_Templar:			if (info->GetHisUnit())
												if (!info->GetHisUnit()->Unit()->isDetected())
													if (!info->GetHisUnit()->WorthScaning())
														++dangerousDarkTemplarsHere;
											break;
		case Protoss_Reaver:				++m_reavers;			break;
		case Protoss_Arbiter:				++arbitersHere;			break;
		case Protoss_Interceptor:			++interceptorsHere;		break;
		case Protoss_Scout:					++scoutsHere;			break;
		case Protoss_Corsair:				++corsairsHere;			break;

		case Zerg_Devourer:					++devourersHere;		break;
		case Zerg_Hydralisk:				++hydrasHere;			break;
		case Zerg_Lurker:					++lurkersHere;			break;
		case Zerg_Mutalisk:					++mutasHere;			break;
		case Zerg_Scourge:					++scourgesHere;			break;
		case Zerg_Ultralisk:				++ultrasHere;			break;
		case Zerg_Zergling:					if (info->GetHisUnit()) ++zerglingsVisibleHere;	break;
		}
	}

	m_groundThreatBuildingNearby = any_of(him().Buildings().begin(), him().Buildings().end(), [this](const unique_ptr<HisBuilding> & b)
											{ return b->GroundThreatBuilding() && dist(b->Pos(), Pos()) < 10*32; });

	m_airThreatBuildingNearby = any_of(him().Buildings().begin(), him().Buildings().end(), [this](const unique_ptr<HisBuilding> & b)
											{ return b->AirThreatBuilding() && dist(b->Pos(), Pos()) < 10*32; });
						
	m_fearMines = tanksHere || goliathsHere || dragoonsHere || hydrasHere;

	m_visibleZealots = zealotsVisibleHere;
	m_visibleDragoons = dragoonsHere;

	m_dangerous = ((siegedTanksHere >= 2) || (medicsHere >= 2)) && (m_zone == zone_t::ground);
	m_kiteOnly = m_groundThreatBuildingNearby && (m_zone == zone_t::ground)
				|| m_airThreatBuildingNearby && (m_zone == zone_t::air)
				|| archonsVisibleHere
				|| dangerousDarkTemplarsHere// && (me().TotalAvailableScans() == 0)
				|| lurkersHere
				|| ultrasHere && (m_zone == zone_t::ground)
				|| zerglingsVisibleHere && !hydrasHere
				|| m_reavers;
	
	m_antiAir = m_airThreatBuildingNearby
				|| goliathsHere || marinesHere || ghostsHere
				|| (wraithsHere || cruisersHere || valkyriesHere) && (m_zone == zone_t::ground)
				|| dragoonsHere || archonsVisibleHere
				|| (interceptorsHere || scoutsHere || corsairsHere || arbitersHere) && (m_zone == zone_t::ground)
				|| hydrasHere || scourgesHere
				|| (devourersHere || mutasHere) && (m_zone == zone_t::ground);

	m_antiGround = (m_zone == zone_t::air) &&
					(m_groundThreatBuildingNearby || siegedTanksHere || m_reavers || ultrasHere);
/*
	m_power = 0;
	for (const HisKnownUnit * m : m_Members)
	{
		switch(m->Type())
		{
			case Terran_Marine:						m_power += 1; break;
			case Terran_Firebat:					m_power += 1; break;
			case Terran_Medic:						m_power += 2; break;
			case Terran_Ghost:						m_power += 5; break;
			case Terran_Vulture:					m_power += 2; break;
			case Terran_Siege_Tank_Tank_Mode:		m_power += 6; break;
			case Terran_Siege_Tank_Siege_Mode:		m_power += 6; break;
			case Terran_Goliath:					m_power += 6; break;
			case Terran_Wraith:						m_power += 2; break;
			case Terran_Valkyrie:					m_power += 3; break;
			case Terran_Battlecruiser:				m_power += 10; break;

			case Protoss_Archon:					m_power += 4; break;
			case Protoss_Dark_Archon:				m_power += 1; break;
			case Protoss_Dark_Templar:				m_power += 2; break;
			case Protoss_Dragoon:					m_power += 6; break;
			case Protoss_High_Templar:				m_power += 1; break;
			case Protoss_Reaver:					m_power += 10; break;
			case Protoss_Zealot:					m_power += 1; break;

			default:								m_power += max(1, (m->LastLife() + m->LastShields()) / 40); break;
			break;
		}
	}
	m_power /= 2;
*/

	m_pMyBaseNearby = findMyClosestBase(m_position);
	m_distToMyBase = zoneDist(m_position, m_pMyBaseNearby ? m_pMyBaseNearby->BWEMPart()->Center() : me().StartingBase()->Center(), m_zone);

	m_priority = m_pMyBaseNearby ? 0 : m_distToMyBase / (32*8);
//	m_priority -= m_power;
}


void EnemyGroup::Draw(BWAPI::Color col, Text::Enum textCol) const
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
	bw->drawBoxMap(topLeft, bottomRight, col);
	bw->drawTextMap(topLeft + Position(2, -12), "%c[%d] (%d) %s", textCol, m_fightSimResult, m_fightSimEfficiency, m_fightSimFinalState.c_str());

	for (const MyUnit * u : m_FightingCandidates)
		drawLineMap(u->Pos(), Pos(), col);

#endif
}


int EnemyGroup::DistanceToNearestVisibleMember(const MyUnit * u) const
{
	int dist = numeric_limits<int>::max();
	for (const HisKnownUnit * m : m_Members)
		if (m->GetHisUnit())
			dist = min(dist, roundedDist(u->Pos(), m->LastPosition()));

	assert_throw(dist != numeric_limits<int>::max());
	return dist;
}


void EnemyGroup::Eval()
{
	assert_throw(Visible());
	assert_throw(Size() > 0);
	assert_throw(FightingCandidates().size() > 0);

	if (m_zone == zone_t::ground)
	{
		if (m_hasGroundAttack && !m_hasAirAttack)	// zealot
			if (m_countNonFlyingCandidates == 0)
			{
				m_fightSimFinalState = "win (air weakness)";
				m_fightSimEfficiency = 1000;
				m_fightSimResult = +1;
				return;
			}

		if (!m_hasGroundAttack)						// darch archon + corsair (m_hasAirAttack) or darch archon (!m_hasAirAttack)
		{	// The case (m_hasAirAttack && (m_countFlyingCandidates > 0)) is handled by AntiAir()
			m_fightSimFinalState = "win (ground weakness)";
			m_fightSimEfficiency = 1000;
			m_fightSimResult = +1;
			return;
		}

/*
//		if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
//			if (pShallowTwo->KeepMarinesAtHome())
				if (none_of(m_Members.begin(), m_Members.end(), [](const HisKnownUnit * u)
						{
							return u->GetHisUnit() && !u->GetHisUnit()->Flying() &&
								((contains(me().EnlargedAreas(), u->GetHisUnit()->GetArea(check_t::no_check))) ||
								(homeAreaToHold() == u->GetHisUnit()->GetArea(check_t::no_check)));
						}))
				{
					m_fightSimFinalState = "bio retrait (KeepMarinesAtHome)";
					m_fightSimEfficiency = -1000;
					m_fightSimResult = -1;
				///	bw << m_fightSimFinalState << endl;
					return;
				}
*/
	}
	else
	{
		if (m_countFlyingCandidates == 0)			// do not pursue flying enemies with ground units
		{
			m_fightSimFinalState = "loose (do not pursue)";
			m_fightSimEfficiency = -1000;
			m_fightSimResult = -1;
			return;
		}

		if (!m_hasAirAttack)						// observer (!m_hasGroundAttack) or guardian (m_hasGroundAttack)
		{
			// TODO: virer les ground candidates (inutiles)
			m_fightSimFinalState = "win (air weakness)";
			m_fightSimEfficiency = 1000;
			m_fightSimResult = +1;
			return;
		}
	}

	int marines = 0;
	int marinesFighting = 0;
	int marinesThatShoted = 0;
	int marinesThatShotedRecently = 0;
	int marinesThatHasDragoonsInRange = 0;
	vector<const MyUnit *> SiegedTanks;
	if (him().IsProtoss())
	{
//		set<HisBWAPIUnit *> HisUnitsShooting;

		for (const MyUnit * u : m_FightingCandidates)
			if (u->Is(Terran_Siege_Tank_Siege_Mode) || u->GetBehavior()->IsSieging())
				SiegedTanks.push_back(u);

		for (const MyUnit * u : m_FightingCandidates)
			if (u->Is(Terran_Marine))
			{
//				for (const FaceOff & fo : u->FaceOffs())
//					if (fo.DistanceToHisRange() <= 0)
//						HisUnitsShooting.insert(fo.His());

/*
				if (!SiegedTanks.empty())
					if (none_of(SiegedTanks.begin(), SiegedTanks.end(), [u](const MyUnit * tank)
						{ return roundedDist(tank->Pos(), u->Pos()) < 5*32; }))
					{
						bw << "FAR" << endl;
						m_fightSimFinalState = "bio retrait (too far from tanks)";
						m_fightSimEfficiency = -1000;
						m_fightSimResult = -1;
						return;
					}	
*/

				++marines;
				if (u->GetBehavior()->IsFighting())
				{
					++marinesFighting;
					if (u->GetBehavior()->IsFighting()->LastShot())
					{
						++marinesThatShoted;
						if (ai()->Frame() - u->GetBehavior()->IsFighting()->LastShot() < 20)
						{
							++marinesThatShotedRecently;

							if (any_of(u->FaceOffs().begin(), u->FaceOffs().end(), [](const FaceOff & fo)
									{ return fo.His()->Is(Protoss_Dragoon) && fo.DistanceToMyRange() <= 0; }))
								++marinesThatHasDragoonsInRange;
						}
					}
				}
			}

//		if (m_visibleDragoons >= 1)
			if (m_visibleZealots >= 1)
			{
				double k = me().HasResearched(TechTypes::Stim_Packs) ? 1.0 : 0.7;
				if ((m_visibleZealots >= 15) && (marines*k < 20) ||
					(m_visibleZealots >= 12) && (marines*k < 16) ||
					(m_visibleZealots >= 9) && (marines*k < 14) ||
					(m_visibleZealots >= 7) && (marines*k < 12) ||
					(m_visibleZealots >= 5) && (marines*k < 10) ||
					(m_visibleZealots >= 3) && (marines*k < 8) ||
					(m_visibleZealots >= 2) && (marines*k < 6) ||
					(m_visibleZealots >= 1) && (marines*k < 5))
				{
					m_fightSimFinalState = "bio retrait (too many zeolots)";
					m_fightSimEfficiency = -1000;
					m_fightSimResult = -1;
					return;
				}	
			}

/*
//		if (!m_reavers)
		{
			if (marinesFighting == marines)
			{
				frame_t delay = 30;
				if (const ShallowTwo * pShallowTwo = ai()->GetStrategy()->Has<ShallowTwo>())
					if (pShallowTwo->KeepMarinesAtHome())
						if (const Area * pFightArea = ai()->GetMap().GetArea(WalkPosition(Pos())))
							if (!contains(me().EnlargedAreas(), pFightArea) &&
								(homeAreaToHold() != pFightArea) &&
								(findNatural(me().StartingVBase())->BWEMPart()->GetArea()) != pFightArea)
								delay = 100;

				if ((marinesThatShotedRecently*2 < marinesThatShoted) && (marinesThatShotedRecently < 2*VisibleUnits()))
				{
					for (MyUnit * u : m_FightingCandidates)
						u->SetNoFightUntil(ai()->Frame() + delay);

					m_fightSimFinalState = "bio retrait (not enough recent shots)";
					m_fightSimEfficiency = -1000;
					m_fightSimResult = -1;
					return;
				}

				if (ai()->Frame() - him().LastDragoonKilledFrame() < 5)
					if (dist(him().LastDragoonKilledPos(), Pos()) < 7*32)
						if (marinesThatHasDragoonsInRange < 3)
						{
							for (MyUnit * u : m_FightingCandidates)
								u->SetNoFightUntil(ai()->Frame() + delay);

							m_fightSimFinalState = "bio retrait (dragoon killed)";
							m_fightSimEfficiency = -1000;
							m_fightSimResult = -1;
						///	bw << m_fightSimFinalState << endl;
							return;
						}
			}
		}
*/
	}

	FightSim Sim(m_zone);
	sort(m_FightingCandidates.begin(), m_FightingCandidates.end(), [](const MyUnit * a, const MyUnit * b)
			{
				if (a->Is(Terran_Medic) && !b->Is(Terran_Medic)) return true;
				if (!a->Is(Terran_Medic) && b->Is(Terran_Medic)) return false;
				return a->LifeWithShields() < b->LifeWithShields();
			});

	sort(m_Members.begin(), m_Members.end(), [](const HisKnownUnit * a, const HisKnownUnit * b)
			{ return a->LastLife() + a->LastShields() < b->LastLife() + b->LastShields(); });

	for (const MyUnit * u : m_FightingCandidates)
	{
		const frame_t timeToReachFight =
			(u->CanMove() && !(u->Is(Terran_Siege_Tank_Siege_Mode) || u->GetBehavior()->IsSieging()))
			? frame_t((DistanceToNearestVisibleMember(u) - u->Range(m_zone)) / (u->Speed()*2/3))
			: 0;

		int life = u->Life();
		if (u->Is(Terran_Medic)) life = min(life, 2*u->Energy());
		if (him().IsProtoss() && (me().Units(Terran_Siege_Tank_Tank_Mode).size() < 0)) life = life*5/4;
		Sim.AddMyUnit(u->Type(), life, u->Shields(), u->Pos(), timeToReachFight, u->Unit()->isStimmed());
	}

	for (const HisKnownUnit * m : m_Members)
//		if (groundAttack(m->Type(), him().Player()))
		Sim.AddHisUnit(m->Type(), m->LastLife(), m->LastShields(), m->LastPosition());


//	Timer t;
	Sim.Eval();
//	if (t.ElapsedMilliseconds() > 50.0) Log << Sim.FinalState() << endl;
	m_fightSimFinalState = Sim.FinalState();
		//"  count = " + to_string(marinesFighting) + "/"  + to_string(marines) +
		//"  shots = " + to_string(marinesThatShotedRecently) + "/" + to_string(marinesThatShoted);
	m_fightSimEfficiency = Sim.Efficiency();
	m_fightSimResult = Sim.Result();
}


bool EnemyGroup::StartFightCondition() const
{
	return m_fightSimResult >= 0;
}


bool EnemyGroup::LeaveFightCondition() const
{
	return m_fightSimResult < 0;
}


void EnemyGroup::Process() const
{
	assert_throw(Visible());
	assert_throw(Size() > 0);
	assert_throw(FightingCandidates().size() > 0);

	for (MyUnit * u : m_FightingCandidates)
		assert_throw(!u->GetBehavior()->IsFighting() ||
						( !u->GetBehavior()->IsFighting()->ProtectTank() &&
							((m_zone == zone_t::air) || (m_zone == u->GetBehavior()->IsFighting()->Zone()))
						)
					);

	if (StartFightCondition())
	{
		map<MyUnit *, int> DistToFight;
		int sumDistToFight = 0;
		for (MyUnit * u : m_FightingCandidates)
			if (!u->GetBehavior()->IsHealing())
			{
				const int d = zoneDist(Pos(), u->Pos(), most<zone_t::air>(m_zone, u->Zone()));
				DistToFight[u] = d;
				sumDistToFight += d;
			}
		int avgDistToFight = sumDistToFight / max(1, (int)DistToFight.size());
		int fightZoneRadius = int(avgDistToFight * 1.5);
#if DEV
		bw->drawCircleMap(Pos(), avgDistToFight, Colors::White);
		bw->drawCircleMap(Pos(), fightZoneRadius, Colors::White);
#endif
		for (;;)
		{
			MyUnit * pNearestFightingCandidateInFightCircle = nullptr;
			int distNearestFightingCandidateInFightCircle = numeric_limits<int>::max();

			MyUnit * pFarestNonFightingCandidate = nullptr;
			int distFarestNonFightingCandidate = numeric_limits<int>::min();

			bool firstShotOccured = false;
			for (MyUnit * u : m_FightingCandidates)
				if (!(u->GetBehavior()->IsHealing() || (u->Is(Terran_Medic) && m_reavers)))
				{
					const int d = DistToFight[u];

					if (u->GetBehavior()->IsFighting() && (u->GetBehavior()->IsFighting()->Zone() == m_zone))
					{
						if (u->GetBehavior()->IsFighting()->Shooted()) firstShotOccured = true;

						if (d <= fightZoneRadius)
							if (d < distNearestFightingCandidateInFightCircle)
							{
								distNearestFightingCandidateInFightCircle = d;
								pNearestFightingCandidateInFightCircle = u;
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

			if (pFarestNonFightingCandidate)
			{
				const int maxSpace = him().IsProtoss() && m_FightingCandidates.front()->Type().isOrganic() ? 20 : 2*32;
				if (firstShotOccured ||
					(distFarestNonFightingCandidate > fightZoneRadius) ||
					!pNearestFightingCandidateInFightCircle ||
					(distFarestNonFightingCandidate > distNearestFightingCandidateInFightCircle - maxSpace))
				{
					pFarestNonFightingCandidate->ChangeBehavior<Fighting>(pFarestNonFightingCandidate, Pos(), m_zone, m_fightSimResult);
					continue;
				}
			}

			break;
		}

/*
		if (pNearestFightingCandidate)
		{
#if DEV
			bw->drawTriangleMap(pNearestFightingCandidate->Pos() + Position(0, -10), pNearestFightingCandidate->Pos() + Position(-10, -5), pNearestFightingCandidate->Pos() + Position(+10, -5), Color());
#endif

			for (MyUnit * u : m_FightingCandidates)
				if (!(u->GetBehavior()->IsFighting() && (u->GetBehavior()->IsFighting()->Zone() == m_zone)))
					if (u->CanMove() && !(u->Is(Terran_Siege_Tank_Siege_Mode) || u->GetBehavior()->IsSieging()))
						if (!u->GetBehavior()->IsLaying())
						if (!u->GetBehavior()->IsHealing())
						{
							const int d = DistToFight[u];

							if (firstShotOccured || (d > distNearestFightingCandidate - 20))
								u->ChangeBehavior<Fighting>(u, Pos(), m_zone);
						}

//			if (pFarestNonFightingCandidate)
//				if (firstShotOccured || (distFarestNonFightingCandidate > distNearestFightingCandidate - 20))
//					pFarestNonFightingCandidate->ChangeBehavior<Fighting>(pFarestNonFightingCandidate, Pos(), m_zone);

		}
		else
		{
			if (pFarestNonFightingCandidate)
				pFarestNonFightingCandidate->ChangeBehavior<Fighting>(pFarestNonFightingCandidate, Pos(), m_zone);
		}
*/
	}
	else if (LeaveFightCondition())
	{
		for (MyUnit * u : m_FightingCandidates)
			if (u->GetBehavior()->IsFighting() && (u->GetBehavior()->IsFighting()->Zone() == m_zone))
				//if (!(him().IsProtoss() && contains(me().EnlargedAreas(), u->GetArea(check_t::no_check))))
					u->ChangeBehavior<Kiting>(u);
	}
}





//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Fight
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Fight::Fight()
{
}


Fight::~Fight()
{
}


string Fight::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


void Fight::OnFrame_v()
{
//	if (!(him().IsTerran() || him().IsProtoss())) return;

	if (him().IsProtoss())
		if (Zone() == zone_t::ground)
			if (ai()->GetStrategy()->Active<Walling>())
				return;

//	UpdateBerserkerLimit
//	if (m_berserkerUntil == 0)

//	bw << ai()->Frame() << ") process " << Name() << endl;

	EnemyGroup::ClearFighters();

	vector<const HisKnownUnit *> Enemies;
	for (const auto & info : him().AllUnits())
		//if (ai()->Frame() - info.second.LastTimeVisible() < 50)
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
	for (zone_t zone : {Zone(), opposite(Zone())})
		for (const HisKnownUnit * u : Enemies) if (iron::zone(u->Type()) == zone)
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

			if (zone == Zone())	// so enemy groups contain at least one unit in Zone()
				EnemyGroups.emplace_back(u);
		}

	for (EnemyGroup & Group : EnemyGroups)
		Group.ComputeStats();

	sort(EnemyGroups.begin(), EnemyGroups.end(), [](const EnemyGroup & a, const EnemyGroup & b)
													{ return a.Priority() < b.Priority(); });

//	if (VisibleEnemyGroups.empty()) return;

	// Assigns candidates to nearest visible EnemyGroup

	vector<MyUnit *> Candidates = FindCandidates();
	for (MyUnit * cand : Candidates)
	{
		EnemyGroup *	pNearestVisibleEnemyGroup = nullptr;

		const bool siegedTank = cand->Is(Terran_Siege_Tank_Siege_Mode) || cand->GetBehavior()->IsSieging();

		const int baseDefenseRadius = 32 * (ai()->GetMap().Size().x + ai()->GetMap().Size().y)/4;
		const int defaultRadius = siegedTank ? 32*12 :
						cand->Is(Terran_Wraith) ? 32*15 :
						32*30;

		int minDist = baseDefenseRadius;

		for (EnemyGroup & Group : EnemyGroups)
			if (Group.Visible())
			{
				if (Group.Priority() > 0)
					minDist = min(minDist, defaultRadius);

				if (pNearestVisibleEnemyGroup)
					if ((pNearestVisibleEnemyGroup->Priority() == 0) && (Group.Priority() > 0))
						continue;

				int d = zoneDist(cand->Pos(), Group.Pos(), siegedTank ? zone_t::air : most<zone_t::air>(Zone(), cand->Zone()));
				if (d < minDist)
				{
					minDist = d;
					pNearestVisibleEnemyGroup = &Group;
				}
			}

		if (pNearestVisibleEnemyGroup)
		{
			if (!((Zone() == zone_t::ground) && ai()->GetStrategy()->Active<Berserker>()))
			{
				if (pNearestVisibleEnemyGroup->KiteOnly()) continue;
				if (pNearestVisibleEnemyGroup->Dangerous() && (pNearestVisibleEnemyGroup->Priority() > 3)) continue;
				if (pNearestVisibleEnemyGroup->AntiAir() && cand->Flying()) continue;
				if (pNearestVisibleEnemyGroup->AntiGround() && !cand->Flying()) continue;
	//			if (pNearestVisibleEnemyGroup->FightingCandidates().empty() && cand->Flying()) continue;	// at least one ground candidate
				
				if (Zone() == zone_t::ground)
					if (!cand->Flying())
						if (him().IsProtoss())
							if (pNearestVisibleEnemyGroup->AllInMain())
								if (!contains(ext(him().GetArea())->EnlargedArea(), cand->GetArea()))
									if (him().HasBuildingsInNatural())
										continue;

				if (pNearestVisibleEnemyGroup->FearMines())
					if (auto * pVulture = cand->IsMy<Terran_Vulture>())
						if (pVulture->RemainingMines() >= 2)
							if (!contains(me().EnlargedAreas(), pVulture->GetArea(check_t::no_check)))
								if (me().TechAvailableIn(TechTypes::Spider_Mines, 200))
									if (LayingBack::Allowed(pVulture))
									{
										pVulture->ChangeBehavior<LayingBack>(pVulture, me().StartingBase()->Center());
										continue;
									}

			}

			pNearestVisibleEnemyGroup->AddFightingCandidate(cand);
		}
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

	// Finally, release all Fighting units that were not processed through an EnemyGroup:
	vector<Fighting *> Fighters = Fighting::Instances();
	for (Fighting * pFighter : Fighters)
		if (!pFighter->ProtectTank())
			if (pFighter->Zone() == Zone())
				if (!contains(EnemyGroup::Fighters(), pFighter->Agent()))
					pFighter->Agent()->ChangeBehavior<DefaultBehavior>(pFighter->Agent());
}





//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class GroundFight
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


GroundFight::GroundFight() : Fight()
{
}


bool GroundFight::ShouldWatchEnemy(BWAPI::UnitType type) const
{
	if (!type.isWorker())
		switch (type)
		{
		case Terran_Marine:
		case Terran_Medic:
		case Terran_Firebat:
		case Terran_Ghost:
		case Terran_Vulture:
		case Terran_Siege_Tank_Tank_Mode:
		case Terran_Siege_Tank_Siege_Mode:
		case Terran_Goliath:
		case Terran_Wraith:
		case Terran_Valkyrie:
		case Terran_Battlecruiser:

		case Protoss_Archon:
		case Protoss_Dark_Archon:
		case Protoss_Dark_Templar:
		case Protoss_Dragoon:
		case Protoss_High_Templar:
		case Protoss_Reaver:
		case Protoss_Zealot:
//		case Protoss_Carrier:
		case Protoss_Interceptor:
		case Protoss_Arbiter:
		case Protoss_Scout:
		case Protoss_Corsair:

		case Zerg_Broodling:
		case Zerg_Defiler:
		case Zerg_Devourer:
		case Zerg_Guardian:
		case Zerg_Hydralisk:
		case Zerg_Lurker:
		case Zerg_Mutalisk:
//		case Zerg_Overlord:
//		case Zerg_Queen:
		case Zerg_Scourge:
		case Zerg_Ultralisk:
		case Zerg_Zergling:
			return true;
		}

	return false;
}


vector<MyUnit *> GroundFight::FindCandidates() const
{
	vector<MyUnit *> FightingCandidates;

	for (UnitType type : {Terran_Siege_Tank_Siege_Mode, Terran_Siege_Tank_Tank_Mode,
						Terran_Goliath,
						Terran_Vulture,
						Terran_Marine,
						Terran_Medic,
						//Terran_SCV,
						Terran_Wraith})
		for (const auto & u : me().Units(type))
			if (u->Completed())
			if (!u->Loaded())
				if (!((type == Terran_SCV) && u->GetStronghold()))
				if (!((type == Terran_Siege_Tank_Tank_Mode) && me().Army().KeepTanksAtHome()))
//						if ((u->Life() > u->MaxLife()*1/4) || !u->WorthBeingRepaired())
						if (ai()->Frame() >= u->NoFightUntil())
							if (!u->GetBehavior()->IsBlocking())
							if (!u->GetBehavior()->IsChecking())
							if (!u->GetBehavior()->IsDestroying())
							if (!u->GetBehavior()->IsKillingMine())
	//						if (!u->GetBehavior()->IsLaying())
							if (!u->GetBehavior()->IsSniping())
							if (!u->GetBehavior()->IsWalking())
							if (!u->GetBehavior()->IsRetraiting())
							if (!u->GetBehavior()->IsLayingBack())
							if (!u->GetBehavior()->IsExecuting())
							if (!u->GetBehavior()->IsDropping())
							if (!(u->GetBehavior()->IsFighting() &&
								 (u->GetBehavior()->IsFighting()->ProtectTank() || (u->GetBehavior()->IsFighting()->Zone() == zone_t::air))))
							{
	/*
								if (him().IsProtoss())
									if (type == Terran_Marine)
										if (u->Life() <= 10)
											continue;

	*/
								FightingCandidates.push_back(u.get());
							}

	return FightingCandidates;
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class AirFight
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


AirFight::AirFight() : Fight()
{
}


bool AirFight::ShouldWatchEnemy(BWAPI::UnitType type) const
{
	if (!type.isWorker())
		switch (type)
		{
		case Terran_Marine:
//		case Terran_Medic:
//		case Terran_Firebat:
		case Terran_Ghost:
//		case Terran_Vulture:
		case Terran_Siege_Tank_Tank_Mode:
		case Terran_Siege_Tank_Siege_Mode:
		case Terran_Goliath:
		case Terran_Wraith:
		case Terran_Valkyrie:
		case Terran_Battlecruiser:
		case Terran_Dropship:

		case Protoss_Archon:
//		case Protoss_Dark_Archon:
//		case Protoss_Dark_Templar:
		case Protoss_Dragoon:
		case Protoss_High_Templar:
		case Protoss_Reaver:
//		case Protoss_Zealot:
		case Protoss_Carrier:
		case Protoss_Interceptor:
		case Protoss_Arbiter:
		case Protoss_Scout:
		case Protoss_Corsair:
		case Protoss_Shuttle:

//		case Zerg_Broodling:
		case Zerg_Defiler:
		case Zerg_Devourer:
//		case Zerg_Guardian:
		case Zerg_Hydralisk:
//		case Zerg_Lurker:
		case Zerg_Mutalisk:
//		case Zerg_Overlord:
//		case Zerg_Queen:
		case Zerg_Scourge:
		case Zerg_Ultralisk:
//		case Zerg_Zergling:
			return true;
		}

	return false;
}


vector<MyUnit *> AirFight::FindCandidates() const
{
	vector<MyUnit *> FightingCandidates;

	for (UnitType type : {	Terran_Wraith,
							Terran_Goliath,
							Terran_Marine,
							Terran_Medic
							})
		for (const auto & u : me().Units(type))
			if (u->Completed())
			if (!u->Loaded())
				if (!((type == Terran_SCV) && u->GetStronghold()))
				if (!((type == Terran_Wraith) && (u->Life() <= 80)))
//				if ((u->Life() > u->MaxLife()*1/4) || !u->WorthBeingRepaired())
						if (!u->GetBehavior()->IsBlocking())
						if (!u->GetBehavior()->IsChecking())
						if (!u->GetBehavior()->IsDestroying())
						if (!u->GetBehavior()->IsKillingMine())
//						if (!u->GetBehavior()->IsLaying())
						if (!u->GetBehavior()->IsSniping())
						if (!u->GetBehavior()->IsWalking())
						if (!(u->GetBehavior()->IsFighting() && u->GetBehavior()->IsFighting()->ProtectTank()))
							FightingCandidates.push_back(u.get());

	return FightingCandidates;
}



} // namespace iron



