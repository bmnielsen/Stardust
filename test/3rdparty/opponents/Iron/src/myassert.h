//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////




#ifndef MY_ASSERT_H
#define MY_ASSERT_H


#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <limits>





template <class T> void unused(const T &) {}


class Exception : public std::runtime_error
{
public:
	explicit                Exception(const char * message) : std::runtime_error(message) {}
	explicit                Exception(const std::string & message) : Exception(message.c_str()) {}
};



void onAssertThrowFailed(const std::string & file, int line, const std::string & condition, const std::string & message);

#define assert_throw_plus(cond, message)     ((cond)?(void)0:onAssertThrowFailed(__FILE__,__LINE__, #cond, message))
#define assert_throw(cond)                   assert_throw_plus(cond, "")

#if defined(_DEBUG)

void onAssertFailed(const std::string & file, int line, const std::string & condition, const std::string & message);

#define assert_plus(cond, message)     ((cond)?(void)0:onAssertFailed(__FILE__,__LINE__, #cond, message))
#define assert_(cond)                   assert_plus(cond, "")

#else

#define assert_plus(cond, message)     ((void)0)
#define assert_(cond)                   ((void)0)

#endif






#endif

