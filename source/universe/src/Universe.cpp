#include "Core/Universe.h"
#include "YamlFieldReaders.h"
#include <exception>
#include <string>
#include <utility>
#include <yaml-cpp/yaml.h>


namespace CivSimCore
{
void UniverseNamePools::AddPool(const std::string &PoolName, std::vector<std::string> Names)
{
	pools[PoolName] = std::move(Names);
}


const std::vector<std::string> &UniverseNamePools::Find(const std::string &PoolName) const
{
	static const std::vector<std::string> noNames;

	const auto entry = pools.find(PoolName);

	return entry != pools.end() ? entry->second : noNames;
}


std::vector<std::string> UniverseNamePools::PoolNames() const
{
	std::vector<std::string> names;
	names.reserve(pools.size());

	for (const auto &entry : pools)
	{
		names.push_back(entry.first);
	}

	return names;
}


std::size_t UniverseNamePools::PoolCount() const
{
	return pools.size();
}


std::size_t UniverseNamePools::NameCount() const
{
	std::size_t total = 0;

	for (const auto &entry : pools)
	{
		total += entry.second.size();
	}

	return total;
}


bool UniverseNamePools::Empty() const
{
	return pools.empty();
}


bool LoadUniverseNamePools(
    const std::string &FilePath, UniverseNamePools &Pools, std::string &ErrorMessage)
{
	ErrorMessage.clear();

	YAML::Node rootNode;
	try
	{
		rootNode = YAML::LoadFile(FilePath);
	}
	catch (const std::exception &exception)
	{
		ErrorMessage = std::string("Could not read '") + FilePath + "': " + exception.what();
		return false;
	}

	// Replace, never append: loading the same file twice gives the same pools.
	Pools = UniverseNamePools{};

	if (!rootNode.IsMap())
	{
		ErrorMessage =
		    FilePath + ": the universe names must be a mapping of pool name to a list of names.";
		return false;
	}

	for (const auto &entry : rootNode)
	{
		if (!YamlRead::HasValue(entry.first))
		{
			ErrorMessage = FilePath + ": a name pool needs a non-empty pool name.";
			return false;
		}

		const std::string poolName = entry.first.Scalar();

		if (!entry.second.IsSequence())
		{
			ErrorMessage = FilePath + ": pool '" + poolName + "' must be a YAML sequence.";
			return false;
		}

		std::vector<std::string> names;
		std::size_t index = 0;
		for (const auto &name : entry.second)
		{
			if (!YamlRead::HasValue(name))
			{
				ErrorMessage = FilePath + ": pool '" + poolName + "' entry [" + std::to_string(index) +
				               "] is missing, null or empty.";
				return false;
			}

			names.push_back(name.Scalar());
			++index;
		}

		if (names.empty())
		{
			ErrorMessage = FilePath + ": pool '" + poolName + "' holds no names.";
			return false;
		}

		Pools.AddPool(poolName, std::move(names));
	}

	if (Pools.Empty())
	{
		ErrorMessage = FilePath + ": no name pools were found.";
		return false;
	}

	return true;
}
} // namespace CivSimCore
