/**
 * nl::rakis::File
 */

#ifndef __RAKIS_NL_FILE_H
#define __RAKIS_NL_FILE_H

#include <sys/stat.h>
#include <string>

namespace nl {
	namespace rakis {

		inline std::wstring str2wstr(const std::string& s) { return std::wstring(s.begin(), s.end()); }
        inline std::string wstr2str(const std::wstring& s) { std::string result; result.reserve(s.size()); for (auto c: s) { int b(wctob(c)); if (b != EOF) { result.push_back(char(b)); } } return result; };

		class File {
		public:  // Types and constants
			static const wchar_t      separatorChar;
			static const char         separatorChar1;
			static const std::wstring separator;
			static const std::string  separator1;

		private: // Hidden members
			std::wstring name_;
			std::wstring path_;
			struct _stat stat_;
			bool         statValid_;
			bool         statDone_;
			int          statResult_;

			void init();

            static const std::wstring defaultPrefix;
            static const std::wstring defaultExt;

        public:  // Public Members
			File();
			File(const char* name);
			File(const std::string& name);
			File(const wchar_t* name);
			File(const std::wstring& name);
            File(File const& file);
			~File();

            File tempFile(std::wstring const& prefix, std::wstring const& ext) const;
            File tempFile(std::string const& prefix,  std::wstring const& ext) const { return tempFile(str2wstr(prefix), ext); };
            File tempFile(std::wstring const& prefix, std::string const& ext)  const { return tempFile(prefix, str2wstr(ext)); };
            File tempFile(std::string const& prefix, std::string const& ext)   const { return tempFile(str2wstr(prefix), str2wstr(ext)); };
            File tempFile(std::wstring const& ext)                             const { return tempFile(defaultPrefix, ext); };
            File tempFile(std::string const& ext)                              const { return tempFile(defaultPrefix, str2wstr(ext)); };
            File tempFile(const char* ext)                                     const { return tempFile(defaultPrefix, str2wstr(ext)); };
            File tempFile()                                                    const { return tempFile(defaultPrefix, defaultExt); };

            bool canExecute()             const { return statValid_ && ((stat_.st_mode&_S_IEXEC) != 0); }
			bool canRead()                const { return statValid_ && ((stat_.st_mode&_S_IREAD) != 0); }
			bool canWrite()               const { return statValid_ && ((stat_.st_mode&_S_IWRITE) != 0); }
			bool exists()                 const { return statValid_; }

            bool remove();

            std::wstring const& getPath() const { return path_; }
            std::string  getPath1()       const { return std::string(path_.begin(), path_.end()); };
			std::wstring const& getName() const { return name_; }
            std::string  getName1()       const { return std::string(name_.begin(), name_.end()); };
			std::wstring getParent()      const;
            std::string  getParent1()     const;
            std::wstring getExt()         const;
            std::string  getExt1()        const;

            File getParentFile()          const { return File(getParent()); }
            File getChild(std::wstring const& name) const;
            File getChild(std::string const& name) const;

            bool isAbsolute() const;
			bool isDirectory()            const { return statValid_ && ((stat_.st_mode&S_IFDIR) != 0); }
			bool isFile()                 const { return statValid_ && ((stat_.st_mode&S_IFREG) != 0); }
			size_t length()               const { return (statValid_ ? stat_.st_size : -1); }

			operator const wchar_t*()     const { return path_.c_str(); }
            File& operator= (File const& file);

		private: // Blocked implementations
		};

	}
}

#endif
