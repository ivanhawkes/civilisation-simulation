#pragma once

#include <charconv>
#include <string>
#include <yaml-cpp/yaml.h>

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
namespace CivSimCore::YamlRead
{
// The text yaml-cpp gives a null scalar, before it is mistaken for a value.
inline bool IsNullText(const std::string &Text)
{
	return Text == "null" || Text == "Null" || Text == "NULL" || Text == "~";
}

// A field carries a useable scalar only when it is defined, not null and a scalar.
inline bool HasValue(const YAML::Node &Node)
{
	return Node.IsDefined() && !Node.IsNull() && Node.IsScalar();
}

// Look up Key. Two yaml-cpp shapes throw, so neither is used directly: indexing a
// scalar, and a const lookup of a key that is not there. Index a copy instead -
// copies share the document, and a non-const lookup of an absent key hands back a node
// that reports itself missing, which readers turn into a load error.
inline YAML::Node FindValue(const YAML::Node &Parent, const char *Key)
{
	if (!Parent.IsMap())
	{
		YAML::Node emptyDocument = YAML::Load("{}");

		return emptyDocument[Key];
	}

	YAML::Node node = Parent;

	return node[Key];
}

// A required string field: missing, null and non-scalars all fail. Contexts names the
// entry, for example items[3].
inline bool ReadString(
    const YAML::Node &Parent, const char *Key, std::string &Out, const std::string &Contexts,
    std::string &ErrorMessage)
{
	const YAML::Node field = FindValue(Parent, Key);

	if (!HasValue(field))
	{
		ErrorMessage =
		    Contexts + ": missing field '" + std::string(Key) + "' (expected a string).";
		return false;
	}

	Out = field.as<std::string>();

	if (Out.empty() || IsNullText(Out))
	{
		ErrorMessage = Contexts + ": field '" + std::string(Key) + "' has no value.";
		return false;
	}

	return true;
}

// An optional string: null and absent become empty, because that is what the data
// means by them. Contexts is unused: an absent optional is not an error.
inline void ReadOptionalString(
    const YAML::Node &Parent, const char *Key, std::string &Out, const std::string &Contexts)
{
	(void)Contexts;

	const YAML::Node field = FindValue(Parent, Key);

	if (!HasValue(field) || IsNullText(field.as<std::string>()))
	{
		Out.clear();
		return;
	}

	Out = field.as<std::string>();
}

inline bool ReadInt(
    const YAML::Node &Parent, const char *Key, int &Out, const std::string &Contexts,
    std::string &ErrorMessage)
{
	const YAML::Node field = FindValue(Parent, Key);

	if (!HasValue(field))
	{
		ErrorMessage = Contexts + ": field '" + std::string(Key) + "' must be an integer" +
		               (field.IsDefined() ? ", not '" + field.as<std::string>() + "'"
		                                  : " (it is missing)");
		return false;
	}

	const std::string text = field.as<std::string>();
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
		ErrorMessage =
		    Contexts + ": field '" + std::string(Key) + "' must be an integer, not '" + text + "'";
		return false;
	}

	Out = parsed;

	return true;
}

inline bool ReadFloat(
    const YAML::Node &Parent, const char *Key, float &Out, const std::string &Contexts,
    std::string &ErrorMessage)
{
	const YAML::Node field = FindValue(Parent, Key);

	if (!HasValue(field))
	{
		ErrorMessage = Contexts + ": field '" + std::string(Key) + "' must be a number, not '" +
		               (field.IsDefined() ? field.as<std::string>() : std::string("<missing>")) +
		               "'";
		return false;
	}

	const std::string text = field.as<std::string>();

	// strtod is locale dependent, and a comma decimal point is not a number here.
	try
	{
		std::size_t consumed = 0;
		const double parsed = std::stod(text, &consumed);

		if (consumed != text.size())
		{
			ErrorMessage =
			    Contexts + ": field '" + std::string(Key) + "' must be a number, not '" + text + "'";
			return false;
		}

		Out = static_cast<float>(parsed);
		return true;
	}
	catch (const std::exception &)
	{
		ErrorMessage =
		    Contexts + ": field '" + std::string(Key) + "' must be a number, not '" + text + "'";
		return false;
	}
}

// Optional numbers, with a floor: maxTiers 0 is a skill nobody can level and a
// negative cost would pay the player for researching, so an absent field takes the
// default and a value below the floor is raised to it.
inline bool ReadInt(
    const YAML::Node &Parent, const char *Key, int &Out, bool Required, int Default,
    const std::string &Contexts, std::string &ErrorMessage, int Min = 1)
{
	const YAML::Node field = FindValue(Parent, Key);

	if (!HasValue(field))
	{
		if (Required)
		{
			ErrorMessage = Contexts + ": missing field '" + std::string(Key) +
			                   "' (expected an integer)";
			return false;
		}

		Out = Default;

		return true;
	}

	if (!ReadInt(Parent, Key, Out, Contexts, ErrorMessage))
	{
		return false;
	}

	if (Out < Min)
	{
		Out = Min;
	}

	return true;
}

inline bool ReadFloat(
    const YAML::Node &Parent, const char *Key, float &Out, bool Required, float Default,
    const std::string &Contexts, std::string &ErrorMessage)
{
	const YAML::Node field = FindValue(Parent, Key);

	if (!HasValue(field))
	{
		if (Required)
		{
			ErrorMessage = Contexts + ": missing field '" + std::string(Key) +
			                   "' (expected a number)";
			return false;
		}

		Out = Default;

		return true;
	}

	return ReadFloat(Parent, Key, Out, Contexts, ErrorMessage);
}

// A required sequence. The node comes back ready to iterate when it is one; on failure
// it reports itself as missing, so check IsSequence() and read ErrorMessage.
inline YAML::Node ReadSequence(const YAML::Node &Root, const char *Key, std::string &ErrorMessage)
{
	const YAML::Node found = FindValue(Root, Key);

	if (!found.IsSequence())
	{
		ErrorMessage = Root.IsMap()
		                   ? std::string("'") + Key + "' must be a YAML sequence."
		                   : std::string("'") + Key +
		                         "' must be a YAML sequence: the document is not a mapping.";
	}

	return found;
}

// Names one entry of a sequence, for example items[3].
inline std::string Context(const std::string &Field, std::size_t Index)
{
	return Field + "[" + std::to_string(Index) + "]";
}

// Names a field of one entry, for example items[3].categoryId.
inline std::string Context(const std::string &Field, std::size_t Index, const std::string &Child)
{
	return Field + "[" + std::to_string(Index) + "]." + Child;
}

// Names one element of a field of one entry, for example skills[2].requires[0].
inline std::string Context(
    const std::string &Field, std::size_t Index, const std::string &Child, std::size_t ChildIndex)
{
	return Field + "[" + std::to_string(Index) + "]." + Child + "[" + std::to_string(ChildIndex) +
	       "]";
}
} // namespace CivSimCore::YamlRead
