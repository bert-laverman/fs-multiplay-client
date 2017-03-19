/*
 */
#ifndef __NL_RAKIS_LOG_H
#define __NL_RAKIS_LOG_H

#include "File.h"

#include <map>
#include <vector>
#include <string>
#include <ostream>
#include <fstream>
#include <cwchar>

namespace nl {
namespace rakis {

	class Logger {
	public:
		enum Level {
			LOGLVL_TRACE,
			LOGLVL_DEBUG,
			LOGLVL_INFO,
			LOGLVL_WARN,
			LOGLVL_ERROR,
			LOGLVL_FATAL,
			LOGLVL_INIT
		};
		typedef std::array<char, 256> LineBuffer;
		typedef std::array<std::ofstream, 16> TargetArray;

		static void configure(const File& file);
		inline static Logger getLogger(const std::string& name) { return Logger(name); }

	private:
		std::string name_;
		Level level_;

		Logger(const std::string& name);

		void startLine(std::ostream& s, Level level);
		void stream(std::ostream& str) { str << std::endl; }

		template <class T, class... Types>
		void stream(std::ostream& str, const T& arg, const Types&... args) {
			str << arg;
			stream(str, args...);
		}
		template <class... Types>
		void stream(std::ostream& str, const std::wstring& arg, const Types&... args) {
			for (auto i = arg.begin(); i != arg.end(); i++) {
				str << char(wctob(*i));
			}
			stream(str, args...);
		}

		static std::map<std::string, Level> levels_;
		static std::map<std::string, size_t> logTargets_;
		static size_t numTargets_;
		static TargetArray targets_;
		static std::ofstream rootLog_;
		static bool configDone_;

		static bool parseLine(std::string& name, std::string& value, const LineBuffer& buf, const File& file);

		static Level getLevel(const std::string& name);
		static std::ostream& getStream(const std::string& name);

	public:
		Logger(const Logger& log) : name_(log.name_), level_(log.level_) { }
		~Logger();

		const std::string& getName() const { return name_; }
		const char* getNameC() const { return name_.c_str(); }

		Level getLevel();
		void setLevel(Level level) { level_ = level; }

		bool isTraceEnabled() { return getLevel() <= LOGLVL_TRACE; }
		bool isDebugEnabled() { return getLevel() <= LOGLVL_DEBUG; }
		bool isInfoEnabled() { return getLevel() <= LOGLVL_INFO; }
		bool isWarnEnabled() { return getLevel() <= LOGLVL_WARN; }
		bool isErrorEnabled() { return getLevel() <= LOGLVL_ERROR; }
		bool isFatalEnabled() { return getLevel() <= LOGLVL_FATAL; }

		template <class T, class... Types>
		void trace(const T& arg, const Types&... args) {
			if (isTraceEnabled()) {
				ostream& s(getStream(name_));
				startLine(s, LOGLVL_TRACE);
				stream(s, arg, args...);
				s.flush();
			}
		}
		template <class T, class... Types>
		void debug(const T& arg, const Types&... args) {
			if (isDebugEnabled()) {
				ostream& s(getStream(name_));
				startLine(s, LOGLVL_DEBUG);
				stream(s, arg, args...);
				s.flush();
			} 
		}
		template <class T, class... Types>
		void info(const T& arg, const Types&... args) {
			if (isInfoEnabled()) {
				ostream& s(getStream(name_));
				startLine(s, LOGLVL_INFO);
				stream(s, arg, args...);
				s.flush();
			}
		}
		template <class T, class... Types>
		void warn(const T& arg, const Types&... args) {
			if (isWarnEnabled()) {
				ostream& s(getStream(name_));
				startLine(s, LOGLVL_WARN);
				stream(s, arg, args...);
				s.flush();
			}
		}
		template <class T, class... Types>
		void error(const T& arg, const Types&... args) {
			if (isErrorEnabled()) {
				ostream& s(getStream(name_));
				startLine(s, LOGLVL_ERROR);
				stream(s, arg, args...);
				s.flush();
			}
		}
		template <class T, class... Types>
		void fatal(const T& arg, const Types&... args) {
			if (isFatalEnabled()) {
				ostream& s(getStream(name_));
				startLine(s, LOGLVL_FATAL);
				stream(s, arg, args...);
				s.flush();
			}
		}

		Logger& operator=(const Logger& log) {
			name_ = log.name_;
			level_ = log.level_;
		}

	private: // deleted and/or disallowed stuff
		Logger() =delete;
	};
}
}

#endif
