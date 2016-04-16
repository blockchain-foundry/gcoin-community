// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Server/client environment: argument handling, config file parsing,
 * logging, thread wrappers
 */
#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "compat.h"
#include "tinyformat.h"
#include "utiltime.h"

#include <exception>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/thread/exceptions.hpp>

/** Signals for translation. */
class CTranslationInterface
{
public:
    /** Translate a message to the native language of the user. */
    boost::signals2::signal<std::string (const char* psz)> Translate;
};
#ifdef ENABLE_SYSTEMD_JOURNAL
#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>
#endif

extern std::map<std::string, std::string> mapArgs;
extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern bool fDebug;
extern bool fPrintToConsole;
extern bool fPrintToDebugLog;
extern bool fSystemdJournal;
extern bool fServer;
extern std::string strMiscWarning;
extern bool fLogTimestamps;
extern bool fLogIPs;
extern volatile bool fReopenDebugLog;
extern CTranslationInterface translationInterface;

/**
 * Translation function: Call Translate signal on UI interface, which returns a boost::optional result.
 * If no translation slot is registered, nothing is returned, and simply return the input.
 */
inline std::string _(const char* psz)
{
    boost::optional<std::string> rv = translationInterface.Translate(psz);
    return rv ? (*rv) : psz;
}

void SetupEnvironment();

/** Return true if log accepts specified category */
bool LogAcceptCategory(const char* category);
/** Send a string to the log output */
int LogPrintStr(const std::string &str);

#define LogPrintf(...) LogPrint(NULL, __VA_ARGS__)

#define STRINGIFY_ARG(x) #x
#define STRINGIFY(x) STRINGIFY_ARG(x)
#define LogPrint(...) LogPrintWithLocation(__FILE__, STRINGIFY(__LINE__), \
    __func__, BOOST_CURRENT_FUNCTION, __VA_ARGS__)
/* Unfortunately, the name 'error' is too generic, so the macro can replace
 * things declared in system headers or boost headers if it is put here.
 * Therefore, the definition of 'error' macro is in utilerror.h, and all files
 * that want to call 'error' must include it.
 */

/* When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#ifdef ENABLE_SYSTEMD_JOURNAL
#define MAKE_ERROR_AND_LOG_FUNC(n)                                        \
    /**   Print to debug.log if -debug=category switch is given OR category is NULL. */ \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline int LogPrintWithLocation(const char* file,                  \
        const char* line, const char* func, const char* pretty_func,          \
        const char* category, const char* format, TINYFORMAT_VARARGS(n))      \
    {                                                                         \
        if (!LogAcceptCategory(category)) return 0;                           \
        std::string msg_str = tfm::format(format, TINYFORMAT_PASSARGS(n));    \
        if (fSystemdJournal) {                                                \
            const char *msg_c_str = msg_str.c_str();                          \
            int msg_str_len = msg_str.size();                                 \
            if (msg_str[msg_str_len - 1] == '\n')                             \
                msg_str_len--;                                                \
            if (category == NULL)                                             \
                category = "";                                                \
            sd_journal_send(                                                  \
                "MESSAGE=%.*s", msg_str_len, msg_c_str,                       \
                "PRIORITY=%d", LOG_INFO,                                      \
                "CATEGORY=%s", category,                                      \
                "CODE_FILE=%s", file,                                         \
                "CODE_LINE=%s", line,                                         \
                "CODE_FUNC=%s", func,                                         \
                "CODE_PRETTY_FUNCTION=%s", pretty_func,                       \
                NULL);                                                        \
        }                                                                     \
        return LogPrintStr(msg_str);                                          \
    }                                                                         \
    /*   Log error and return false */                                        \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline bool errorWithLocation(const char* file,                    \
        const char* line, const char* func, const char* pretty_func,          \
        const char* format, TINYFORMAT_VARARGS(n))                            \
    {                                                                         \
        std::string msg_str = tfm::format(format, TINYFORMAT_PASSARGS(n));    \
        if (fSystemdJournal) {                                                \
            const char *msg_c_str = msg_str.c_str();                          \
            int msg_str_len = msg_str.size();                                 \
            if (msg_str[msg_str_len - 1] == '\n')                             \
                msg_str_len--;                                                \
            sd_journal_send(                                                  \
                "MESSAGE=%.*s", msg_str_len, msg_c_str,                       \
                "PRIORITY=%d", LOG_ERR,                                       \
                "CODE_FILE=%s", file,                                         \
                "CODE_LINE=%s", line,                                         \
                "CODE_FUNC=%s", func,                                         \
                "CODE_PRETTY_FUNCTION=%s", pretty_func,                       \
                NULL);                                                        \
        }                                                                     \
        LogPrintStr("ERROR: " + msg_str + "\n");                              \
        return false;                                                         \
    }
