//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef DEFS_H
#define DEFS_H

#include <BWAPI.h>
#include "bwem/src/bwem.h"

#define DEV 0
#define INTERACTIVE 0

#define DISPLAY_SCV_FLEEING 0
#define DISPLAY_SCV_CHASING 0
#define DISPLAY_PRODUCTION 0
#define DISPLAY_WALLS 0
#define DISPLAY_BERSERKER_TIME 0


#include "bwem/src/mapPrinter.h"
#include "vect.h"

using namespace BWAPI;
using namespace BWAPI::UnitTypes::Enum;

using namespace BWEM;
using namespace BWEM::BWAPI_ext;
using namespace BWEM::utils;

namespace iron
{

typedef enum class zone_t {ground, air};

inline zone_t opposite(zone_t zone) { return zone == zone_t::ground ? zone_t::air : zone_t::ground; }

inline zone_t zone(BWAPI::UnitType type) { return type.isFlyer() ? zone_t::air : zone_t::ground; }

template<zone_t Zone>
inline zone_t most(zone_t za, zone_t zb)
{
	return (za == Zone) ? za : zb;
}






typedef int frame_t;
typedef int gameTime_t;
typedef int delay_t;
typedef BWAPI::UnitTypes::Enum::Enum tid_t;

#define FORWARD_DECLARE_MY(tid)	template<tid_t> class My; template<> class My<tid>;

const double pi = acos(-1);



inline Position toPosition(const Vect & v)	{ return Position(lround(v.x), lround(v.y)); }
inline Vect toVect(const Position & p)		{ return Vect(p.x,p.y); }


} // namespace iron


#endif

