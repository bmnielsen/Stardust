//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DEBUG_H
#define DEBUG_H

#include <BWAPI.h>
#include "BWEM/src/winutils.h"
#include "defs.h"
#include "utils.h"

namespace iron
{



template<class Code>
void exceptionHandler(const string & functionName, int delay, Code code)
{
	::unused(functionName);
	::unused(delay);

	const char * exceptionType = "?";
	char message[256];

	try
	{
		code();
		return;
	}
	catch (const ::Exception & e)
	{
		exceptionType = "Exception";
		strncpy(message, e.what(), 255);
	}
	catch (const std::exception & e)
	{
		exceptionType = "std::exception";
		strncpy(message, e.what(), 255);
	}
	catch (...)
	{
		exceptionType = "unexpected exception";
	}

#if DEV
	bw << exceptionType << " in " << functionName;//2
	if (message) bw << ": " << message;//2
	bw << endl;//2

	ai()->SetDelay(delay);//2
#else
/*
	if (ai()->m_logLines++ < 1000)
	{
	 	Log << ai()->Frame() << ": " << exceptionType << " in " << functionName;
	 	if (message) Log << ": " << message;//2
	 	Log << endl;
	}
*/
#endif
}

#if DEV && BWEM_USE_WINUTILS

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class TimerStats
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

class TimerStats
{
public:

	void				Starting()			{ m_Timer.Reset(); }
	void				Finishing();

	double				Time() const		{ return m_time; }
	double				Total() const		{ return m_total; }
	double				Avg() const			{ return Total() / Count(); }
	double				Max() const			{ return m_max; }
	int					Count() const		{ return m_count; }

private:
	Timer	m_Timer;
	double	m_time = 0;
	double	m_max = 0;
	double	m_total = 0;
	int		m_count = 0;
};
#endif



void reportCommandError(const string & command);


const bool crop = true;
void drawLineMap(Position a, Position b, Color color, bool crop = false);


#define CHECK_POS(p) 	assert_throw_plus(ai()->GetMap().Valid(p), my_to_string(p))

const int life_tag = 123456789;


struct CheckableInstance
{
public:
	int		m_lifeTag;

			CheckableInstance() : m_lifeTag(life_tag) {}
			~CheckableInstance() { m_lifeTag = 0; }
};

#define CI(p) 	(assert_throw_plus(((reinterpret_cast<std::uintptr_t>(p) | 1023) != 1023) && (p->m_lifeTag == life_tag), my_to_string(p)), p)
#define CTHIS 	assert_throw_plus(((reinterpret_cast<std::uintptr_t>(this) | 1023) != 1023), my_to_string(this));


} // namespace iron




#endif

