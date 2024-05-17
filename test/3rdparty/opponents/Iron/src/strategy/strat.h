//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef STRAT_H
#define STRAT_H

#include <BWAPI.h>
#include "../defs.h"
#include "../utils.h"


namespace iron
{

class BWAPIUnit;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Strat
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class Strat
{
public:
//									Strat() = default;
	virtual							~Strat() = default;

	void							OnFrame()						{ OnFrame_v(); }

	virtual string					Name() const = 0;
	virtual string					StateDescription() const = 0;
	virtual void					OnBWAPIUnitDestroyed(BWAPIUnit * ) {};
	virtual bool					PreBehavior() const				{ return false; }

	bool							ToRemove() const				{ return m_toRemove; }
	void							Discard()						{ m_toRemove = true; }

private:

protected:

	virtual void					OnFrame_v() = 0;
	bool							m_toRemove = false;
};


} // namespace iron


#endif

