/**
 * XMLFile.h
 */
#ifndef __NL_RAKIS_XMLFILE_H
#define __NL_RAKIS_XMLFILE_H

#include <vector>

#include <boost/property_tree/detail/rapidxml.hpp>

#include "RapidXmlUtil.h"
#include "File.h"
#include "Log.h"

namespace nl {
namespace rakis {

	class XMLFile: public virtual File
	{
	public: // typedefs, constants, ...

	private: // Members
		std::vector<char> data_;
		ParseResult       msg_;
		XmlDoc            doc_;

		bool              dataLoaded_;
		bool              xmlParsed_;
		bool              error_;

		static Logger log_;

		void error(const char* s, const char* inMethod = nullptr);
		void warning(const char* s, const char* inMethod = nullptr);
		void loadData();
		void parseData();

	public: // Methods
		XMLFile();
		XMLFile(const char* name);
		XMLFile(const std::string& name);
		XMLFile(const wchar_t* name);
		XMLFile(const std::wstring& name);
		XMLFile(const File& file);
		XMLFile(const XMLFile& xmlFile);
		virtual ~XMLFile();
		XMLFile& operator= (const File& file);
		XMLFile& operator= (const XMLFile& file);

		const char* data();
		const ParseResult& messages();
		const XmlDoc& document();

		void reload();
		bool reloadIfChanged();

		inline operator bool() const { return !error_; };

	private: // Deleted stuff
	};

} // rakis
} // nl

#endif
