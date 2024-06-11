#include "pcg_converter.h"

#include "alchemist.h"
#include "helpers.h"

#include <sstream>
#include <iostream>

std::vector<std::string> PCG_Converter::vst_bank_letters = { "A", "B", "C", "D" };

PCG_Converter::PCG_Converter(
	EnumKorgModel model,
	KorgPCG* pcg,
	const std::string destFolder)
	: m_model(model)
	, m_pcg(pcg)
	, m_destFolder(destFolder)
{
	retrieveTemplatesData();
	retrieveProgramNamesList();
}

PCG_Converter::PCG_Converter(
	EnumKorgModel model)
	: m_model(model)
	, m_pcg(nullptr)
	, m_destFolder("..\\UnitTest\\")
{
	retrieveTemplatesData();
}

void PCG_Converter::retrieveTemplatesData()
{
	using namespace nlohmann;

	auto parse = [](auto& path, auto& target)
	{
		std::ifstream f(path);
		if (!f.is_open())
		{
			std::cerr << "Template " << path << " not found!!\n";
			assert(f.is_open());
		}
		
		target = json::parse(f);
	};

	if (m_model == EnumKorgModel::KORG_TRITON_EXTREME)
	{
		parse("Extreme_Program.patch", m_templateProg);
		parse("Extreme_Combi.patch", m_templateCombi);
	}
	else
	{
		parse("Triton_Program.patch", m_templateProg);
		parse("Triton_Combi.patch", m_templateCombi);
	}

	auto& progDspSettings = m_templateProg["dsp_settings"];
	for (auto& setting: progDspSettings)
	{
		m_dictProgParams.emplace_back(setting["index"].get<int>(), setting["key"], setting["value"].get<int>());
	}

	auto& combiDspSettings = m_templateCombi["dsp_settings"];
	for (auto& setting : combiDspSettings)
	{
		m_dictCombiParams.emplace_back(setting["index"].get<int>(), setting["key"], setting["value"].get<int>());
	}
}

void PCG_Converter::retrieveProgramNamesList()
{
	m_mapProgramsNames.clear();

	for (uint32_t i = 0; i < m_pcg->Program->count; i++)
	{
		auto* prog = m_pcg->Program->bank[i];
		for (uint32_t j = 0; j < prog->count; j++)
		{
			auto* item = prog->item[j];
			auto name = std::string((char*)item->data, 16);
			auto uniqueId = Helpers::getUniqueId(Helpers::bankIdToLetter(prog->bank), j);
			m_mapProgramsNames[uniqueId] = name;
		}
	}
}

KorgBank* PCG_Converter::findDependencyBank(KorgPCG* pcg, int depBank)
{
	auto depBankLetter = Helpers::pcgProgBankIdToLetter(depBank);
	KorgBank* foundBank = nullptr;

	for (uint32_t i = 0; i < pcg->Program->count; i++)
	{
		auto* prog = pcg->Program->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(prog->bank);
		if (bankLetter == depBankLetter)
		{
			foundBank = prog;
			break;
		}
	}

	return foundBank;
}

void PCG_Converter::convertPrograms(const std::vector<std::string>& letters)
{
	int currentUserBank = 0;

	for (uint32_t i = 0; i < m_pcg->Program->count; i++)
	{
		auto* prog = m_pcg->Program->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(prog->bank);

		if (std::find(letters.begin(), letters.end(), bankLetter) == letters.end())
			continue;

		auto targetLetter = vst_bank_letters[currentUserBank];
		auto userFolder = Helpers::createSubfolders(m_destFolder, "Program", targetLetter);

		for (uint32_t j = 0; j < prog->count; j++)
		{
			auto* item = prog->item[j];
			auto name = std::string((char*)item->data, 16);

			auto msg = utils::string_format("%s::%d Patching %s\n", Helpers::bankIdToLetter(prog->bank).c_str(), j, name.c_str());
			std::cout << msg;

			patchProgramToJson(prog->bank, j, name, item->data, userFolder, targetLetter);
		}

		currentUserBank++;
	}
}

