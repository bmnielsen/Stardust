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
    loadModule("C:\\Dev\\BW\\Stardust-dev-tournament\\out\\src\\RelWithDebInfo\\Stardust.dll");

    while (true)
    {
    }
}
