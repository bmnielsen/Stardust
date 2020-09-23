#pragma once

#include "Common.h"
#include "BuildingPlacement.h"
#include "Base.h"
#include <variant>

// Can either be empty (which either means N/A or don't care), a neighbourhood, or a specific tile position (buildings only)
using ProductionLocation = std::variant<std::monostate, BuildingPlacement::Neighbourhood, BuildingPlacement::BuildLocation, Base *>;
