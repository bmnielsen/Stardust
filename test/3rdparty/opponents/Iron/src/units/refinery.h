//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef REFINERY_H
#define REFINERY_H

#include "my.h"


namespace iron
{


template<tid_t> class My;

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Refinery>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

template<>
class My<Terran_Refinery> : public MyBuilding
{
public:
	static ConstructingExpert *	GetConstructingExpert();

							My(BWAPI::Unit u);

	Geyser *				GetGeyser() const			{CI(this); return m_pGeyser; }
	void					SetGeyser(Geyser * g)		{CI(this); assert(!m_pGeyser != !g); m_pGeyser = g; }

private:
	void					DefaultBehaviorOnFrame() override;
	int						MaxRepairers() const override;

	Geyser *				m_pGeyser = nullptr;

	static ExpertInConstructing<Terran_Refinery>	m_ConstructingExpert;
};


} // namespace iron


#endif

