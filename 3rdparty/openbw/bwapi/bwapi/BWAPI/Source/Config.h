#pragma once
#include <cstdlib>
#include <string>

// Functions
std::string LoadConfigString(const char *pszKey, const char *pszItem, const char *pszDefault = NULL);
std::string LoadConfigStringUCase (const char *pszKey, const char *pszItem, const char *pszDefault = NULL);
int LoadConfigInt(const char *pszKey, const char *pszItem, const int iDefault = 0);
void WriteConfig(const char *pszKey, const char *pszItem, const std::string& value);
void WriteConfig(const char *pszKey, const char *pszItem, int value);

void InitPrimaryConfig();

// The starcraft instance's root directory
inline std::string installPath()
{
  return "./";
}

// The config directory
inline std::string configDir()
{
  return "./bwapi-data/";
}
// The bwapi.ini file in the config directory. Can be overridden by environment variable BWAPI_CONFIG_INI
inline std::string configPath()
{
  char* env = std::getenv("BWAPI_CONFIG_INI");
  return env ? env : configDir() + "bwapi.ini";
}
extern std::string screenshotFmt;

extern bool isCorrectVersion;
extern bool showWarn;
extern bool serverEnabled;
//extern unsigned gdwProcNum;