void PCG_Converter::convertCombis(const std::vector<std::string>& letters)
{
	int currentUserBank = 0;

	for (uint32_t i = 0; i < m_pcg->Combination->count; i++)
	{
		auto* bank = m_pcg->Combination->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(bank->bank);

		if (std::find(letters.begin(), letters.end(), bankLetter) == letters.end())
			continue;

		auto targetLetter = vst_bank_letters[currentUserBank];
		auto userFolder = Helpers::createSubfolders(m_destFolder, "Combi", targetLetter);

		for (uint32_t jPreset = 0; jPreset < bank->count; jPreset++)
		{
			auto* item = bank->item[jPreset];
			auto name = std::string((char*)item->data, 16);

			patchCombiToJson(bank->bank, jPreset, name, item->data, userFolder, targetLetter);
		}

		currentUserBank++;
	}
}

struct twoBits
{
	char d : 2;
};

template<typename T>
int getBits(void* data, int offset, int bitstart, int bitend)
{
	T* s = (T*)data + offset;
	unsigned bitmask = (1 << (bitend - bitstart + 1)) - 1;
	return (T)((*s >> bitstart) & bitmask);
}

int getBits(TritonStruct::EVarType type, void* data, int offset, int bitstart, int bitend)
{
	if (type == TritonStruct::EVarType::Signed)
		return getBits<char>(data, offset, bitstart, bitend);
	else
		return getBits<unsigned char>(data, offset, bitstart, bitend);
}

int PCG_Converter::getPCGValue(unsigned char* data, TritonStruct& info)
{
	int result = getBits(info.varType, data, info.pcgOffset, info.pcgBitStart, info.pcgBitEnd);

	// Signed 2-bit values
	if ((info.pcgBitEnd - info.pcgBitStart == 1) && info.pcgLSBOffset == -1 && info.varType == TritonStruct::EVarType::Signed)
	{
		twoBits temp = static_cast<twoBits>(result);
		result = static_cast<int>(temp.d);
	}

	if (info.pcgLSBOffset >= 0)
	{
		result <<= info.pcgMSBBitShift;
		result += getBits(info.varType, data, info.pcgLSBOffset, info.pcgLSBBitStart, info.pcgLSBBitEnd);
	}

	return result;
}

void PCG_Converter::jsonWriteHeaderBegin(std::ofstream& json, const std::string& presetName)
{
	json << "{\"version\": 1280, \"general_program_information\": {";
	json << "\"name\": \"" << presetName << "\", \"category\": \"Keyboard        \", \"categoryIndex\": 0, ";
	json << "\"author\": \"\", ";
}

void PCG_Converter::jsonWriteHeaderEnd(std::ofstream& json, int presetId, int bankNumber,
	const std::string& targetLetter, const std::string& mode)
{
	json << "\"mode\": \"" << mode << "\", \"bank\": \"USER-" << targetLetter << "\", ";
	json << "\"number\": " << presetId << ", \"triton_bank_number\": " << bankNumber << ", ";
	json << "\"triton_program_number\": " << presetId << ", \"characters\": {\"arp_basic_phrase\": 0, ";
	json << "\"bright_dark\": 1, \"fast_slow\": 1, \"mono_poly\": 2, \"percussive_gate\": 2, \"pitch_mod_filter_mod\": 0, \"type\": 4}}, ";
}

void PCG_Converter::jsonWriteTimbers(std::ofstream& json, const std::vector<Timber>& timbers)
{
	json << "\"timbres\": [";

	bool bAddComma = false;
	for(auto& timber : timbers)
	{
		if (bAddComma) json << ", ";
		json << "{\"timbre_name\":\"" << timber.programName << "\", \"bank_name\" : \"" << timber.bankName;
		json << "\", \"program_number\" : " << timber.programId << "}";

		if (!bAddComma) bAddComma = true;
	}

	json << " ],";
}

