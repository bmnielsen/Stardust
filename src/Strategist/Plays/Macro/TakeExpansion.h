#pragma once

#include "Play.h"

// Takes the next expansion.
class TakeExpansion : public Play
{
public:
    explicit TakeExpansion(BWAPI::TilePosition depotPosition);

    [[nodiscard]] const char *label() const override { return "TakeExpansion"; }

    void update() override;

    bool constructionStarted() const;

    virtual void cancel();

    BWAPI::TilePosition depotPosition;

protected:
    MyUnit builder;
};
