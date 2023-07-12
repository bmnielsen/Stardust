// Written by jaj22

#include <cstdint>
#include <array>
#include <algorithm>
#include <assert.h>
#include <BWAPI.h>
using namespace BWAPI;

#include "BaseFinder.h"

namespace BaseFinder
{

    static int mapx, mapy, scanw;
    static std::vector<bool> walkgrid;            // true if tile blocked by terrain
    static std::vector<bool> resblock;            // true if blocked (for depot top-left) by mineral proximity
    static std::vector<int16_t> resval;        // value of tile (for depot top-left)

    inline int TILEOFF(int x, int y) { return x + 1 + (y + 1)*(mapx + 2); }

    // simplified version for standalone, doesn't bother with bitmask
    static void MakeWalkGrid()
    {
        walkgrid.assign(scanw*(mapy + 2), true);
        int curoff = mapx + 3;            // 1,1 in walkgrid = 0,0 in bwapi
        for (int y = 0; y < mapy; y++, curoff += 2) {
            for (int x = 0; x < mapx; x++, curoff++) {
                walkgrid[curoff] = false;            // assume open unless otherwise
                for (int ym = 0; ym < 4; ym++) for (int xm = 0; xm < 4; xm++) {
                    if (Broodwar->isWalkable(x * 4 + xm, y * 4 + ym)) continue;
                    walkgrid[curoff] = true; break;
                }
            }
        }
    }

    // mark area around resource for depot top-left blocking
    static void MarkResBlock(TilePosition p, int tw, int th)
    {
        TilePosition p1(std::max(0, p.x - 6), std::max(0, p.y - 5));                    // offset by base size
        TilePosition p2(std::min(mapx - 1, p.x + 2 + tw), std::min(mapy - 1, p.y + 2 + th));

        for (int y = p1.y; y <= p2.y; y++) {
            int off = TILEOFF(p1.x, y);
            for (int x = p1.x; x <= p2.x; x++, off++) resblock[off] = true;
        }
    }

    // midoff is the offset of the starting tile
    // distrow is a distance lookup table, 0 = nothing extra
    // end is size of end sections
    // mid is size of mid section
    // inc is increment either downwards or right
    static int MarkRow(int midoff, const int *distrow, int mid, int end, int inc, int valmod)
    {
        int writes = 0;
        for (int i = 1, roff = midoff + inc; i <= end; i++, roff += inc, ++writes) {
            if (walkgrid[roff]) break;        // blocked tile, don't continue in this dir
            resval[roff] += valmod * distrow[i];
        }
        for (int i = 1 - mid, roff = midoff; i <= end; i++, roff -= inc, ++writes) {
            if (walkgrid[roff]) break;        // blocked tile, don't continue in this dir
            resval[roff] += valmod * distrow[std::max(i, 0)];
        }
        return writes;
    }

    // inputs, p = top left tile, tw = width, th = height, valmod is multiplier for value.
    static void MarkBorderValue(TilePosition p, int tw, int th, int valmod)
    {
        // first 24 entries should be unused. formula sqrt(900/(x*x+y*y)), 3,0 gives val 10
        const int sqrtarr[64] ={ 0, 300, 150, 100, 75, 60, 50, 42, 300, 212, 134, 94, 72, 58, 49, 42, 150, 134, 106, 83, 67, 55, 47, 41,
            100, 94, 83, 70, 60, 51, 44, 39, 75, 72, 67, 60, 53, 46, 41, 37, 60, 58, 55, 51, 46, 42, 38, 34, 50, 49, 47, 44, 41, 38, 35, 32, 42, 42, 41, 39, 37, 34, 32, 30 };
        int coff = TILEOFF(p.x + tw - 1, p.y + th - 1);

        bool c = false; for (int i = th; i < th + 6; i++) if (walkgrid[coff - i * scanw]) { c = true; break; }
        if (!c) for (int s = 3; s < 7; s++) if (!MarkRow(coff - (s + 2 + th)*scanw, sqrtarr + s * 8, tw + 3, s, +1, valmod)) break;        // top

        c = false; for (int i = 1; i < 5; i++) if (walkgrid[coff + i * scanw]) { c = true; break; }
        if (!c) for (int s = 3; s < 7; s++) if (!MarkRow(coff + (s + 1)*scanw, sqrtarr + s * 8, tw + 3, s, +1, valmod)) break;        // bot

        c = false; for (int i = tw; i < tw + 7; i++) if (walkgrid[coff - i]) { c = true; break; }
        if (!c) for (int s = 3; s < 7; s++) if (!MarkRow(coff - (s + 3 + tw), sqrtarr + s * 8, th + 2, s + 1, scanw, valmod)) break;        // left

        c = false; for (int i = 1; i < 5; i++) if (walkgrid[coff + i]) { c = true; break; }
        if (!c) for (int s = 3; s < 7; s++) if (!MarkRow(coff + (s + 1), sqrtarr + s * 8, th + 2, s + 1, scanw, valmod)) break;        // right
    }