void PCG_Converter::jsonWriteEnd(std::ofstream& json, const std::string& presetType)
{
	json << "], \"data_type\": \"" << presetType << "\"}";
	json << std::endl;
}

std::string getOutputPatchPath(int presetId, const std::string& userFolder)
{
	std::string outFilePath;
	std::ostringstream ss;
	ss << userFolder << std::setw(3) << std::setfill('0') << presetId << ".patch";
	return ss.str();
}

void PCG_Converter::jsonWriteDSPSettings(std::ofstream& json, const PCG_Converter::ParamList& content)
{
	json << "\"dsp_settings\": [";

	bool bAddComma = false;
	for (auto& param : content)
	{
		if (bAddComma) json << ", ";
		json << "{\"index\": " << param.index << ", \"key\": \"" << param.key << "\", \"value\": " << param.value << "}";

		if (!bAddComma) bAddComma = true;
	}
}

void PCG_Converter::patchProgramToJson(int bankId, int presetId, const std::string& presetName, unsigned char* data,
	const std::string& userFolder, const std::string& targetLetter)
{
	auto outFilePath = getOutputPatchPath(presetId, userFolder);
	std::ofstream json(outFilePath);
	auto bankNumber = Helpers::getVSTBankNumber(EPatchMode::Program, targetLetter, m_model);
	jsonWriteHeaderBegin(json, presetName);
	jsonWriteHeaderEnd(json, presetId, bankNumber, targetLetter, "Program");

	auto& content = m_dictProgParams;
	patchInnerProgram(content, "prog_", data, presetName, EPatchMode::Program);
	jsonWriteDSPSettings(json, content);

	jsonWriteEnd(json, "program");
}

void PCG_Converter::patchEffect(PCG_Converter::ParamList& content, int dataOffset, unsigned char* data, int effectId, const std::string& prefix)
{
	auto found = std::find_if(effect_conversions.begin(), effect_conversions.end(), [&effectId](auto& e)
		{ return e.id == effectId; });
	if (found != effect_conversions.end())
	{
		for (auto& spec_conv : found->conversions)
		{
			if (spec_conv.jsonParam.empty())
				continue;

			auto info = TritonStruct(spec_conv);
			info.pcgOffset += dataOffset;
			if (info.pcgLSBOffset != -1)
				info.pcgLSBOffset += dataOffset;

			auto pcgVal = getPCGValue(data, info);
			auto jsonName = utils::string_format("%s_%s", prefix.c_str(), spec_conv.jsonParam.c_str());

			auto found = std::find_if(content.begin(), content.end(), [&jsonName](auto& e) { return e.key == jsonName; });
			assert(found != content.end());
			found->value = pcgVal;
		}
	}
}

void PCG_Converter::patchValue(PCG_Converter::ParamList& content, const std::string& jsonName, int value)
{
	auto found = std::find_if(content.begin(), content.end(), [&jsonName](auto& e) { return e.key == jsonName; });
	assert(found != content.end());
	found->value = value;
}

