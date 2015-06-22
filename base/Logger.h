#ifndef __BT_LOGGER_H
#define __BT_LOGGER_H

#include <string>
#include <sstream>
#include <errno.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <deque>


//! logger ID type. DO NOT TOUCH
typedef int LoggerId;

//! the invalid logger id. DO NOT TOUCH
const int LOG4Z_INVALID_LOGGER_ID = -1;

//! the main logger id. DO NOT TOUCH
//! can use this id to set the main logger's attribute.
//! example:
//! ILog4zManager::getPtr()->setLoggerLevel(LOG4Z_MAIN_LOGGER_ID, LOG_LEVEL_WARN);
//! ILog4zManager::getPtr()->setLoggerDisplay(LOG4Z_MAIN_LOGGER_ID, false);
const int LOG4Z_MAIN_LOGGER_ID = 0;

//! the main logger name. DO NOT TOUCH
const char*const LOG4Z_MAIN_LOGGER_KEY = "Main";

#define LOG4Z_FORMAT_INPUT_ENABLE

//! LOG Level
enum ENUM_LOG_LEVEL
{
	LOG_LEVEL_TRACE = 0,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_ALARM,
	LOG_LEVEL_FATAL,
};

//////////////////////////////////////////////////////////////////////////
//! -----------------default logger config, can change on this.-----------
//////////////////////////////////////////////////////////////////////////
//! the max logger count.
const int LOG4Z_LOGGER_MAX = 10;
//! the max log content length.
const int LOG4Z_LOG_BUF_SIZE = 2048;

//! all logger synchronous output or not
const bool LOG4Z_ALL_SYNCHRONOUS_OUTPUT = false;
//! all logger synchronous display to the windows debug output
const bool LOG4Z_ALL_DEBUGOUTPUT_DISPLAY = false;

//! default logger output file.
const char* const LOG4Z_DEFAULT_PATH = "./log/";
//! default log filter level
const int LOG4Z_DEFAULT_LEVEL = LOG_LEVEL_DEBUG;
//! default logger display
const bool LOG4Z_DEFAULT_DISPLAY = true;
//! default logger output to file
const bool LOG4Z_DEFAULT_OUTFILE = true;
//! default logger month dir used status
const bool LOG4Z_DEFAULT_MONTHDIR = false;
//! default logger output file limit size, unit M byte.
const int LOG4Z_DEFAULT_LIMITSIZE = 100;
//! default logger show suffix (file name and line number) 
const bool LOG4Z_DEFAULT_SHOWSUFFIX = true;

///////////////////////////////////////////////////////////////////////////
//! -----------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////

namespace bt {

//! log4z class
class ILog4zManager
{
public:
	ILog4zManager(){};
	virtual ~ILog4zManager(){};

	//! Log4z Singleton
	
	static ILog4zManager * getInstance();
	inline static ILog4zManager & getRef(){return *getInstance();}
	inline static ILog4zManager * getPtr(){return getInstance();}

	//! Config or overwrite configure
	//! Needs to be called before ILog4zManager::Start,, OR Do not call.
	virtual bool config(const char * configPath) = 0;
	virtual bool configFromString(const char * configContent) = 0;

	//! Create or overwrite logger.
	//! Needs to be called before ILog4zManager::Start, OR Do not call.
	virtual LoggerId createLogger(const char* key) = 0;

	//! Start Log Thread. This method can only be called once by one process.
	virtual bool start() = 0;

	//! Default the method will be calling at process exit auto.
	//! Default no need to call and no recommended.
	virtual bool stop() = 0;

	//! Find logger. thread safe.
	virtual LoggerId findLogger(const char* key) =0;

	//pre-check the log filter. if filter out return false. 
	virtual bool prePushLog(LoggerId id, int level) = 0;
	//! Push log, thread safe.
	virtual bool pushLog(LoggerId id, int level, const char * log, const char * file = NULL, int line = 0) = 0;

