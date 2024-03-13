#pragma once

#include <BWAPI.h>
#include <set>
#include "Log.h"
#include "CherryVis.h"

// Utility to convert an enum class to its underlying type (usually int)
// See https://stackoverflow.com/a/33083231
#include <type_traits>
template <typename E>
constexpr auto to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}