void PCG_Converter::patchSharedConversions(PCG_Converter::ParamList& content, const std::string& prefix, unsigned char* data)
{
	for (auto& conversion : shared_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, jsonName, pcgVal);

		if (conversion.jsonParam.find("effect_type") != std::string::npos)
		{
			int index = conversion.jsonParam.find("mfx1") != std::string::npos ? 1 : 2;
			int startOffset = conversion.jsonParam.find("mfx1") != std::string::npos ? 136 : 156;
			auto fxprefix = utils::string_format("%smfx%d", prefix.c_str(), index);
			patchEffect(content, startOffset, data, pcgVal, fxprefix);
		}
	}

	for (auto& ifx_struct : program_ifx_offsets)
	{
		for (auto& conversion : program_ifx_conversions)
		{
			if (conversion.jsonParam.empty())
				continue;

			TritonStruct info = TritonStruct(conversion);
			info.pcgOffset += ifx_struct.startOffset;
			if (info.pcgLSBOffset != -1)
				info.pcgLSBOffset += ifx_struct.startOffset;

			auto jsonName = utils::string_format("%sifx%d_%s", prefix.c_str(), ifx_struct.id, conversion.jsonParam.c_str());
			auto pcgVal = getPCGValue(data, info);

			patchValue(content, jsonName, pcgVal);

			if (info.jsonParam.find("effect_type") != std::string::npos)
			{
				auto fxprefix = utils::string_format("%sifx%d", prefix.c_str(), ifx_struct.id);
				patchEffect(content, ifx_struct.startOffset, data, pcgVal, fxprefix);
			}
		}
	}
}

void PCG_Converter::patchInnerProgram(PCG_Converter::ParamList& content, const std::string& prefix, unsigned char* data,
	const std::string& progName, EPatchMode mode)
{
	for (auto& conversion : program_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, jsonName, pcgVal);
	}

	if (mode == EPatchMode::Program) // Program shared data (IFX, MFX, Valve..) not used in Combi mode
	{
		patchSharedConversions(content, prefix, data);

		for (auto& conversion : triton_extreme_prog_conversions)
		{
			if (conversion.jsonParam.empty())
				continue;

			auto pcgVal = getPCGValue(data, conversion);
			auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());

			patchValue(content, jsonName, pcgVal);
		}
	}

	static const int NUM_OSC = 2;
	for (int iOscId = 0; iOscId < NUM_OSC; iOscId++)
	{
		for (auto& conversion : program_osc_conversions)
		{
			if (conversion.jsonParam.empty())
				continue;

			TritonStruct info = TritonStruct(conversion);
			if (iOscId == 1)
			{
				info.pcgOffset += 154;
				if (info.pcgLSBOffset != -1)
					info.pcgLSBOffset += 154;
			}

			auto jsonName = utils::string_format("%sosc_%d_%s", prefix.c_str(), iOscId + 1, conversion.jsonParam.c_str());
			auto pcgVal = getPCGValue(data, info);

			if (conversion.optionalConverter)
				pcgVal = conversion.optionalConverter(pcgVal, jsonName, data);

			patchValue(content, jsonName, pcgVal);
		}
	}
}

