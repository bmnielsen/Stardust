#pragma once

#include "Play.h"

// Takes the next expansion.
class TakeExpansion : public Play
{
public:
    explicit TakeExpansion(BWAPI::TilePosition depotPosition);

    [[nodiscard]] const char *label() const override { return "TakeExpansion"; }

    void update() override;

    [[nodiscard]] bool constructionStarted() const;

    void disband(const std::function<void(const MyUnit&)> &removedUnitCallback,
                 const std::function<void(const MyUnit&)> &movableUnitCallback) override;

    BWAPI::TilePosition depotPosition;

protected:
    MyUnit builder;
};
