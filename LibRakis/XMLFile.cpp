/**
 * XMLFile.cpp
 */
#include "stdafx.h"

#include "XMLFile.h"

#include <fstream>
#include <sstream>

#include <boost/property_tree/detail/rapidxml.hpp>

using namespace std;
using namespace boost::property_tree::detail::rapidxml;

using namespace nl::rakis;

/*static*/ Logger XMLFile::log_(Logger::getLogger("nl.rakis.XMLFile"));


XMLFile::XMLFile() : File(), dataLoaded_(false), xmlParsed_(false), error_(false)
{
}

XMLFile::XMLFile(const char* name) : File(name), dataLoaded_(false), xmlParsed_(false), error_(false)
{
}

XMLFile::XMLFile(const std::string& name) : File(name), dataLoaded_(false), xmlParsed_(false), error_(false)
{
}

XMLFile::XMLFile(const wchar_t* name) : File(name), dataLoaded_(false), xmlParsed_(false), error_(false)
{
}

XMLFile::XMLFile(const std::wstring& name) : File(name), dataLoaded_(false), xmlParsed_(false), error_(false)
{
}

XMLFile::XMLFile(const File& file) : File(file), dataLoaded_(false), xmlParsed_(false), error_(false)
{
}

XMLFile::XMLFile(const XMLFile& xmlFile) : File(xmlFile), dataLoaded_(false), xmlParsed_(false), error_(false)
{
}

XMLFile::~XMLFile()
{
}

XMLFile& XMLFile::operator= (const File& file)
{
	File::operator=(file);
	dataLoaded_ = false;
	xmlParsed_ = false;
	error_ = false;

	return *this;
}

XMLFile& XMLFile::operator= (const XMLFile& file)
{
	File::operator=(file);
	dataLoaded_ = false;
	xmlParsed_ = false;
	error_ = false;

	return *this;
}


void XMLFile::error(const char* s, const char* inMethod)
{
	ostringstream os;
	if (inMethod != nullptr) {
		os << inMethod << "(): ";
	}
	os << s << " (path=\"" << getPath1() << "\"";

	string msg = os.str();
	log_.error(msg);
	msg_.addError(msg);
	error_ = true;
}

void XMLFile::warning(const char* s, const char* inMethod)
{
	ostringstream os;
	if (inMethod != nullptr) {
		os << inMethod << "(): ";
	}
	os << s << " (path=\"" << getPath1() << "\"";

	string msg = os.str();
	log_.warn(msg);
	msg_.addWarning(msg);
}

void XMLFile::loadData()
{
	if (!dataLoaded_) {
		log_.debug("loadData()");

		dataLoaded_ = true; // Only once, unless forced to reload...
		if (!exists()) {
			error("Non-existent file", "loadData");
		}
		else if (isDirectory()) {
			error("Cannot load a directory", "loadData");
		}
		else {
			const size_t len = length();
			log_.debug("loadData(): Read file \"", getPath1(), "\", ", len, " byte(s)");
			try {
				data_.reserve(len + 1);
				data_.resize(len + 1);
				ifstream f(getPath1());
				if (!f) {
					throw "Fail";
				}

				f.read(data_.data(), len);
				unsigned n = unsigned(f.gcount());
				data_[n] = '\0'; // Make it a valid C-string...
				data_.resize(n + 1);

				if (n != len) {
					log_.debug("loadData(): File size reported as ", len, " byte(s), could only read ", n, " byte(s). CRLF to LF?");
				}
			}
			catch (...) {
				error("Reading file threw an exception", "loadData");
			}
		}
	}
	log_.trace("loadData(): Done");
}

void XMLFile::parseData()
{
	loadData();
	if (!xmlParsed_ && !error_) {
		log_.debug("parseData()");

		xmlParsed_ = true;
		try {
			doc_.parse<0>(data_.data());
		}
		catch (parse_error const& e) {
			error("XML parser threw a parse_error", "parseData");
			log_.error("load(): what=\"", e.what(), "\", where index=", size_t(e.where<char>() - data()), ", next char='", *e.where<char>(), "' (", int(*e.where<char>()), ")");
		}
		catch (...) {
			error("XML parser threw some exception", "parseData");
		}
	}
	log_.trace("parseData(): Done");
}

const char* XMLFile::data()
{
	loadData();

	return data_.data();
}

const ParseResult& XMLFile::messages()
{
	parseData();

	return msg_;
}

const XmlDoc& XMLFile::document()
{
	parseData();

	return doc_;
}


void XMLFile::reload()
{
	dataLoaded_ = false;
	xmlParsed_ = false;
	error_ = false;
}

bool XMLFile::reloadIfChanged()
{
	reload();
	return true;
}

