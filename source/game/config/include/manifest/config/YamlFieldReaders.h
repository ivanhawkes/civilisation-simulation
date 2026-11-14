#pragma once

#include <yaml-cpp/yaml.h>

#include <charconv>
#include <string>

// Field readers shared by the config loaders. They own the two rules every loader
// needs, so those rules are stated once.
//
//  * A YAML null is "not set", never the four-character string "null". yaml-cpp
//    renders a null as that text, and `requires: []` and `exclusiveGroupId: null`
//    appear all over the data. Loading them as a value invents prerequisites and
//    puts every skill in an exclusive group named "null", which locks whole branches
//    of the tree out.
//  * A missing or mistyped field is an error naming the field and the value, so the
//    loader can point at the offending entry instead of dying in as<std::string>.
//
// All the lookups go through FindValue: indexing a non-mapping throws, and a
// default-constructed YAML::Node is invalid and throws on any use, so this file
// creates neither.
namespace Manifest::Core::YamlRead
{
// The text yaml-cpp gives a null scalar, before it is mistaken for a value.
inline bool IsNullText(const std::string &text)
{
	return text == "null" || text == "Null" || text == "NULL" || text == "~";
}

// A field carries a useable scalar only when it is defined, not null and a scalar.
inline bool HasValue(const YAML::Node &node)
{
	return node.IsDefined() && !node.IsNull() && node.IsScalar();
}

// Look up Key. Two yaml-cpp shapes throw, so neither is used directly: indexing a
// scalar, and a const lookup of a key that is not there. Index a copy instead -
// copies share the document, and a non-const lookup of an absent key hands back a node
// that reports itself missing, which readers turn into a load error.
inline YAML::Node FindValue(const YAML::Node &parent, const char *key)
{
	if (!parent.IsMap())
	{
		YAML::Node emptyDocument = YAML::Load("{}");

		return emptyDocument[key];
	}

	YAML::Node node = parent;

	return node[key];
}

// A required string field: missing, null and non-scalars all fail. Contexts names the
// entry, for example items[3].
inline bool ReadString(
    const YAML::Node &parent, const char *key, std::string &out, const std::string &contexts,
    std::string &errorMessage)
{
	const YAML::Node field = FindValue(parent, key);

	if (!HasValue(field))
	{
		errorMessage = contexts + ": missing field '" + std::string(key) + "' (expected a string).";
		return false;
	}

	out = field.as<std::string>();

	if (out.empty() || IsNullText(out))
	{
		errorMessage = contexts + ": field '" + std::string(key) + "' has no value.";
		return false;
	}

	return true;
}

// An optional string: null and absent become empty, because that is what the data
// means by them. Contexts is unused: an absent optional is not an error.
inline void ReadOptionalString(
    const YAML::Node &parent, const char *key, std::string &out, const std::string &contexts)
{
	(void)contexts;

	const YAML::Node field = FindValue(parent, key);

	if (!HasValue(field) || IsNullText(field.as<std::string>()))
	{
		out.clear();
		return;
	}

	out = field.as<std::string>();
}

inline bool ReadInt(
    const YAML::Node &parent, const char *key, int &out, const std::string &contexts,
    std::string &errorMessage)
{
	const YAML::Node field = FindValue(parent, key);

	if (!HasValue(field))
	{
		errorMessage =
		    contexts + ": field '" + std::string(key) + "' must be an integer" +
		    (field.IsDefined() ? ", not '" + field.as<std::string>() + "'" : " (it is missing)");
		return false;
	}

	const auto text = field.as<std::string>();
	int parsed = 0;
	const char *begin = text.data();
	const char *end = begin + text.size();

	while (begin < end && (*begin == ' ' || *begin == '\t'))
	{
		++begin;
	}

	while (end > begin && (end[-1] == ' ' || end[-1] == '\t'))
	{
		--end;
	}

	const std::from_chars_result result = std::from_chars(begin, end, parsed);

	if (result.ec != std::errc() || result.ptr != end)
	{
		errorMessage =
		    contexts + ": field '" + std::string(key) + "' must be an integer, not '" + text + "'";
		return false;
	}

	out = parsed;

	return true;
}

inline bool ReadFloat(
    const YAML::Node &parent, const char *key, float &out, const std::string &contexts,
    std::string &errorMessage)
{
	const YAML::Node field = FindValue(parent, key);

	if (!HasValue(field))
	{
		errorMessage = contexts + ": field '" + std::string(key) + "' must be a number, not '" +
		               (field.IsDefined() ? field.as<std::string>() : std::string("<missing>")) + "'";
		return false;
	}

	const auto text = field.as<std::string>();

	// strtod is locale dependent, and a comma decimal point is not a number here.
	try
	{
		std::size_t consumed = 0;
		const double parsed = std::stod(text, &consumed);

		if (consumed != text.size())
		{
			errorMessage =
			    contexts + ": field '" + std::string(key) + "' must be a number, not '" + text + "'";
			return false;
		}

		out = static_cast<float>(parsed);
		return true;
	}
	catch (const std::exception &)
	{
		errorMessage =
		    contexts + ": field '" + std::string(key) + "' must be a number, not '" + text + "'";
		return false;
	}
}

// Optional numbers, with a floor: maxTiers 0 is a skill nobody can level and a
// negative cost would pay the player for researching, so an absent field takes the
// default and a value below the floor is raised to it.
inline bool ReadInt(
    const YAML::Node &parent, const char *key, int &out, bool required, int fallback,
    const std::string &contexts, std::string &errorMessage, int min = 1)
{
	const YAML::Node field = FindValue(parent, key);

	if (!HasValue(field))
	{
		if (required)
		{
			errorMessage =
			    contexts + ": missing field '" + std::string(key) + "' (expected an integer)";
			return false;
		}

		out = fallback;

		return true;
	}

	if (!ReadInt(parent, key, out, contexts, errorMessage))
	{
		return false;
	}

	if (out < min)
	{
		out = min;
	}

	return true;
}

inline bool ReadFloat(
    const YAML::Node &parent, const char *key, float &out, bool required, float fallback,
    const std::string &contexts, std::string &errorMessage)
{
	const YAML::Node field = FindValue(parent, key);

	if (!HasValue(field))
	{
		if (required)
		{
			errorMessage = contexts + ": missing field '" + std::string(key) + "' (expected a number)";
			return false;
		}

		out = fallback;

		return true;
	}

	return ReadFloat(parent, key, out, contexts, errorMessage);
}

// A required sequence. The node comes back ready to iterate when it is one; on failure
// it reports itself as missing, so check IsSequence() and read ErrorMessage.
inline YAML::Node ReadSequence(const YAML::Node &root, const char *key, std::string &errorMessage)
{
	const YAML::Node found = FindValue(root, key);

	if (!found.IsSequence())
	{
		errorMessage = root.IsMap() ? std::string("'") + key + "' must be a YAML sequence."
		                            : std::string("'") + key +
		                                  "' must be a YAML sequence: the document is not a mapping.";
	}

	return found;
}

// Names one entry of a sequence, for example items[3].
inline std::string Context(const std::string &field, std::size_t index)
{
	return field + "[" + std::to_string(index) + "]";
}

// Names a field of one entry, for example items[3].categoryId.
inline std::string Context(const std::string &field, std::size_t index, const std::string &child)
{
	return field + "[" + std::to_string(index) + "]." + child;
}

// Names one element of a field of one entry, for example skills[2].requires[0].
inline std::string Context(
    const std::string &field, std::size_t index, const std::string &child, std::size_t childIndex)
{
	return field + "[" + std::to_string(index) + "]." + child + "[" + std::to_string(childIndex) + "]";
}
} // namespace Manifest::Core::YamlRead
