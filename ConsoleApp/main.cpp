#include <iostream>

#include "alchemist.h"
#include "pcg_converter.h"
#include "helpers.h"

#include <map>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "unit_tests.h"

using string_map = std::unordered_map<std::string, std::vector<std::string>>;
template <class Keyword>
string_map generic_parse(std::vector<std::string> as, Keyword keyword)
{
	string_map result;

	std::string flag;
	for (auto&& x : as)
	{
		auto f = keyword(x);
		if (f.empty())
		{
			result[flag].push_back(x);
		}
		else
		{
			flag = f.front();
			result[flag];
			flag = f.back();
		}
	}
	return result;
}

void printUsage()
{
	std::cout << "Usage:\n"
		<< "-PCG <Path> : path of the PCG file to open\n"
		<< "-OutFolder <Path> : path of the destination folder\n"
		<< "-Combi <Letters> : combis to export (max:4). Ex: -Combi A C D M\n"
		<< "-Program <Letters> : programs to export (max:4). Ex: -Program B D J\n"
		<< "[-unit_test] : performs unit test (optional)\n";
}

int main(int argc, const char* argv[])
{
	const char* kCombi = "-Combi";
	const char* kProgram = "-Program";
	const char* kPCG = "-PCG";
	const char* kOutFolder = "-OutFolder";
	const char* kUnitTestArg = "-unit_test";

	std::vector<std::string> args(argv + 1, argv + argc);

	std::unordered_map<std::string, std::string> keys = {
		{ kCombi, kCombi },
		{ kProgram, kProgram },
		{ kPCG, kPCG },
		{ kOutFolder, kOutFolder },
		{ kUnitTestArg, kUnitTestArg }
	};

	auto result = generic_parse(args, [&](auto&& s) -> std::vector<std::string> {
		if (keys.count(s) > 0)
			return { keys[s] };
		else
			return {};
	});

	if (result.find(kUnitTestArg) != result.end())
	{
		doUnitTests();
		return 0;
	}

	if ((result.find(kPCG) == result.end() || result[kPCG].empty()))
	{
		std::cerr << "Please enter the path of a PCG file to read!\n";
		printUsage();
		return -1;
	}

	if ((result.find(kOutFolder) == result.end() || result[kOutFolder].empty()))
	{
		std::cerr << "Please enter the path for the destination folder!\n";
		printUsage();
		return -1;
	}

	if ((result.find(kCombi) == result.end() || result[kCombi].empty())
		&& (result.find(kProgram) == result.end() || result[kProgram].empty()))
	{
		std::cerr << "Nothing to export! Missing -Combi or -Program parameter!\n";
		printUsage();
		return -1;
	}

	if (result[kCombi].size() > 4 || result[kProgram].size() > 4)
	{
		std::cerr << "Too many combis or programs selected for export! The Triton VST only has 4 user banks for each category.\n";
		return -1;
	}

	auto& programsToExport = result[kProgram];
	auto& combisToExport = result[kCombi];
	auto& pcgPath = result[kPCG][0];
	auto& destFolder = result[kOutFolder][0];

	if (!std::filesystem::exists(pcgPath))
	{
		std::cerr << "Input PCG file doesn't exist on disk!\n";
		return -1;
	}

	EnumKorgModel model;
	auto* pcg = LoadTritonPCG(pcgPath.c_str(), model);

	if (!pcg)
	{
		std::cerr << "Input PCG file is invalid or corrupted!\n";
		return -1;
	}

	auto converter = PCG_Converter(
		model,
		pcg,
		destFolder);

	auto process = [](auto& selected, auto&& func)
	{
		if (!selected.empty())
		{
			int currentUserBank = 0;

			std::vector<std::string> letters;
			std::vector<int> targetIds;
			for (auto& arg : selected)
			{
				letters.push_back(arg);
				targetIds.push_back(currentUserBank);
				currentUserBank++;
			}
			func(letters, targetIds);
		}
	};

	process(result[kProgram], [&](const auto& letters, const auto& targets) { converter.convertPrograms(letters, targets); });
	process(result[kCombi], [&](const auto& letters, const auto& targets) { converter.convertCombis(letters, targets); });

	return 0;
}
