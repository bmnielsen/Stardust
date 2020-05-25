//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#ifndef UTILS_H
#define UTILS_H


#include "myassert.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <limits>




using namespace std;



#define DO_ONCE for (static int done = 0 ; !done++ ; )


struct RunOnce {
  template <typename T>
  RunOnce(T &&f) { f(); }
};


template<class T>
void clear_delete_elements(T & Container)
{
    for (typename T::iterator i = Container.begin() ; i != Container.end() ; i++)
        delete *i;

    Container.clear();
}



template<class T>
inline void push_back_if_not_found(T & Container, const typename T::value_type & Element)
{
    if (!contains(Container, Element)) Container.push_back(Element);
}




template<class T>
inline void push_back_assert_does_not_contain(T & Container, const typename T::value_type & Element)
{
    assert_throw(!contains(Container, Element));
	Container.push_back(Element);
}

#define PUSH_BACK_UNCONTAINED_ELEMENT(Container, Element)	\
do															\
{															\
    assert_throw(!contains(Container, Element));			\
	Container.push_back(Element);							\
}															\
__pragma(warning(push))										\
__pragma(warning(disable:4127))								\
while(0)													\
__pragma(warning(pop))										\
// end define



// SWAR algorithm
inline int numberOfSetBits(uint32_t i)
{
	i = i - ((i >> 1) & 0x55555555);
	i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
	return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}



void fileCopy(const string & dest, const string & src);









#endif

