#include <string>
#include <vector>

#include <Util/StringUtil.h>

#include "Config.h"

#include "WMode.h"

std::string screenshotFmt;

bool isCorrectVersion = true;
bool showWarn         = true;
bool serverEnabled    = true;

std::string readIni(const std::string& filename, const std::string& section, const std::string& key) {
  FILE* f = fopen(filename.c_str(), "rb");
  if (!f) return {};
  std::vector<char> data;
  fseek(f, 0, SEEK_END);
  long filesize = ftell(f);
  data.resize(filesize);
  fseek(f, 0, SEEK_SET);
  data.resize(data.size() * fread(data.data(), filesize, 1, f));
  fclose(f);
  bool correct_section = section.empty();
  const char* c = data.data();
  const char* e = c + data.size();
  auto whitespace = [&]() {
    switch (*c) {
    case ' ': case '\t': case '\v': case '\f': case '\r':
      return true;
    default:
      return false;
    }
  };
  while (c != e) {
    while (c != e && (whitespace() || *c == '\n')) ++c;
    if (c == e) break;
    if (*c == '#' || *c == ';') {
      while (c != e && *c != '\n') ++c;
    } else if (*c == '[') {
      correct_section = false;
      ++c;
      const char* n = c;
      while (c != e && *c != ']' && *c != '\n') {
        ++c;
      }
      if (!section.compare(0, section.size(), n, c - n)) correct_section = true;
      if (c != e) ++c;
    } else {
      const char* n = c;
      while (c != e && !whitespace() && *c != '=' && *c != '\n') {
        ++c;
      }
      if (c != e) {
        if (correct_section && !key.compare(0, key.size(), n, c - n)) {
          while (c != e && whitespace()) ++c;
          if (c != e && *c == '=') {
            ++c;
            while (c != e && whitespace()) ++c;
            n = c;
            while (c != e && *c != '\r' && *c != '\n') ++c;
            return std::string(n, c - n);
          }
        } else {
          while (c != e && *c != '\n') ++c;
        }
      }
    }
  }
  return {};
}

std::string readIniString(const std::string& section, const std::string& key, const std::string& default_, const std::string& filename) {
  auto s = readIni(filename, section, key);
  if (s.empty()) s = default_;
  return s;
}

int readIniInt(const std::string& section, const std::string& key, int default_, const std::string& filename) {
  auto s = readIni(filename, section, key);
  if (s.empty()) return default_;
  return (int)std::atoi(s.c_str());
}

//----------------------------- LOAD CONFIG FXNS ------------------------------------------
std::string envKeyName(const char *pszKey, const char *pszItem)
{
  return "BWAPI_CONFIG_" + Util::to_upper_copy(pszKey) + "__" + Util::to_upper_copy(pszItem);
}
std::string LoadConfigStringFromFile(const char *pszKey, const char *pszItem, const char *pszDefault)
{
  return readIniString(pszKey, pszItem, pszDefault ? pszDefault : "", configPath());
}
std::string LoadConfigString(const char *pszKey, const char *pszItem, const char *pszDefault)
{
  std::string envKey = envKeyName(pszKey, pszItem);
  if (char* v = std::getenv(envKey.c_str()))
    return v;
  else
    return LoadConfigStringFromFile(pszKey, pszItem, pszDefault);
}
// this version uppercase result string after loading, should be used for the most of enum-like strings
std::string LoadConfigStringUCase (const char *pszKey, const char *pszItem, const char *pszDefault)
{
  return Util::to_upper_copy(LoadConfigString(pszKey, pszItem, pszDefault));
}
int LoadConfigInt(const char *pszKey, const char *pszItem, const int iDefault)
{
  std::string envKey = envKeyName(pszKey, pszItem);
  if (char* v = std::getenv(envKey.c_str()))
    return std::stoi(v);
  else
    return readIniInt(pszKey, pszItem, iDefault, configPath().c_str());
}
void WriteConfig(const char *pszKey, const char *pszItem, const std::string& value)
{
  // avoid writing unless the value is actually different, because writing causes
  // an annoying popup when having the file open in e.g. notepad++
//  if (LoadConfigStringFromFile(pszKey, pszItem, "_NULL") != value)
//    WritePrivateProfileStringA(pszKey, pszItem, value.c_str(), configPath().c_str());
// fixme
}
void WriteConfig(const char *pszKey, const char *pszItem, int value)
{
  WriteConfig(pszKey, pszItem, std::to_string(value));
}

void InitPrimaryConfig()
{
  static bool isPrimaryConfigInitialized = false;
  // Return if already initialized
  if ( isPrimaryConfigInitialized )
    return;
  isPrimaryConfigInitialized = true;

  // ------------------------- GENERAL/GLOBAL CONFIG OPTIONS ----------------------------------
  // Get screenshot format
  screenshotFmt = LoadConfigString("starcraft", "screenshots", "gif");
  if ( !screenshotFmt.empty() )
    screenshotFmt.insert(0, ".");

  // Check if warning dialogs should be shown
  showWarn = LoadConfigStringUCase("config", "show_warnings", "YES") == "YES";

  // Check if shared memory should be enabled
  serverEnabled = LoadConfigStringUCase("config", "shared_memory", "ON") == "ON";

  // Get process count
  //gdwProcNum = getProcessCount("StarCraft.exe");

  // ------------------------- WMODE CONFIG OPTIONS ----------------------------------
  // Load windowed mode position and fullscreen setting
//  windowRect.left   = LoadConfigInt("window", "left");
//  windowRect.top    = LoadConfigInt("window", "top");
//  windowRect.right  = LoadConfigInt("window", "width");
//  windowRect.bottom = LoadConfigInt("window", "height");
//  switchToWMode     = LoadConfigStringUCase("window", "windowed", "OFF") == "ON";

//  // Limit minimum w-mode size
//  if ( windowRect.right < WMODE_MIN_WIDTH )
//    windowRect.right = WMODE_MIN_WIDTH;
//  if ( windowRect.bottom < WMODE_MIN_HEIGHT )
//    windowRect.bottom = WMODE_MIN_HEIGHT;

}

