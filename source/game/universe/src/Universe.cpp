#include <manifest/universe/Universe.h>

#include <exception>
#include <format>
#include <yaml-cpp/yaml.h>

#include <string>
#include <utility>

#include <manifest/config/YamlFieldReaders.h>


namespace Manifest::Core
{
void UniverseNamePools::AddPool(const std::string &poolName, std::vector<std::string> names)
{
	m_pools[poolName] = std::move(names);
}


const std::vector<std::string> &UniverseNamePools::Find(const std::string &poolName) const
{
	static const std::vector<std::string> noNames;

	const auto entry = m_pools.find(poolName);

	return entry != m_pools.end() ? entry->second : noNames;
}


std::vector<std::string> UniverseNamePools::PoolNames() const
{
	std::vector<std::string> names;
	names.reserve(m_pools.size());

	for (const auto &entry : m_pools)
	{
		names.push_back(entry.first);
	}

	return names;
}


std::size_t UniverseNamePools::PoolCount() const
{
	return m_pools.size();
}


std::size_t UniverseNamePools::NameCount() const
{
	std::size_t total = 0;

	for (const auto &entry : m_pools)
	{
		total += entry.second.size();
	}

	return total;
}


bool UniverseNamePools::Empty() const
{
	return m_pools.empty();
}


bool LoadUniverseNamePools(
    const std::string &filePath, UniverseNamePools &pools, std::string &errorMessage)
{
	errorMessage.clear();

	YAML::Node rootNode;
	try
	{
		rootNode = YAML::LoadFile(filePath);
	}
	catch (const std::exception &exception)
	{
		errorMessage =
		    std::format("{}{}': {}", std::string("Could not read '"), filePath, exception.what());
		return false;
	}

	// Replace, never append: loading the same file twice gives the same pools.
	pools = UniverseNamePools{};

	if (!rootNode.IsMap())
	{
		errorMessage =
		    filePath + ": the universe names must be a mapping of pool name to a list of names.";
		return false;
	}

	for (const auto &entry : rootNode)
	{
		if (!YamlRead::HasValue(entry.first))
		{
			errorMessage = filePath + ": a name pool needs a non-empty pool name.";
			return false;
		}

		const std::string poolName = entry.first.Scalar();

		if (!entry.second.IsSequence())
		{
			errorMessage = std::format("{}: pool '{}' must be a YAML sequence.", filePath, poolName);
			return false;
		}

		std::vector<std::string> names;
		std::size_t index = 0;
		for (const auto &name : entry.second)
		{
			if (!YamlRead::HasValue(name))
			{
				errorMessage = std::format(
				    "{}: pool '{}' entry [{}] is missing, null or empty.", filePath, poolName,
				    std::to_string(index));
				return false;
			}

			names.push_back(name.Scalar());
			++index;
		}

		if (names.empty())
		{
			errorMessage = std::format("{}: pool '{}' holds no names.", filePath, poolName);
			return false;
		}

		pools.AddPool(poolName, std::move(names));
	}

	if (pools.Empty())
	{
		errorMessage = filePath + ": no name pools were found.";
		return false;
	}

	return true;
}
} // namespace Manifest::Core