	//! set logger's attribute, thread safe.
	virtual bool enableLogger(LoggerId id, bool enable) = 0;
	virtual bool setLoggerName(LoggerId id, const char * name) = 0;
	virtual bool setLoggerPath(LoggerId id, const char * path) = 0;
	virtual bool setLoggerLevel(LoggerId id, int nLevel) = 0;
	virtual bool setLoggerFileLine(LoggerId id, bool enable) = 0;
	virtual bool setLoggerDisplay(LoggerId id, bool enable) = 0;
	virtual bool setLoggerOutFile(LoggerId id, bool enable) = 0;
	virtual bool setLoggerLimitsize(LoggerId id, unsigned int limitsize) = 0;
	virtual bool setLoggerMonthdir(LoggerId id, bool enable) = 0;
	

	//! Update logger's attribute from config file, thread safe.
	virtual bool setAutoUpdate(int interval/*per second, 0 is disable auto update*/) = 0;
	virtual bool updateConfig() = 0;

	//! Log4z status statistics, thread safe.
	virtual bool isLoggerEnable(LoggerId id) = 0;
	virtual unsigned long long getStatusTotalWriteCount() = 0;
	virtual unsigned long long getStatusTotalWriteBytes() = 0;
	virtual unsigned long long getStatusWaitingCount() = 0;
	virtual unsigned int getStatusActiveLoggers() = 0;
};

class Log4zStream;
class Log4zBinary;

}

//! base micro.
#define LOG_STREAM(id, level, log)\
do{\
	if (bt::ILog4zManager::getPtr()->prePushLog(id,level)) \
	{\
		char logBuf[LOG4Z_LOG_BUF_SIZE];\
		bt::Log4zStream ss(logBuf, LOG4Z_LOG_BUF_SIZE);\
		ss << log;\
		bt::ILog4zManager::getPtr()->pushLog(id, level, logBuf, __FILE__, __LINE__);\
	}\
} while (0)


//! fast micro
#define LOG_TRACE(id, log) LOG_STREAM(id, LOG_LEVEL_TRACE, log)
#define LOG_DEBUG(id, log) LOG_STREAM(id, LOG_LEVEL_DEBUG, log)
#define LOG_INFO(id, log)  LOG_STREAM(id, LOG_LEVEL_INFO, log)
#define LOG_WARN(id, log)  LOG_STREAM(id, LOG_LEVEL_WARN, log)
#define LOG_ERROR(id, log) LOG_STREAM(id, LOG_LEVEL_ERROR, log)
#define LOG_ALARM(id, log) LOG_STREAM(id, LOG_LEVEL_ALARM, log)
#define LOG_FATAL(id, log) LOG_STREAM(id, LOG_LEVEL_FATAL, log)

//! super micro.
#define LOGT( log ) LOG_TRACE(LOG4Z_MAIN_LOGGER_ID, log )
#define LOGD( log ) LOG_DEBUG(LOG4Z_MAIN_LOGGER_ID, log )
#define LOGI( log ) LOG_INFO(LOG4Z_MAIN_LOGGER_ID, log )
#define LOGW( log ) LOG_WARN(LOG4Z_MAIN_LOGGER_ID, log )
#define LOGE( log ) LOG_ERROR(LOG4Z_MAIN_LOGGER_ID, log )
#define LOGA( log ) LOG_ALARM(LOG4Z_MAIN_LOGGER_ID, log )
#define LOGF( log ) LOG_FATAL(LOG4Z_MAIN_LOGGER_ID, log )


