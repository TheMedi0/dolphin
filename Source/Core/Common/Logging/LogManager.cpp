// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstring>
#include <mutex>
#include <ostream>
#include <string>

#include "Common/BitSet.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

constexpr size_t MAX_MSGLEN = 1024;

class FileLogListener : public LogListener
{
public:
  FileLogListener(const std::string& filename)
  {
    File::OpenFStream(m_logfile, filename, std::ios::app);
    SetEnable(true);
  }

  void Log(LogTypes::LOG_LEVELS, const char* msg) override
  {
    if (!IsEnabled() || !IsValid())
      return;

    std::lock_guard<std::mutex> lk(m_log_lock);
    m_logfile << msg << std::flush;
  }

  bool IsValid() const { return m_logfile.good(); }
  bool IsEnabled() const { return m_enable; }
  void SetEnable(bool enable) { m_enable = enable; }
  // const char* GetName() const { return "file"; }
private:
  std::mutex m_log_lock;
  std::ofstream m_logfile;
  bool m_enable;
};

class LogContainer
{
public:
  LogContainer(const std::string& short_name, const std::string& full_name, bool enable = false)
      : m_full_name(full_name), m_short_name(short_name), m_enable(enable)
  {
  }

  std::string GetShortName() const { return m_short_name; }
  std::string GetFullName() const { return m_full_name; }
  void AddListener(LogListener::LISTENER id) { m_listener_ids[id] = 1; }
  void RemoveListener(LogListener::LISTENER id) { m_listener_ids[id] = 0; }
  bool IsEnabled() const { return m_enable; }
  void SetEnable(bool enable) { m_enable = enable; }
  bool HasListeners() const { return bool(m_listener_ids); }
  typedef class BitSet32::Iterator iterator;
  iterator begin() const { return m_listener_ids.begin(); }
  iterator end() const { return m_listener_ids.end(); }
private:
  std::string m_full_name;
  std::string m_short_name;
  bool m_enable;
  BitSet32 m_listener_ids;
};

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file, int line,
                const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  if (LogManager::GetInstance())
    LogManager::GetInstance()->Log(level, type, file, line, fmt, args);
  va_end(args);
}

static size_t DeterminePathCutOffPoint()
{
  constexpr const char* pattern = DIR_SEP "Source" DIR_SEP "Core" DIR_SEP;
  size_t pos = std::string(__FILE__).find(pattern);
  if (pos != std::string::npos)
    return pos + strlen(pattern);
  return 0;
}

