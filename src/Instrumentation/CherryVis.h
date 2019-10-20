#pragma once

#include <BWAPI.h>

namespace CherryVis
{
    class LogWrapper
    {
    protected:
        std::ostringstream *os;
        int *refCount;
        int unitId;

    public:
        explicit LogWrapper(int unitId);

        LogWrapper(const LogWrapper &other);

        ~LogWrapper();

        template<typename T> LogWrapper &operator<<(T const &value)
        {
            (*os) << value;
            return *this;
        }

        LogWrapper &operator=(const LogWrapper &) = delete;
    };

    void initialize();

    void setBoardValue(const std::string& key, const std::string& value);

    void setBoardListValue(const std::string& key, std::vector<std::string> &values);

    void unitFirstSeen(BWAPI::Unit unit);

    LogWrapper log(int unitId = -1);

    LogWrapper log(BWAPI::Unit unit);

    void addHeatmap(const std::string& key, const std::vector<long> &data, int sizeX, int sizeY);

    void frameEnd(int frame);

    void gameEnd();
}
