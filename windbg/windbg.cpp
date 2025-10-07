#include <iostream>

#include "windows.h"

namespace
{
    void loadModule(const std::string &path)
    {
        std::cout << "Loading module from " << path << "..." << std::endl;

        HMODULE hMod = LoadLibraryA(path.c_str());

        std::cout << "Module loaded! Address: " << std::hex << (unsigned long) hMod << std::endl;
    }
}

int main()
{
    loadModule("C:\\Users\\bruce\\AppData\\Roaming\\scbw\\build\\Stardust-private\\out\\src\\RelWithDebInfo\\Stardust.dll");

    while (true)
    {
    }
}