LogManager::LogManager()
{
  // create log containers
  m_log[LogTypes::ACTIONREPLAY] = new LogContainer("ActionReplay", "ActionReplay");
  m_log[LogTypes::AUDIO] = new LogContainer("Audio", "Audio Emulator");
  m_log[LogTypes::AUDIO_INTERFACE] = new LogContainer("AI", "Audio Interface (AI)");
  m_log[LogTypes::BOOT] = new LogContainer("BOOT", "Boot");
  m_log[LogTypes::COMMANDPROCESSOR] = new LogContainer("CP", "CommandProc");
  m_log[LogTypes::COMMON] = new LogContainer("COMMON", "Common");
  m_log[LogTypes::CONSOLE] = new LogContainer("CONSOLE", "Dolphin Console");
  m_log[LogTypes::CORE] = new LogContainer("CORE", "Core");
  m_log[LogTypes::DISCIO] = new LogContainer("DIO", "Disc IO");
  m_log[LogTypes::DSPHLE] = new LogContainer("DSPHLE", "DSP HLE");
  m_log[LogTypes::DSPLLE] = new LogContainer("DSPLLE", "DSP LLE");
  m_log[LogTypes::DSP_MAIL] = new LogContainer("DSPMails", "DSP Mails");
  m_log[LogTypes::DSPINTERFACE] = new LogContainer("DSP", "DSPInterface");
  m_log[LogTypes::DVDINTERFACE] = new LogContainer("DVD", "DVD Interface");
  m_log[LogTypes::DYNA_REC] = new LogContainer("JIT", "Dynamic Recompiler");
  m_log[LogTypes::EXPANSIONINTERFACE] = new LogContainer("EXI", "Expansion Interface");
  m_log[LogTypes::FILEMON] = new LogContainer("FileMon", "File Monitor");
  m_log[LogTypes::GDB_STUB] = new LogContainer("GDB_STUB", "GDB Stub");
  m_log[LogTypes::GPFIFO] = new LogContainer("GP", "GPFifo");
  m_log[LogTypes::HOST_GPU] = new LogContainer("Host GPU", "Host GPU");
  m_log[LogTypes::IOS] = new LogContainer("IOS", "IOS");
  m_log[LogTypes::IOS_DI] = new LogContainer("IOS_DI", "IOS - Drive Interface");
  m_log[LogTypes::IOS_ES] = new LogContainer("IOS_ES", "IOS - ETicket Services");
  m_log[LogTypes::IOS_FILEIO] = new LogContainer("IOS_FILEIO", "IOS - FileIO");
  m_log[LogTypes::IOS_SD] = new LogContainer("IOS_SD", "IOS - SDIO");
  m_log[LogTypes::IOS_SSL] = new LogContainer("IOS_SSL", "IOS - SSL");
  m_log[LogTypes::IOS_STM] = new LogContainer("IOS_STM", "IOS - State Transition Manager");
  m_log[LogTypes::IOS_NET] = new LogContainer("IOS_NET", "IOS - Network");
  m_log[LogTypes::IOS_USB] = new LogContainer("IOS_USB", "IOS - USB");
  m_log[LogTypes::IOS_WC24] = new LogContainer("IOS_WC24", "IOS - WiiConnect24");
  m_log[LogTypes::IOS_WIIMOTE] = new LogContainer("IOS_WIIMOTE", "IOS - Wii Remote");
  m_log[LogTypes::MASTER_LOG] = new LogContainer("*", "Master Log");
  m_log[LogTypes::MEMCARD_MANAGER] = new LogContainer("MemCard Manager", "MemCard Manager");
  m_log[LogTypes::MEMMAP] = new LogContainer("MI", "MI & memmap");
  m_log[LogTypes::NETPLAY] = new LogContainer("NETPLAY", "Netplay");
  m_log[LogTypes::OSHLE] = new LogContainer("HLE", "HLE");
  m_log[LogTypes::OSREPORT] = new LogContainer("OSREPORT", "OSReport");
  m_log[LogTypes::PAD] = new LogContainer("PAD", "Pad");
  m_log[LogTypes::PIXELENGINE] = new LogContainer("PE", "PixelEngine");
  m_log[LogTypes::PROCESSORINTERFACE] = new LogContainer("PI", "ProcessorInt");
  m_log[LogTypes::POWERPC] = new LogContainer("PowerPC", "IBM CPU");
  m_log[LogTypes::SERIALINTERFACE] = new LogContainer("SI", "Serial Interface (SI)");
  m_log[LogTypes::SP1] = new LogContainer("SP1", "Serial Port 1");
  m_log[LogTypes::VIDEO] = new LogContainer("Video", "Video Backend");
  m_log[LogTypes::VIDEOINTERFACE] = new LogContainer("VI", "Video Interface (VI)");
  m_log[LogTypes::WIIMOTE] = new LogContainer("Wiimote", "Wiimote");
  m_log[LogTypes::WII_IPC] = new LogContainer("WII_IPC", "WII IPC");

  RegisterListener(LogListener::FILE_LISTENER,
                   new FileLogListener(File::GetUserPath(F_MAINLOG_IDX)));
  RegisterListener(LogListener::CONSOLE_LISTENER, new ConsoleListener());

  IniFile ini;
  ini.Load(File::GetUserPath(F_LOGGERCONFIG_IDX));
  IniFile::Section* logs = ini.GetOrCreateSection("Logs");
  IniFile::Section* options = ini.GetOrCreateSection("Options");
  bool write_file;
  bool write_console;
  bool write_window;
  options->Get("WriteToFile", &write_file, false);
  options->Get("WriteToConsole", &write_console, true);
  options->Get("WriteToWindow", &write_window, true);

  // Set up log listeners
  int verbosity;
  options->Get("Verbosity", &verbosity, 0);

  // Ensure the verbosity level is valid
  if (verbosity < 1)
    verbosity = 1;
  if (verbosity > MAX_LOGLEVEL)
    verbosity = MAX_LOGLEVEL;

  SetLogLevel(static_cast<LogTypes::LOG_LEVELS>(verbosity));

  for (LogContainer* container : m_log)
  {
    bool enable;
    logs->Get(container->GetShortName(), &enable, false);
    container->SetEnable(enable);
    if (enable && write_file)
      container->AddListener(LogListener::FILE_LISTENER);
    if (enable && write_console)
      container->AddListener(LogListener::CONSOLE_LISTENER);
    if (enable && write_window)
      container->AddListener(LogListener::LOG_WINDOW_LISTENER);
  }

  m_path_cutoff_point = DeterminePathCutOffPoint();
}

