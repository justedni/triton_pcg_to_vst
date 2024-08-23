#include "helpers.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <assert.h>

#include <filesystem>

#include "alchemist.h"

std::vector<BankDef> Helpers::bank_definitions = {
	{ 0, 0, 0x0000, "A" },
	{ 1, 1, 0x0001, "B" },
	{ 2, 2, 0x0002, "C" },
	{ 3, 3, 0x0003, "D" },
	{ 4, 4, 0x0004, "E" },
	{ 5, 5, 0x08000, "F" },

	{ 6, 6, 0xF0000, "GM" },
	{ 7, -1, 0xF0001, "g(1)" },
	{ 8, -1, 0xF0002, "g(2)" },
	{ 9, -1, 0xF0003, "g(3)" },
	{ 10, -1, 0xF0004, "g(4)" },
	{ 11, -1, 0xF0005, "g(5)" },
	{ 12, -1, 0xF0006, "g(6)" },
	{ 13, -1, 0xF0007, "g(7)" },
	{ 14, -1, 0xF0008, "g(8)" },
	{ 15, -1, 0xF0009, "g(9)" },
	{ 16, -1, 0xF000A, "g(d)" },

	{ 17, 7, 0x20000, "H" },
	{ 18, 8, 0x20001, "I" },
	{ 19, 9,  0x20002, "J" },
	{ 20, 10, 0x20003, "K" },
	{ 21, 11, 0x20004, "L" },
	{ 22, 12, 0x20005, "M" },
	{ 23, 13, 0x20006, "N" }
};

std::string Helpers::pcgProgBankIdToLetter(int bankId)
{
	auto found = std::find_if(bank_definitions.begin(), bank_definitions.end(), [&bankId](auto& e) { return e.shortId == bankId; });
	if (found != bank_definitions.end())
		return found->name;

	assert(false && "Unknown bank Id");
	return "?";
}

std::string Helpers::bankIdToLetter(int bankId)
{
	auto found = std::find_if(bank_definitions.begin(), bank_definitions.end(), [&bankId](auto& e) { return e.hexId == bankId; });
	if (found != bank_definitions.end())
		return found->name;

	assert(false && "Unknown bank Id");
	return "?";
}

int Helpers::bankPcgIdToId(int bankId)
{
	auto found = std::find_if(bank_definitions.begin(), bank_definitions.end(), [&bankId](auto& e) { return e.shortId == bankId; });
	if (found != bank_definitions.end())
	{
		assert(found->regularId != -1);
		return found->regularId;
	}

	assert(false && "Unknown bank Id");
	return -1;
}

int Helpers::getVSTBankNumber(EPatchMode type, const std::string& letter, EnumKorgModel model)
{
	int startId = 0;
	switch (type)
	{
	case EPatchMode::Program:
	{
		if (model == EnumKorgModel::KORG_TRITON_EXTREME)
			startId = 23;
		else
			startId = 25;
		break;
	}
	case EPatchMode::Combi:
	{
		if (model == EnumKorgModel::KORG_TRITON_EXTREME)
			startId = 12;
		else
			startId = 14;
		break;
	}
	}

	if (letter == "A")
		return startId;
	else if (letter == "B")
		return startId + 1;
	else if (letter == "C")
		return startId + 2;
	else if (letter == "D")
		return startId + 3;

	assert(false && "Unknown bank Id");
	return -1;
}

std::string Helpers::getVSTProgramBankName(int bankId, EnumKorgModel model)
{
	if (model == EnumKorgModel::KORG_TRITON_EXTREME)
	{
		return Helpers::pcgProgBankIdToLetter(bankId);
	}
	else
	{
		std::string bank_name = "INT-";
		bank_name += Helpers::pcgProgBankIdToLetter(bankId);
		return bank_name;
	}
}

std::string Helpers::getUniqueId(std::string bankLetter, int presetId)
{
	std::ostringstream ss;
	ss << bankLetter << "::" << std::setw(3) << std::setfill('0') << presetId;
	return ss.str();
}

void Helpers::createFolder(const std::string& path)
{
	namespace fs = std::filesystem;

	if (!fs::is_directory(path) || !fs::exists(path))
	{
		fs::create_directory(path);
	}
}

std::string Helpers::createSubfolders(const std::string& destFolder, const std::string& subFolder, const std::string& bankLetter)
{
	createFolder(destFolder);

	auto basePath = destFolder;
	if (!basePath.empty() && *basePath.rbegin() != '\\')
		basePath += '\\';

	auto combiFolder = basePath + "\\" + subFolder;
	createFolder(combiFolder);

	std::ostringstream oss;
	oss << basePath << subFolder << "\\USER-" << bankLetter << "\\";
	auto userFolder = oss.str();
	createFolder(userFolder);

	return userFolder;
}

bool Helpers::isIgnoredParam(const std::string& paramName)
{
	auto contains = [&](const auto& str) { return paramName.find(str) != std::string::npos; };

	return ((contains("combi_timbre") &&
			(contains("arpeggiator_parameter") || contains("user_arp_pattern_name") || contains("specific_parameter")))
		);
}

