#pragma once

#include <BWAPI.h>

namespace CherryVis
{
    class LogWrapper
    {
    private:
        LogWrapper& operator =(const LogWrapper&) = delete;

    protected:
        std::ostringstream* os;
        int* refCount;
        int unitId;

    public:
        LogWrapper(int unitId);
        LogWrapper(const LogWrapper& other);
        ~LogWrapper();

        template<typename T> LogWrapper& operator<<(T const& value)
        {
            (*os) << value;
            return *this;
        }
    };

    void initialize();

    void setBoardValue(std::string key, std::string value);
    void setBoardListValue(std::string key, std::vector<std::string> & values);

    void unitFirstSeen(BWAPI::Unit unit);

    LogWrapper log(int unitId = -1);
    LogWrapper log(BWAPI::Unit unit);

    void addHeatmap(std::string key, const std::vector<long> & data, int sizeX, int sizeY);

    void frameEnd(int frame);
    void gameEnd();
}
