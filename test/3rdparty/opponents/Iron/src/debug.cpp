//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "debug.h"
#include "Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{

#if DEV && BWEM_USE_WINUTILS

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class TimerStats
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

void TimerStats::Finishing()
{
	++m_count;
	m_time = m_Timer.ElapsedMilliseconds();
	m_total += m_time;
	m_max = max(m_max, m_time);
}
#endif


void reportCommandError(const string & command)
{
	::unused(command);
#if DEV
	Error err = bw->getLastError();
	bw << "BWAPI ERROR: " << err << " on: " << command << endl;//2
	bw->setLastError();//2
	
	//if (err != Errors::Incompatible_State)
	//	ai()->SetDelay(2000);//2
#endif
}


void drawLineMap(Position a, Position b, Color color, bool crop)
{
	if (crop)
	{
		a = ai()->GetMap().Crop(a);
		b = ai()->GetMap().Crop(b);
	}

	CHECK_POS(a);
	CHECK_POS(b);
	bw->drawLineMap(a, b, color);
}
	
} // namespace iron


