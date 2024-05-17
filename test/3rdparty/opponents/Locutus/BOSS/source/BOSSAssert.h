#pragma once

#include "Common.h"
#include <cstdarg>
#include "BOSSException.h"

#include <ctime>

namespace BOSS
{

class GameState;
namespace Assert
{
    const std::string currentDateTime();
    void ReportFailure(const GameState * state, const char * condition, const char * file, int line, const char * msg, ...);
}
}

#define BOSS_ASSERT(cond, msg, ...)
#define BOSS_ASSERT_STATE(cond, state, filename, msg, ...)