LogManager::~LogManager()
{
  for (LogContainer* container : m_log)
    delete container;

  // The log window listener pointer is owned by the GUI code.
  delete m_listeners[LogListener::CONSOLE_LISTENER];
  delete m_listeners[LogListener::FILE_LISTENER];
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char* file,
                     int line, const char* format, va_list args)
{
  return LogWithFullPath(level, type, file + m_path_cutoff_point, line, format, args);
}

void LogManager::LogWithFullPath(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type,
                                 const char* file, int line, const char* format, va_list args)
{
  char temp[MAX_MSGLEN];
  LogContainer* log = m_log[type];

  if (!log->IsEnabled() || level > GetLogLevel() || !log->HasListeners())
    return;

  CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

  std::string msg = StringFromFormat(
      "%s %s:%u %c[%s]: %s\n", Common::Timer::GetTimeFormatted().c_str(), file, line,
      LogTypes::LOG_LEVEL_TO_CHAR[(int)level], log->GetShortName().c_str(), temp);

  for (auto listener_id : *log)
    if (m_listeners[listener_id])
      m_listeners[listener_id]->Log(level, msg.c_str());
}

LogTypes::LOG_LEVELS LogManager::GetLogLevel() const
{
  return m_level;
}

void LogManager::SetLogLevel(LogTypes::LOG_LEVELS level)
{
  m_level = level;
}

void LogManager::SetEnable(LogTypes::LOG_TYPE type, bool enable)
{
  m_log[type]->SetEnable(enable);
}

bool LogManager::IsEnabled(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level) const
{
  return m_log[type]->IsEnabled() && GetLogLevel() >= level;
}

std::string LogManager::GetShortName(LogTypes::LOG_TYPE type) const
{
  return m_log[type]->GetShortName();
}

std::string LogManager::GetFullName(LogTypes::LOG_TYPE type) const
{
  return m_log[type]->GetFullName();
}

void LogManager::RegisterListener(LogListener::LISTENER id, LogListener* listener)
{
  m_listeners[id] = listener;
}

void LogManager::AddListener(LogTypes::LOG_TYPE type, LogListener::LISTENER id)
{
  m_log[type]->AddListener(id);
}

void LogManager::RemoveListener(LogTypes::LOG_TYPE type, LogListener::LISTENER id)
{
  m_log[type]->RemoveListener(id);
}

// Singleton. Ugh.
static LogManager* s_log_manager;

LogManager* LogManager::GetInstance()
{
  return s_log_manager;
}

void LogManager::Init()
{
  s_log_manager = new LogManager();
}

void LogManager::Shutdown()
{
  delete s_log_manager;
  s_log_manager = nullptr;
}
