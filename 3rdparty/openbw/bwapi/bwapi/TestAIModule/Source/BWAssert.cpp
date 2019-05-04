#include "BWAssert.h"
#include <fstream>
#include <stdarg.h>
#include <sys/stat.h>
#include <BWAPI.h>

#define BUFFER_SIZE 1024

void log(const char* format, ...)
{
  char buffer[BUFFER_SIZE];

  va_list ap;
  va_start(ap, format);
  vsnprintf(buffer, BUFFER_SIZE, format, ap);
  va_end(ap);

  FILE *outfile;
  BWAPI::Broodwar->printf(buffer);
  outfile = fopen("bwapi-data/logs/TestModule - Failed Asserts.log", "a+");
  if (outfile)
  {
    if (outfile)
    {
      fprintf(outfile, "%s\n", buffer);
      fclose(outfile);
    }
  }
}
