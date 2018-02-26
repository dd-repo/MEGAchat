#include "megaLoggerApplication.h"
#include <iostream>
using namespace std;
using namespace megachat;

MegaLoggerApplication::MegaLoggerApplication(const char *filename)
{
    logConsole=false;
    testlog.open(filename, ios::out | ios::app);
}

MegaLoggerApplication::~MegaLoggerApplication()
{
    testlog.close();
}

void MegaLoggerApplication::log(const char *time, int loglevel, const char *source, const char *message)
{
    testlog << message << endl;
    if(logConsole)
       cout << message << endl;
}

void MegaLoggerApplication::postLog(const char *message)
{
    testlog << message << endl;
    if(logConsole)
       cout << message << endl;
}

void MegaLoggerApplication::log(int loglevel, const char *message)
{
    string levelStr;
    switch (loglevel)
    {
    case MegaChatApi::LOG_LEVEL_ERROR: levelStr = "err"; break;
        case MegaChatApi::LOG_LEVEL_WARNING: levelStr = "warn"; break;
        case MegaChatApi::LOG_LEVEL_INFO: levelStr = "info"; break;
        case MegaChatApi::LOG_LEVEL_VERBOSE: levelStr = "verb"; break;
        case MegaChatApi::LOG_LEVEL_DEBUG: levelStr = "debug"; break;
        case MegaChatApi::LOG_LEVEL_MAX: levelStr = "debug-verbose"; break;
        default: levelStr = ""; break;
    }
    testlog  << message;
    if(logConsole)
       cout << message << endl;
}

bool MegaLoggerApplication::getLogConsole() const
{
    return logConsole;
}

void MegaLoggerApplication::setLogConsole(bool logConsole)
{
    logConsole = logConsole;
}