//! format input log.
#ifdef LOG4Z_FORMAT_INPUT_ENABLE
#define LOG_FORMAT(id, level, logformat, ...) \
do{ \
	if (bt::ILog4zManager::getPtr()->prePushLog(id,level)) \
	{\
		char logbuf[LOG4Z_LOG_BUF_SIZE]; \
		snprintf(logbuf, LOG4Z_LOG_BUF_SIZE,logformat, ##__VA_ARGS__); \
		bt::ILog4zManager::getPtr()->pushLog(id, level, logbuf, __FILE__, __LINE__); \
	} \
}while(0)

//!format string
#define LOGFMT_TRACE(id, fmt, ...)  LOG_FORMAT(id, LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)
#define LOGFMT_DEBUG(id, fmt, ...)  LOG_FORMAT(id, LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOGFMT_INFO(id, fmt, ...)  LOG_FORMAT(id, LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOGFMT_WARN(id, fmt, ...)  LOG_FORMAT(id, LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOGFMT_ERROR(id, fmt, ...)  LOG_FORMAT(id, LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOGFMT_ALARM(id, fmt, ...)  LOG_FORMAT(id, LOG_LEVEL_ALARM, fmt, ##__VA_ARGS__)
#define LOGFMT_FATAL(id, fmt, ...)  LOG_FORMAT(id, LOG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#define LOGFMTT( fmt, ...) LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, fmt,  ##__VA_ARGS__)
#define LOGFMTD( fmt, ...) LOGFMT_DEBUG(LOG4Z_MAIN_LOGGER_ID, fmt,  ##__VA_ARGS__)
#define LOGFMTI( fmt, ...) LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, fmt,  ##__VA_ARGS__)
#define LOGFMTW( fmt, ...) LOGFMT_WARN(LOG4Z_MAIN_LOGGER_ID, fmt,  ##__VA_ARGS__)
#define LOGFMTE( fmt, ...) LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, fmt,  ##__VA_ARGS__)
#define LOGFMTA( fmt, ...) LOGFMT_ALARM(LOG4Z_MAIN_LOGGER_ID, fmt,  ##__VA_ARGS__)
#define LOGFMTF( fmt, ...) LOGFMT_FATAL(LOG4Z_MAIN_LOGGER_ID, fmt,  ##__VA_ARGS__)
#else
inline void empty_log_format_function1(LoggerId id, const char*, ...){}
inline void empty_log_format_function2(const char*, ...){}
#define LOGFMT_TRACE empty_log_format_function1
#define LOGFMT_DEBUG LOGFMT_TRACE
#define LOGFMT_INFO LOGFMT_TRACE
#define LOGFMT_WARN LOGFMT_TRACE
#define LOGFMT_ERROR LOGFMT_TRACE
#define LOGFMT_ALARM LOGFMT_TRACE
#define LOGFMT_FATAL LOGFMT_TRACE
#define LOGFMTT empty_log_format_function2
#define LOGFMTD LOGFMTT
#define LOGFMTI LOGFMTT
#define LOGFMTW LOGFMTT
#define LOGFMTE LOGFMTT
#define LOGFMTA LOGFMTT
#define LOGFMTF LOGFMTT
#endif


namespace bt {

class Log4zBinary
{
public:
	Log4zBinary(const char * buf, int len)
	{
		_buf = buf;
		_len = len;
	}
	const char * _buf;
	int  _len;
};
class Log4zStream
{
public:
	inline Log4zStream(char * buf, int len);
	inline int getCurrentLen(){return (int)(_cur - _begin);}
private:
	template<class T>
	inline Log4zStream & writeData(const char * ft, T t);
	inline Log4zStream & writeLongLong(long long t);
	inline Log4zStream & writeULongLong(unsigned long long t);
	inline Log4zStream & writePointer(const void * t);
	inline Log4zStream & writeString(const wchar_t* t){ return writeData("%s", t); }
	inline Log4zStream & writeWString(const wchar_t* t);
	inline Log4zStream & writeBinary(const Log4zBinary & t);
public:
	inline Log4zStream & operator <<(const void * t){ return  writePointer(t); }

	inline Log4zStream & operator <<(const char * t){return writeData("%s", t);}
	
    inline Log4zStream & operator <<(bool t){ return (t ? writeData("%s", "true") : writeData("%s", "false"));}

	inline Log4zStream & operator <<(char t){return writeData("%c", t);}

	inline Log4zStream & operator <<(unsigned char t){return writeData("%u",(unsigned int)t);}

	inline Log4zStream & operator <<(short t){ return writeData("%d", (int)t); }

	inline Log4zStream & operator <<(unsigned short t){ return writeData("%u", (unsigned int)t); }

	inline Log4zStream & operator <<(int t){return writeData("%d", t);}

	inline Log4zStream & operator <<(unsigned int t){return writeData("%u", t);}

	inline Log4zStream & operator <<(long t) { return writeLongLong(t); }

	inline Log4zStream & operator <<(unsigned long t){ return writeULongLong(t); }

	inline Log4zStream & operator <<(long long t) { return writeLongLong(t); }

	inline Log4zStream & operator <<(unsigned long long t){ return writeULongLong(t); }

	inline Log4zStream & operator <<(float t){return writeData("%.4f", t);}

	inline Log4zStream & operator <<(double t){return writeData("%.4lf", t);}

	template<class _Elem,class _Traits,class _Alloc> //support std::string, std::wstring
	inline Log4zStream & operator <<(const std::basic_string<_Elem, _Traits, _Alloc> & t){ return *this << t.c_str(); }

	inline Log4zStream & operator << (const bt::Log4zBinary & binary){ return writeBinary(binary); }

	template<class _Elem, class _Alloc>
	inline Log4zStream & operator <<(const std::vector<_Elem, _Alloc> & t){ *this << "vector[size="; *this << t.size(); return *this << "]"; }
	template<class _Elem, class _Alloc>
	inline Log4zStream & operator <<(const std::list<_Elem, _Alloc> & t){ *this << "list[size="; *this << t.size(); return *this << "]"; }
	template<class _Elem, class _Alloc>
	inline Log4zStream & operator <<(const std::deque<_Elem, _Alloc> & t){ *this << "deque[size="; *this << t.size(); return *this << "]"; }
	template<class _Elem, class _Alloc>
	inline Log4zStream & operator <<(const std::queue<_Elem, _Alloc> & t){ *this << "queue[size="; *this << t.size(); return *this << "]"; }
	template<class _K, class _V, class _Pr, class _Alloc>
	inline Log4zStream & operator <<(const std::map<_K, _V, _Pr, _Alloc> & t){ *this << "map[size="; *this << t.size(); return *this << "]"; }

private:
	Log4zStream(){}
	Log4zStream(Log4zStream &){}
	char *  _begin;
	char *  _end;
	char *  _cur;
};

inline Log4zStream::Log4zStream(char * buf, int len)
{
	_begin = buf;
	_end = buf + len;
	_cur = _begin;
}

template<class T>
inline Log4zStream& Log4zStream::writeData(const char * ft, T t)
{
	if (_cur < _end)
	{
		int len = 0;
		int count = (int)(_end - _cur);
		len = snprintf(_cur, count, ft, t);
		if (len < 0)
		{
			*_cur = '\0';
			len = 0;
		}
		else if (len >= count)
		{
			len = count;
			*(_end - 1) = '\0';
		}
		_cur += len;
	}
	return *this;
}

inline Log4zStream & Log4zStream::writeLongLong(long long t)
{
	writeData("%lld", t);
	return *this;
}

inline Log4zStream & Log4zStream::writeULongLong(unsigned long long t)
{
	writeData("%llu", t);
	return *this;
}

inline Log4zStream & Log4zStream::writePointer(const void * t)
{
	sizeof(t) == 8 ? writeData("%016llx", (unsigned long long)t) : writeData("%08llx", (unsigned long long)t);
	return *this;
}

inline Log4zStream & Log4zStream::writeBinary(const Log4zBinary & t)
{
	writeData("%s", "\r\n\t[");
	for (int i = 0; i < t._len; i++)
	{
		if (i % 16 == 0)
		{
			writeData("%s", "\r\n\t");
			*this << (void*)(t._buf + i);
			writeData("%s", ": ");
		}
		writeData("%02x ", (unsigned char)t._buf[i]);
	}
	writeData("%s", "\r\n\t]\r\n\t");
	return *this;
}

inline bt::Log4zStream & bt::Log4zStream::writeWString(const wchar_t* t)
{
	//not support
	return *this;
}

}
#endif
