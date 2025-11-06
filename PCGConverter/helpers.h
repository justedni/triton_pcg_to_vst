#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <algorithm>

enum class EnumKorgModel : uint8_t;

namespace utils
{
	template<typename ... Args>
	std::string string_format(const std::string& format, Args ... args)
	{
		int size_s = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1;
		if (size_s <= 0) { throw std::runtime_error("Error during formatting."); }
		auto size = static_cast<size_t>(size_s);
		std::unique_ptr<char[]> buf(new char[size]);
		std::snprintf(buf.get(), size, format.c_str(), args ...);
		return std::string(buf.get(), buf.get() + size - 1);
	}
}

struct BankDef
{
	int shortId = -1;
	int regularId = -1;
	int hexId = -1;
	std::string name;
};

enum class EPatchMode : uint8_t { Program, Combi };

class Helpers
{
public:
	static std::string bankIdToLetter(int bankId);
	static std::string pcgProgBankIdToLetter(int bankId);
	static bool isGMBank(int bankId);
	static int bankPcgIdToId(int bankId);

	static int getVSTBankNumber(EPatchMode type, const std::string& letter, EnumKorgModel model);
	static std::string getVSTProgramBankName(int bankId, EnumKorgModel model);

	static std::string getUniqueId(std::string bankLetter, int presetId);

	static void createFolder(const std::string& path);
	static std::string createSubfolders(const std::string& destFolder, const std::string& subFolder, const std::string& bankLetter);

	static bool isIgnoredParam(const std::string& paramName);
	static bool isIgnoredIFXParamForUnitTest(const std::string& paramName, int ifxId, bool ifxState);

	template<typename F>
	static BankDef* findBankDef(F func)
	{
		auto found = std::find_if(bank_definitions.begin(), bank_definitions.end(), func);
		if (found != bank_definitions.end())
		{
			return &(*found);
		}

		return nullptr;
	}

private:
	static std::vector<BankDef> bank_definitions;
};