#else
#define MAKE_ERROR_AND_LOG_FUNC(n)                                        \
    /*   Print to debug.log if -debug=category switch is given OR category is NULL. */ \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline int LogPrintWithLocation(const char* file,                  \
        const char* line, const char* func, const char* pretty_func,          \
        const char* category, const char* format, TINYFORMAT_VARARGS(n))      \
    {                                                                         \
        if(!LogAcceptCategory(category)) return 0;                            \
        return LogPrintStr(tfm::format(format, TINYFORMAT_PASSARGS(n))); \
    }                                                                         \
    /**   Log error and return false */                                        \
    template<TINYFORMAT_ARGTYPES(n)>                                          \
    static inline bool errorWithLocation(const char* file,                    \
        const char* line, const char* func, const char* pretty_func,          \
        const char* format, TINYFORMAT_VARARGS(n))                            \
    {                                                                         \
        LogPrintStr("ERROR: " + tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n"); \
        return false;                                                         \
    }
#endif // ENABLE_SYSTEMD_JOURNAL

TINYFORMAT_FOREACH_ARGNUM(MAKE_ERROR_AND_LOG_FUNC)

/**
 * Zero-arg versions of logging and error, these are not covered by
 * TINYFORMAT_FOREACH_ARGNUM
 */
static inline int LogPrintWithLocation(const char* file, const char* line,
    const char* func, const char* pretty_func, const char* category, const char* format)
{
    if(!LogAcceptCategory(category)) return 0;
#ifdef ENABLE_SYSTEMD_JOURNAL
    if (fSystemdJournal) {
        int format_len = strlen(format);
        if (format[format_len - 1] == '\n')
            format_len--;
        if (category == NULL)
            category = "";
        sd_journal_send(
            "MESSAGE=%.*s", format_len, format,
            "PRIORITY=%d", LOG_INFO,
            "CATEGORY=%s", category,
            "CODE_FILE=%s", file,
            "CODE_LINE=%s", line,
            "CODE_FUNC=%s", func,
            "CODE_PRETTY_FUNCTION=%s", pretty_func,
            NULL);
    }
#endif // ENABLE_SYSTEMD_JOURNAL
    return LogPrintStr(format);
}
static inline bool errorWithLocation(const char* file, const char* line,
    const char* func, const char* pretty_func, const char* format)
{
#ifdef ENABLE_SYSTEMD_JOURNAL
    if (fSystemdJournal) {
        int format_len = strlen(format);
        if (format[format_len - 1] == '\n')
            format_len--;
        sd_journal_send(
            "MESSAGE=%.*s", format_len, format,
            "PRIORITY=%d", LOG_ERR,
            "CODE_FILE=%s", file,
            "CODE_LINE=%s", line,
            "CODE_FUNC=%s", func,
            "CODE_PRETTY_FUNCTION=%s", pretty_func,
            NULL);
    }
#endif // ENABLE_SYSTEMD_JOURNAL
    LogPrintStr(std::string("ERROR: ") + format + "\n");
    return false;
}

void PrintExceptionContinue(const std::exception *pex, const char* pszThread);
void ParseParameters(int argc, const char*const argv[]);
void FileCommit(FILE *fileout);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);
bool RenameOver(boost::filesystem::path src, boost::filesystem::path dest);
bool TryCreateDirectory(const boost::filesystem::path& p);
boost::filesystem::path GetDefaultDataDir();
const boost::filesystem::path &GetDataDir(bool fNetSpecific = true);
void ClearDatadirCache();
boost::filesystem::path GetConfigFile();
#ifndef WIN32
boost::filesystem::path GetPidFile();
void CreatePidFile(const boost::filesystem::path &path, pid_t pid);
#endif
void ReadConfigFile(std::map<std::string, std::string>& mapSettingsRet, std::map<std::string, std::vector<std::string> >& mapMultiSettingsRet);
#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
boost::filesystem::path GetTempPath();
void ShrinkDebugFile();
void runCommand(std::string strCommand);

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);

/**
 * Format a string to be used as group of options in help messages
 *
 * @param message Group name (e.g. "RPC server options:")
 * @return the formatted string
 */
std::string HelpMessageGroup(const std::string& message);

/**
 * Format a string to be used as option description in help messages
 *
 * @param option Option message (e.g. "-rpcuser=<user>")
 * @param message Option description (e.g. "Username for JSON-RPC connections")
 * @return the formatted string
 */
std::string HelpMessageOpt(const std::string& option, const std::string& message);

void SetThreadPriority(int nPriority);
void RenameThread(const char* name);

/**
 * .. and a wrapper that just calls func once
 */
template <typename Callable> void TraceThread(const char* name,  Callable func)
{
    std::string s = strprintf("bitcoin-%s", name);
    RenameThread(s.c_str());
    try
    {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("%s thread interrupt\n", name);
        throw;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, name);
        throw;
    }
    catch (...) {
        PrintExceptionContinue(NULL, name);
        throw;
    }
}

#endif // BITCOIN_UTIL_H