void PCG_Converter::patchCombiToJson(int bankId, int presetId,
	const std::string& presetName, unsigned char* data, const std::string& userFolder, const std::string& targetLetter)
{
	auto msg = utils::string_format("%s::%d Patching %s\n", Helpers::bankIdToLetter(bankId).c_str(), presetId, presetName.c_str());
	std::cout << msg;

	auto& content = m_dictCombiParams;

	for (auto& conversion : combi_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, conversion.jsonParam, pcgVal);
	}

	patchSharedConversions(content, "combi_", data);

	for (auto& conversion : triton_extreme_combi_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("combi_%s", conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, jsonName, pcgVal);
	}

	std::array<PCG_Converter::Prog, 8> associatedPrograms;

	for (auto& timbre : combi_timbres)
	{
		for (auto& conversion : combi_timbre_conversions)
		{
			if (conversion.jsonParam.empty())
				continue;

			auto jsonName = utils::string_format("combi_timbre_%d_%s", timbre.id, conversion.jsonParam.c_str());

			TritonStruct timbreInfo = TritonStruct(conversion);
			timbreInfo.pcgOffset += timbre.startOffset;
			if (timbreInfo.pcgLSBOffset != -1)
			{
				timbreInfo.pcgLSBOffset += timbre.startOffset;
			}
			
			auto pcgVal = getPCGValue(data, timbreInfo);
			patchValue(content, jsonName, pcgVal);

			if (conversion.jsonParam.find("program_no") != std::string::npos)
			{
				associatedPrograms[timbre.id - 1].program = pcgVal;
			}
			else if (conversion.jsonParam.find("program_bank") != std::string::npos)
			{
				associatedPrograms[timbre.id - 1].bank = pcgVal;
			}
		}
	}

	std::vector<Timber> timbersToWrite;

	int iTimber = 0;
	for (auto& prog : associatedPrograms)
	{
		std::string programName = "Unknown";
		auto letter = Helpers::pcgProgBankIdToLetter(prog.bank);
		auto uniqueId = Helpers::getUniqueId(letter, prog.program);
		if (m_mapProgramsNames.find(uniqueId) != m_mapProgramsNames.end())
		{
			programName = m_mapProgramsNames[uniqueId];
		}

		auto timberBankName = Helpers::getVSTProgramBankName(prog.bank, m_model);
		timbersToWrite.emplace_back(timberBankName, prog.program, programName);

		if (auto* progBank = findDependencyBank(m_pcg, prog.bank))
		{
			auto* progItem = progBank->item[prog.program];
			auto depProgName = std::string((char*)progItem->data, 16);

			auto prefix = utils::string_format("combi_timbre_%d_", iTimber + 1);
			patchInnerProgram(content, prefix, progItem->data, depProgName, EPatchMode::Combi);
		}

		iTimber++;
	}

	auto outFilePath = getOutputPatchPath(presetId, userFolder);
	std::ofstream json(outFilePath);
	auto bankNumber = Helpers::getVSTBankNumber(EPatchMode::Combi, targetLetter, m_model);

	jsonWriteHeaderBegin(json, presetName);
	jsonWriteTimbers(json, timbersToWrite);
	jsonWriteHeaderEnd(json, presetId, bankNumber, targetLetter, "Combi");
	jsonWriteDSPSettings(json, content);

	jsonWriteEnd(json, "combi");
}

bool PCG_Converter::unitTest(EnumKorgModel model, const std::string& inPCGBinName, const std::string& inVSTRefName)
{
	const auto unitTestFolder = std::string("..\\..\\UnitTest\\");
	auto pcgPath = unitTestFolder + inPCGBinName;
	std::ifstream file(pcgPath, std::ios::in | std::ios::binary | std::ios::ate);
	assert(file.is_open());

	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(fileSize);
	auto& st = file.read(buffer.data(), fileSize);
	assert(st);

	char* data = buffer.data();
	auto name = std::string((char*)data, 16);

	auto converter = PCG_Converter(EnumKorgModel::KORG_TRITON_EXTREME);
	converter.patchProgramToJson(0, 0, name, (unsigned char*)data, unitTestFolder, "A");

	bool bErrors = false;

	auto parseJson = [](auto path)
	{
		std::ifstream f(path);
		if (!f.is_open())
		{
			std::cerr << "Reference path " << path << " not found!!\n";
			assert(f.is_open());
		}

		return nlohmann::json::parse(f);
	};

	auto refPath = unitTestFolder + inVSTRefName;
	auto leftJson = parseJson(refPath);
	auto generatedPath = unitTestFolder + "000.patch";
	auto rightJson = parseJson(generatedPath);

	auto& left_settings = leftJson["dsp_settings"];
	auto& right_settings = rightJson["dsp_settings"];

	int paramId = 0;
	for (auto& param : left_settings)
	{
		auto leftIndex = param["index"].get<int>();
		auto leftName = param["key"];
		auto leftVal = param["value"].get<int>();

		auto rightParam = right_settings[paramId];
		auto rightIndex = rightParam["index"].get<int>();
		auto rightName = rightParam["key"];
		auto rightVal = rightParam["value"].get<int>();
		if (leftVal != rightVal)
		{
			std::cerr << "\terror: Param " << param["key"] << ": " << leftVal << " != " << rightVal << "\n";
			bErrors = true;
			assert(false);
		}

		paramId++;
	}

	std::remove(generatedPath.c_str());

	return !bErrors;
}