    std::vector<Base> bases;

    const std::vector<Base> &GetBases() { return bases; }

    const int BASE_MIN = 400;        // mininum resource value to consider for a base
    const int MINERAL_MIN = 450;    // consider minerals smaller than this to be blockers
    const int INC_DIST = 9 * 32;        // mid-to-mid pixel distance for including resources in a base

    void Init()
    {
        mapx = Broodwar->mapWidth();
        mapy = Broodwar->mapHeight();
        scanw = mapx + 2;
        MakeWalkGrid();

        bases.clear();
        resblock.assign((mapx + 2)*(mapy + 2), false);
        resval.assign((mapx + 2)*(mapy + 2), 0);
        std::vector<Unit> res; res.reserve(200);

        for (Unit u : Broodwar->getStaticMinerals())
        {            
            if (u->getInitialResources() < MINERAL_MIN) continue;
            TilePosition p = u->getInitialTilePosition();
            MarkResBlock(p, 2, 1);
            MarkBorderValue(p, 2, 1, 1);
            res.push_back(u);
        }

        for (Unit u : Broodwar->getStaticGeysers())
        {
            if (!u->getInitialResources()) continue;
            TilePosition p = u->getInitialTilePosition();
            MarkResBlock(p, 4, 2);
            MarkBorderValue(p, 4, 2, 3);
            res.push_back(u);
        }

        // ok, then find all values above some threshold, sort them
        std::vector<int> potbase; potbase.reserve(scanw*mapy / 8);
        for (int off = scanw; off < scanw*(mapy + 1); off++)
            if (resval[off] > BASE_MIN && !resblock[off]) potbase.push_back(off);
        std::sort(potbase.begin(), potbase.end(), [](int a, int b) { return resval[b] < resval[a]; });

        // Make some fucking bases
        for (int off : potbase)
        {
            if (resval[off] <= BASE_MIN || resblock[off]) continue;        // can get wiped

            bases.push_back({});
            Base &base = bases.back();
            base.tpos = TilePosition((off - mapx - 3) % scanw, (off - mapx - 3) / scanw);
            Position &bp = base.pos = Position(base.tpos.x * 32 + 64, base.tpos.y * 32 + 48);

            if (!(base.tpos + TilePosition(3, 2)).isValid())
                continue;

            for (size_t i = 0; i < res.size(); ) {
                Position diff = bp - res[i]->getInitialPosition();
                if (diff.x*diff.x + diff.y*diff.y > INC_DIST*INC_DIST) { ++i; continue; }

                if (res[i]->getInitialType().isMineralField()) {
                    base.minerals.push_back(res[i]);
                    MarkBorderValue(res[i]->getInitialTilePosition(), 2, 1, -1);
                }
                else {
                    base.geysers.push_back(res[i]);
                    MarkBorderValue(res[i]->getInitialTilePosition(), 4, 2, -3);
                }
                res[i] = res.back(); res.pop_back();
            }
        }
    }

    void DrawStuff()    // could add min/max tiles in here?
    {
        for (Base &base : bases) {
            Position bpos(base.tpos.x * 32 + 64, base.tpos.y * 32 + 48);
            for (Unit u : base.minerals)
                Broodwar->drawLineMap(u->getInitialPosition(), bpos, Colors::Blue);
            for (Unit u : base.geysers)
                Broodwar->drawLineMap(u->getInitialPosition(), bpos, Colors::Green);
        }
    }
}
