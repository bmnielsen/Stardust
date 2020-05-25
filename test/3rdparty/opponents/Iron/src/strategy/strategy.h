//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef STRATEGY_H
#define STRATEGY_H

#include <BWAPI.h>
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class MyUnit;
class Strat;
class BWAPIUnit;
	
//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Strategy
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Strategy
{
public:
									Strategy();

	void							OnFrame(bool preBehavior);

	const vector<TilePosition> &	HisPossibleLocations() const	{ return m_HisPossibleLocations; }
	void							RemovePossibleLocation(TilePosition pos);

	const vector<unique_ptr<Strat>> &	Strats() const	{ return m_Strats; }

	template<class S> const S *		Has() const;
	template<class S> S *			Has();

	template<class S> const S *		Detected() const;
	template<class S> const S *		Active() const;

	void							OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit);
	int								MinScoutingSCVs() const			{ return m_minScoutingSCVs; }
	void							SetMinScoutingSCVs(int n)		{ m_minScoutingSCVs = n; }

	bool							TimeToBuildFirstShop() const;
	bool							TimeToScout() const;

private:
	bool							RessourceDispatch();
	bool							MiningDispatch();

	vector<TilePosition>			m_HisPossibleLocations;
	vector<unique_ptr<Strat>>		m_Strats;

	int								m_minScoutingSCVs = 1;
};


template<class S>
const S * Strategy::Has() const
{
	for (const auto & s : m_Strats)
		if (auto p = dynamic_cast<const S *>(s.get()))
			return p;

	return nullptr;
}


template<class S>
S * Strategy::Has()
{
	for (const auto & s : m_Strats)
		if (auto p = dynamic_cast<S *>(s.get()))
			return p;

	return nullptr;
}


template<class S>
const S * Strategy::Detected() const
{
	if (const S * s = ai()->GetStrategy()->Has<S>())
		if (s->Detected())
			return s;

	return nullptr;
}


template<class S>
const S * Strategy::Active() const
{
	if (const S * s = ai()->GetStrategy()->Has<S>())
		if (s->Active())
			return s;

	return nullptr;
}


} // namespace iron


#endif

