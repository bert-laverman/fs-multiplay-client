/**
 * RapidXmlUtil
 */
#ifndef __NL_RAKIS_FSX_PARSERESULT_H
#define __NL_RAKIS_FSX_PARSERESULT_H

#include <string>
#include <vector>
#include <cctype>

#include <boost/property_tree/detail/rapidxml.hpp>

namespace nl {
namespace rakis {

typedef boost::property_tree::detail::rapidxml::xml_document<>  XmlDoc;
typedef boost::property_tree::detail::rapidxml::xml_node<>      XmlNode;
typedef XmlNode*                                                XmlNodePtr;
typedef boost::property_tree::detail::rapidxml::xml_attribute<> XmlAttr;
typedef XmlAttr*                                                XmlAttrPtr;

class ParseResult
{
public: // Constants & Types

private: // Members
    std::vector<std::string> warnings_;
    std::vector<std::string> errors_;

public: // Methods
    ParseResult() {};
    ParseResult(ParseResult const& r) : warnings_(r.warnings_), errors_(r.errors_) {};
    ~ParseResult() {};

    void addWarning(std::string const& s) { warnings_.push_back(s); };
    std::vector<std::string> const& getWarnings() const { return warnings_; };

    void addError(std::string const& s) { errors_.push_back(s); };
    std::vector<std::string> const& getErrors() const { return errors_; };

    operator bool() const { return errors_.empty(); };
};

inline std::wstring fromCharStr(const char* cs)
{
    std::string s(cs);
    return std::wstring(s.begin(), s.end());
};

inline XmlNodePtr getNode(XmlNodePtr node, const char* name)
{
    return node->first_node(name, 0, false);
};
inline XmlNodePtr getSibling(XmlNodePtr node, const char* name)
{
    return node->next_sibling(name, 0, false);
};
inline bool getNode(XmlNodePtr node, const char* name, XmlNodePtr& result)
{
    result = getNode(node, name);
    return result != nullptr;
};
inline const char* getNodeText(XmlNodePtr node, const char* name)
{
    auto result = getNode(node, name);
    return (result == nullptr) ? nullptr : result->value();
};
inline const char* getNodeNonNullText(XmlNodePtr node, const char* name)
{
	auto result = getNode(node, name);
	return (result == nullptr) ? "" : result->value();
};

inline XmlAttrPtr getAttr(XmlNodePtr node, const char* name)
{
    return node->first_attribute(name, 0, false);
};
inline bool getAttr(XmlNodePtr node, const char* name, XmlAttrPtr& result)
{
    result = getAttr(node, name);
    return result != nullptr;
};
inline const char* getAttrText(XmlNodePtr node, const char* name, const char* defaultValue ="")
{
    auto result = getAttr(node, name);
    return (result == nullptr) ? defaultValue : result->value();
};

inline const char* getAttr(XmlNodePtr node, const char* name, ParseResult& msg, const char* error)
{
    const char* result = nullptr;
    XmlAttrPtr attr;
    if (getAttr(node, name, attr)) {
        result = attr->value();
    }
    else {
        msg.addError(error);
    }
    return result;
};
inline double getDblAttr(XmlNodePtr node, const char* name, ParseResult& msg, const char* error)
{
    const char* result = getAttr(node, name, msg, error);

    return (result == nullptr) ? 0.0f : atof(result);
};
inline double getDblAttr(XmlNodePtr node, const char* name, double defaultValue)
{
    const char* result = getAttrText(node, name);

    return (result [0] == '\0') ? defaultValue : atof(result);
};
inline int getIntAttr(XmlNodePtr node, const char* name, ParseResult& msg, const char* error)
{
    const char* result = getAttr(node, name, msg, error);

    return (result == nullptr) ? 0 : atoi(result);
};
inline int getIntAttr(XmlNodePtr node, const char* name, int defaultValue)
{
	const char* result = getAttrText(node, name);

	return (result[0] == '\0') ? defaultValue : atoi(result);
};
inline bool isTrue(const char* s)
{
    return (std::toupper(s[0]) == 'T') && (std::toupper(s[1]) == 'R') && (std::toupper(s[2]) == 'U') && (std::toupper(s[3]) == 'E') && (std::toupper(s[4]) == 0);
};
inline bool isFalse(const char* s)
{
    return (std::toupper(s[0]) == 'F') && (std::toupper(s[1]) == 'A') && (std::toupper(s[2]) == 'L') && (std::toupper(s[3]) == 'S') && (std::toupper(s[4]) == 'E') && (std::toupper(s[5]) == 0);
};
inline bool isYes(const char* s)
{
    return (std::toupper(s[0]) == 'Y') && (std::toupper(s[1]) == 'E') && (std::toupper(s[2]) == 'S') && (std::toupper(s[3]) == 0);
};
inline bool isNo(const char* s)
{
    return (std::toupper(s[0]) == 'N') && (std::toupper(s[1]) == 'O') && (std::toupper(s[2]) == 0);
};
inline bool getBoolAttr(XmlNodePtr node, const char* name, ParseResult& msg, const char* error)
{
    const char* value = getAttr(node, name, msg, error);
    bool result = false;

    if (isTrue(value) || isYes(value)) {
        result = true;
    }
    else if (isFalse(value) || isNo(value)) {
        result = false;
    }
    else {
        msg.addError("Bad boolean attribute value");
    }
    return result;
};
inline bool getBoolAttr(XmlNodePtr node, const char* name, bool defaultValue)
{
    const char* value = getAttrText(node, name);
    bool result = defaultValue;

    if (value != nullptr) {
        result = isTrue(value) || isYes(value);
    }
    return result;
};

} // namespace rakis
} // nl

#endif
