//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "myassert.h"
#include <assert.h>


void onAssertThrowFailed(const std::string & file, int line, const std::string & condition, const std::string & message)
{
	unused(file);
	unused(line);
	unused(condition);
	unused(message);

	assert(false);
	throw Exception("An error occurred in " + file + ", line " + std::to_string(line) + " - " + message);
}


#if defined(_DEBUG)
void onAssertFailed(const std::string & file, int line, const std::string & condition, const std::string & message)
{
	unused(file);
	unused(line);
	unused(condition);
	unused(message);

#if LIB_USER
	throw Exception("an error occurred in " + file + ", line " + std::to_string(line) + "");
#else
	assert(false);
#endif
}
#endif
