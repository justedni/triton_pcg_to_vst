#include "pcg_converter.h"

#include "alchemist.h"
#include "helpers.h"

#include <sstream>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <array>
#include <format>
#include <regex>

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

std::vector<std::string> PCG_Converter::vst_bank_letters = { "A", "B", "C", "D" };
std::map<std::string, int> PCG_Converter::m_mapProgram_keyToId;
std::map<std::string, int> PCG_Converter::m_mapCombi_keyToId;

std::vector<char> PCG_Converter::m_gmData;
std::vector<PCG_Converter::GMBankData> PCG_Converter::m_mappedGMInfo;

const int CustomProgramBufferSize = 540;

PCG_Converter::PCG_Converter(
	EnumKorgModel model,
	KorgPCG* pcg,
	const std::string destFolder,
	std::function<void(const std::string&)>&& func)
	: m_pcg(pcg)
	, m_targetModel(model)
	, m_destFolder(destFolder)
	, m_logFunc(std::move(func))
{
	initEffectConversions();

	if (!retrieveTemplatesData())
		return;

	if (!retrieveGMData())
		return;

	if (!retrieveFactoryPCG())
		return;

	m_initialized = true;
}

PCG_Converter::PCG_Converter(const PCG_Converter& other, const std::string destFolder)
	: m_pcg(other.m_pcg)
	, m_targetModel(other.m_targetModel)
	, m_destFolder(destFolder)
{
	m_dictProgParams = other.m_dictProgParams;
	m_dictCombiParams = other.m_dictCombiParams;
}

template<typename T>
T readBytes(std::ifstream& stream)
{
	T a;
	stream.read((char*)&a, sizeof(T));
	return a;
}

bool PCG_Converter::retrieveTemplatesData()
{
	auto parse = [&](std::string path, auto& target)
	{
		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			auto errMsg = std::format("Critical error: Template {} not found!!\n", path.c_str());
			error(errMsg);
			return false;
		}
		else
		{
			rapidjson::IStreamWrapper refisw{ ifs };
			target.ParseStream(refisw);
			return true;
		}
	};

	rapidjson::Document templateProgDoc;
	if (!parse("Data\\PatchTemplate_Program.patch", templateProgDoc))
		return false;

	rapidjson::Document templateCombiDoc;
	if (!parse("Data\\PatchTemplate_Combi.patch", templateCombiDoc))
		return false;

	static bool initIdMaps = true;

	auto getAllData = [](auto& doc, auto& out_dict, auto& out_map)
	{
		auto dspSettings = doc["dsp_settings"].GetArray();
		for (auto& setting : dspSettings)
		{
			auto key = setting["key"].GetString();
			auto id = setting["index"].GetInt();
			out_dict[id] = ProgParam(key, setting["value"].GetInt());
			if (initIdMaps)
				out_map[key] = id;
		}
	};

	getAllData(templateProgDoc, m_dictProgParams, m_mapProgram_keyToId);
	getAllData(templateCombiDoc, m_dictCombiParams, m_mapCombi_keyToId);

	initIdMaps = false;
	return true;
}

bool PCG_Converter::retrieveGMData()
{
	if (!m_mappedGMInfo.empty())
		return true;

	std::ifstream file("Data\\Factory_GM_Programs.bin", std::ios::in | std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		error("Critical error: Factory_GM_Programs data not found!!\n");
		return false;
	}

	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	m_gmData.resize(fileSize);
	file.read(m_gmData.data(), fileSize);

	static std::vector<std::string> kDrumKitNames = {
		"STANDARD", "ROOM", "POWER", "ELECTRONIC", "ANALOG", "JAZZ", "BRUSH", "ORCHESTRA", "SFX" };

	auto removeStrSpaces = [](auto& str)
	{
		const auto strEnd = str.find_last_not_of(" ");
		return str.substr(0, strEnd + 1);
	};

	struct GMPreset {
		std::string presetName;
		int dataOffset = 0;
	};
	std::vector<GMPreset> gmPresetsInfo;

	constexpr const int regularChunkSize = 540;
	constexpr const int drumkitChunkSize = 4112;

	int currentOffset = 0;
	char* ptr = m_gmData.data();
	while (currentOffset < fileSize)
	{
		auto presetName = std::string(ptr, 16);
		gmPresetsInfo.emplace_back(presetName, currentOffset);

		if (std::find(kDrumKitNames.begin(), kDrumKitNames.end(), removeStrSpaces(presetName)) != kDrumKitNames.end())
		{
			ptr += drumkitChunkSize;
			currentOffset += drumkitChunkSize;
		}
		else
		{
			ptr += regularChunkSize;
			currentOffset += regularChunkSize;
		}
	}

	int currentId = 0;
	for (auto& info : m_gmInfo)
	{
		auto& letter = info.bank;
		auto* bankDef = Helpers::findBankDef([&letter](auto& e) { return e.name == letter; });
		assert(bankDef);

		auto bankId = bankDef->shortId;
		auto programId = info.id - 1;
		auto name = removeStrSpaces(info.name);
		auto factoryProgram = std::find_if(gmPresetsInfo.begin(), gmPresetsInfo.end(),
			[&](auto& e) { return removeStrSpaces(e.presetName) == name; });
		assert(factoryProgram != gmPresetsInfo.end());

		m_mappedGMInfo.emplace_back(bankId, programId, factoryProgram->dataOffset);
	}

	return true;
}

bool PCG_Converter::retrieveFactoryPCG()
{
	EnumKorgModel model;
	KorgPCG* pcg = nullptr;

	if (m_pcg->model == EnumKorgModel::KORG_TRITON_EXTREME)
		pcg = LoadTritonPCG("Data\\Factory_TritonExtreme.PCG", model);
	else
		pcg = LoadTritonPCG("Data\\Factory_Triton.PCG", model);

	if (!pcg)
	{
		error("Critical error: Factory PCG file is invalid or corrupted!\n");
		return false;
	}
	else
	{
		assert(model == m_pcg->model);
		m_factoryPcg = pcg;
		return true;
	}
}

KorgBank* PCG_Converter::findDependencyBank(KorgPCG* pcg, int depBank)
{
	auto depBankLetter = Helpers::pcgProgBankIdToLetter(depBank);
	KorgBank* foundBank = nullptr;

	if (pcg->Program)
	{
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
	}

	return foundBank;
}

void PCG_Converter::log(const std::string& text)
{
	if (m_logFunc)
		m_logFunc(text);
	else
		std::cout << text;
}

void PCG_Converter::error(const std::string& text)
{
	if (m_logFunc)
		m_logFunc(text);
	else
		std::cerr << text;
}

void PCG_Converter::convertPrograms(const std::vector<std::string>& letters)
{
	if (!m_initialized)
		return;

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

			std::stringstream msgStrm;
			msgStrm << "Program " << Helpers::bankIdToLetter(prog->bank) << ":" << std::setw(3) << std::setfill('0') << j;
			msgStrm << " " << name << "\n";
			log(msgStrm.str());

			patchProgramToJson(prog->bank, j, name, item->data, userFolder, targetLetter);
		}

		currentUserBank++;
	}
}

void PCG_Converter::convertCombis(const std::vector<std::string>& letters)
{
	if (!m_initialized)
		return;

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

			std::stringstream msgStrm;
			msgStrm << "Combi " << Helpers::bankIdToLetter(bank->bank) << ":" << std::setw(3) << std::setfill('0') << jPreset;
			msgStrm << " " << name << "\n";
			log(msgStrm.str());

			patchCombiToJson(bank->bank, jPreset, name, item->data, userFolder, targetLetter);
		}

		currentUserBank++;
	}
}

template<typename T>
T getBits_Impl(void* data, int offset, int bitstart, int bitend)
{
	assert(bitend >= bitstart);
	T* s = (T*)data + offset;
	unsigned bitmask = (1 << (bitend - bitstart + 1)) - 1;
	return (T)((*s >> bitstart) & bitmask);
}

short getBits(EVarType type, void* data, int offset, int bitstart, int bitend)
{
	if (type == EVarType::Signed)
		return getBits_Impl<char>(data, offset, bitstart, bitend);
	else
		return getBits_Impl<unsigned char>(data, offset, bitstart, bitend);
}

struct twoBits { char d : 2; };
struct threeBits { char d : 3; };
struct fourBits { char d : 4; };
struct fiveBits { char d : 5; };
struct sixBits { char d : 6; };
struct sevenBits { char d : 7; };
struct eightBits { char d : 8; };
struct nineBits { short d : 9; };
struct tenBits { short d : 10; };
struct elevenBits { short d : 11; };
struct twelveBits { short d : 12; };
struct thirteenBits { short d : 13; };
struct sixteenBits { short d : 16; };

template<typename S, typename T>
T convertOddValue(T val)
{
	S temp = static_cast<S>(val);
	return static_cast<T>(temp.d);
}

int convertValue(int val, int numBits)
{
	if (numBits == 2)
		return convertOddValue<twoBits>(val);
	else if (numBits == 3)
		return convertOddValue<threeBits>(val);
	else if (numBits == 4)
		return convertOddValue<fourBits>(val);
	else if (numBits == 5)
		return convertOddValue<fiveBits>(val);
	else if (numBits == 6)
		return convertOddValue<sixBits>(val);
	else if (numBits == 7)
		return convertOddValue<sevenBits>(val);
	else if (numBits == 8)
		return convertOddValue<eightBits>(val);
	else if (numBits == 9)
		return convertOddValue<nineBits>(val);
	else if (numBits == 10)
		return convertOddValue<tenBits>(val);
	else if (numBits == 11)
		return convertOddValue<elevenBits>(val);
	else if (numBits == 12)
		return convertOddValue<twelveBits>(val);
	else if (numBits == 13)
		return convertOddValue<thirteenBits>(val);
	else if (numBits == 16)
		return convertOddValue<sixteenBits>(val);

	return val;
}

int PCG_Converter::getPCGValue(unsigned char* data, TritonStruct& info)
{
	auto varType = (info.pcgLSBOffset >= 0 || info.third.has_value()) ? EVarType::Unsigned : info.varType;
	int result = getBits(varType, data, info.pcgOffset, info.pcgBitStart, info.pcgBitEnd);

	auto numBits = info.pcgBitEnd - info.pcgBitStart + 1;
	if (info.pcgLSBOffset >= 0)
	{
		short bitShift = (info.pcgLSBBitEnd - info.pcgLSBBitStart + 1);
		result <<= bitShift;

		int lsbVal = static_cast<int>(getBits(varType, data, info.pcgLSBOffset, info.pcgLSBBitStart, info.pcgLSBBitEnd));
		result |= lsbVal;

		numBits += bitShift;
	}

	if (info.third.has_value())
	{
		short bitShift = (info.third->bit_end - info.third->bit_start + 1);
		result <<= bitShift;

		int lsbVal = static_cast<int>(getBits(info.varType, data, info.third->offset, info.third->bit_start, info.third->bit_end));
		result |= lsbVal;

		numBits += bitShift;
	}

	// Convert values with odd num bits
	if (info.varType == EVarType::Signed)
	{
		result = convertValue(result, numBits);
	}

	return result;
}

std::string getPresetNameSafe(const std::string& presetName)
{
	auto safePresetName = std::string(presetName);
	std::string backslash = "\"";
	std::string escapedBackslash = "\\\"";

	std::vector<size_t> positions;

	size_t pos = safePresetName.find(backslash, 0);
	while (pos != std::string::npos)
	{
		positions.push_back(pos);
		pos = safePresetName.find(backslash, pos + 1);
	}

	int total = static_cast<int>(positions.size());
	if (total > 0)
	{
		for (int i = total - 1; i >= 0; i--)
		{
			safePresetName.replace(positions[i], backslash.size(), escapedBackslash);
		}
	}

	return safePresetName;
}

std::pair<int, std::string> PCG_Converter::getCategory(EPatchMode mode, EnumKorgModel model, const PCG_Converter::ParamList& content)
{
	const bool isCombi = (mode == EPatchMode::Combi);
	const bool isTritonCombi = isCombi && model != EnumKorgModel::KORG_TRITON_EXTREME;
	auto index = isCombi ? 5702 : 6474; // combi_category / prog_common_category
	auto found = content.find(index);
	assert(found != content.end());
	auto categoryId = found->second.value;

	std::string name;

	switch (categoryId)
	{
	case 0:  name = "Keyboard"; break;
	case 1:  name = "Organ"; break;
	case 2:  name = isTritonCombi ? "Bell/Mallet/Perc" : "Bell/Mallet"; break;
	case 3:  name = "Strings"; break;
	case 4:  name = isTritonCombi ? "BrassReed" : "Vocal/Airy"; break;
	case 5:  name = isTritonCombi ? "Orchestral" : "Brass"; break;
	case 6:  name = isTritonCombi ? "World" : "Woodwind/Reed"; break;
	case 7:  name = "Guitar/Plucked"; break;
	case 8:  name = isTritonCombi ? "Pads" : "Bass"; break;
	case 9:  name = isTritonCombi ? "MotionSynth" : "SlowSynth"; break;
	case 10: name = isTritonCombi ? "Synth" : "FastSynth"; break;
	case 11: name = isTritonCombi ? "LeadSplits" : "LeadSynth"; break;
	case 12: name = isTritonCombi ? "BassSplits" : "MotionSynth"; break;
	case 13: name = isTritonCombi ? "Complex & SE" : "SE"; break;
	case 14: name = isTritonCombi ? "RhythmicPattern" : "Hit/Arpg"; break;
	case 15: name = isTritonCombi ? "Ds/Hits" : "Drums"; break;
	default:
		assert(false);
	}

	auto whitespace = std::string(16 - name.length(), ' ');
	name += whitespace;
	return { categoryId, name };
}

void PCG_Converter::jsonWriteHeaderBegin(std::ostream& json, const std::string& presetName, EPatchMode mode, EnumKorgModel model, const PCG_Converter::ParamList& content)
{
	auto safePresetName = getPresetNameSafe(presetName);
	auto [categoryId, categoryName] = getCategory(mode, model, content);
	json << "{\"version\": 1280, \"general_program_information\": {";
	json << "\"name\": \"" << safePresetName << "\", \"category\": \"" << categoryName << "\", \"categoryIndex\": " << categoryId << ", ";
	json << "\"author\": \"\", ";
}

void PCG_Converter::jsonWriteHeaderEnd(std::ostream& json, int presetId, int bankNumber,
	const std::string& targetLetter, const std::string& mode)
{
	json << "\"mode\": \"" << mode << "\", \"bank\": \"USER-" << targetLetter << "\", ";
	json << "\"number\": " << presetId << ", \"triton_bank_number\": " << bankNumber << ", ";
	json << "\"triton_program_number\": " << presetId << ", \"characters\": {\"arp_basic_phrase\": 0, ";
	json << "\"bright_dark\": 1, \"fast_slow\": 1, \"mono_poly\": 2, \"percussive_gate\": 2, \"pitch_mod_filter_mod\": 0, \"type\": 4}}, ";
}

void PCG_Converter::jsonWriteTimbers(std::ostream& json, const std::vector<Timber>& timbers)
{
	json << "\"timbres\": [";

	bool bAddComma = false;
	for(auto& timber : timbers)
	{
		if (bAddComma) json << ", ";
		auto safeProgramName = getPresetNameSafe(timber.programName);
		json << "{\"timbre_name\":\"" << safeProgramName << "\", \"bank_name\" : \"" << timber.bankName;
		json << "\", \"program_number\" : " << timber.programId << "}";

		if (!bAddComma) bAddComma = true;
	}

	json << " ],";
}

void PCG_Converter::jsonWriteEnd(std::ostream& json, const std::string& presetType)
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

void PCG_Converter::jsonWriteDSPSettings(std::ostream& json, const PCG_Converter::ParamList& content)
{
	json << "\"dsp_settings\": [";

	bool bAddComma = false;
	for (auto& param : content)
	{
		if (bAddComma) json << ", ";
		json << "{\"index\": " << param.first << ", \"key\": \"" << param.second.key << "\", \"value\": " << param.second.value << "}";

		if (!bAddComma) bAddComma = true;
	}
}

void PCG_Converter::patchProgramToStream(int bankId, int presetId, const std::string& presetName, unsigned char* data,
	const std::string& targetLetter, std::ostream& out_stream)
{
	const auto mode = EPatchMode::Program;
	auto& content = m_dictProgParams;
	auto bankNumber = Helpers::getVSTBankNumber(mode, targetLetter, m_targetModel);

	patchInnerProgram(content, "prog_", data, presetName, mode);
	patchArpeggiator(content, "prog_user_arp_", "prog_arpeggiator_pattern_no.", data, mode);
	patchDrumKit(content, "prog_", data, mode);
	patchProgramUnusedValues(mode, content, "prog_");

	jsonWriteHeaderBegin(out_stream, presetName, mode, m_targetModel, content);
	jsonWriteHeaderEnd(out_stream, presetId, bankNumber, targetLetter, "Program");
	jsonWriteDSPSettings(out_stream, content);
	jsonWriteEnd(out_stream, "program");
}

void PCG_Converter::patchProgramToJson(int bankId, int presetId, const std::string& presetName, unsigned char* data,
	const std::string& userFolder, const std::string& targetLetter)
{
	auto outFilePath = getOutputPatchPath(presetId, userFolder);
	std::ofstream json(outFilePath);
	patchProgramToStream(bankId, presetId, presetName, data, targetLetter, json);
}

void PCG_Converter::patchEffect(EPatchMode mode, PCG_Converter::ParamList& content, int dataOffset, unsigned char* data, int effectId, const std::string& prefix)
{
	if (effectId == 0) // No effect
		return;

	auto foundEntry = effect_conversions.find(effectId);
	if (foundEntry != effect_conversions.end())
	{
		for (const auto& spec_conv : foundEntry->second)
		{
			if (spec_conv.jsonParam.empty())
				continue;

			auto info = TritonStruct(spec_conv);
			info.pcgOffset += dataOffset;
			if (info.pcgLSBOffset != -1)
				info.pcgLSBOffset += dataOffset;
			if (info.third.has_value())
				info.third->offset += dataOffset;

			auto jsonName = utils::string_format("%s_specific_parameter_%s", prefix.c_str(), spec_conv.jsonParam.c_str());
			auto pcgVal = getPCGValue(data, info);

			patchValue(mode, content, jsonName, pcgVal);
		}
	}
	else
	{
		auto msg = std::format("  Unhandled effect {} \n", effectId);
		log(msg);
	}
}

PCG_Converter::ProgParam* PCG_Converter::findParamByKey(EPatchMode mode, PCG_Converter::ParamList& content, const std::string& key)
{
	auto& m = (mode == EPatchMode::Combi) ? m_mapCombi_keyToId : m_mapProgram_keyToId;
	auto id = m.find(key);
	assert(id != m.end());
	auto found = content.find(id->second);
	assert(found != content.end());
	return &(found->second);
}

void PCG_Converter::patchValue(EPatchMode mode, PCG_Converter::ParamList& content, const std::string& jsonName, int value)
{
	auto* found = findParamByKey(mode, content, jsonName);
	found->value = value;
}

void PCG_Converter::patchCombiUnusedValues(PCG_Converter::ParamList& content, const std::string& prefix)
{
	struct NameValue { std::string name; int value; };

	std::vector<NameValue> ignoredParams =
	{
		{ "arpeggiator_gate_control", 0 },
		{ "arpeggiator_velocity_control", 0 },
		{ "arpeggiator_tempo", 40 },
		{ "arpeggiator_switch", 0 },
		{ "arpeggiator_pattern_no.", 0 },
		{ "arpeggiator_resolution", 0 },
		{ "arpeggiator_octave", 0 },
		{ "arpeggiator_gate", 0 },
		{ "arpeggiator_velocity", 1 },
		{ "arpeggiator_swing", 0 },
		{ "arpeggiator_sort_onoff", 0 },
		{ "arpeggiator_latch_onoff", 0 },
		{ "arpeggiator_keysync_onoff", 0 },
		{ "arpeggiator_keyboard_onoff", 0 },
		{ "arpeggiator_top_key", 0 },
		{ "arpeggiator_bottom_key", 0 },
		{ "arpeggiator_top_velocity", 1 },
		{ "arpeggiator_bottom_velocity", 1 },
		{ "osc_1_low_start_offset", 0 },
		{ "osc_2_low_start_offset", 0 }
	};

	for (auto& ignoredParam : ignoredParams)
	{
		std::string fullParamName = prefix + ignoredParam.name;
		auto* found = findParamByKey(EPatchMode::Combi, content, fullParamName);
		found->value = ignoredParam.value;
	}
}

void PCG_Converter::patchProgramUnusedValues(EPatchMode mode, PCG_Converter::ParamList& content, const std::string& prefix)
{
	struct NameValue { std::string name; int value; };

	struct ProgramPatch
	{
		NameValue param;
		std::vector<NameValue> paramsToUpdate;
	};

	auto fullParamName = [&](auto& name)
	{
		return prefix + name;
	};

	std::vector<ProgramPatch> allPatches =
	{
		{ { "common_oscillator_mode", 2 }, {{ "osc_1_output_use_drum_kit_setting", 1 }} },
		{ { "osc_1_low_sample_no.", 4095 }, {{ "osc_1_low_sample_no.", 999 }} }, // N/A patch
		{ { "osc_2_low_sample_no.", 4095 }, {{ "osc_2_low_sample_no.", 999 }} }, // N/A patch
	};

	if (mode == EPatchMode::Program || prefix.find("combi_timbre") != std::string::npos)
	{
		for (auto& patch : allPatches)
		{
			auto paramName = fullParamName(patch.param.name);
			auto* found = findParamByKey(mode, content, paramName);
			if (found->value == patch.param.value)
			{
				for (auto& paramToUpdate : patch.paramsToUpdate)
				{
					std::string targetName = fullParamName(paramToUpdate.name);
					auto* foundTarget = findParamByKey(mode, content, targetName);
					foundTarget->value = paramToUpdate.value;
				}
			}
		}
	}

	// Patching knobs default value (only for program or global combi, not timber)
	if (prefix.find("combi_timbre") == std::string::npos)
	{
		for (int i = 1; i <= 4; i++)
		{
			std::string paramName;
			if (prefix.find("combi_") != std::string::npos)
				paramName = utils::string_format("%sknob%d_assign_type", prefix.c_str(), i);
			else
				paramName = utils::string_format("%scommon_knob%d_assign_type", prefix.c_str(), i);

			auto* foundAssign = findParamByKey(mode, content, paramName);
			auto assignedKnob = foundAssign->value;
			{
				std::string targetName = utils::string_format("%sknob%d", prefix.c_str(), i);
				auto* found = findParamByKey(mode, content, targetName);

				// 0:Off; 1-4: Assignable Knob; 5+: assigned params
				switch (assignedKnob)
				{
				case 7: // Volume
					found->value = 100;
					break;
				case 10: // Expression
					found->value = 127;
					break;
				case 8: // Post IFX Pan
				case 9: // Pan
				case 13: // LPF Cutoff
				case 14: // Resonance/HPF
				case 15: // Filter EG Int.
				case 16: // Filter/Amp Attack
				case 17: // Filter/Amp Decay
				case 18: // Filter/Amp Sustain
				case 19: // Filter/Amp Release
				case 20: // LFO1 Speed
				case 21: // LFO1 Pitch Depth
					found->value = 64;
					break;
				default:
					found->value = 0;
				}
			}
		}
	}
}

void PCG_Converter::postPatchCombi(ParamList& content)
{
	for (auto& timbre : combi_timbres)
	{
		auto jsonName = utils::string_format("combi_timbre_%d_program_bank", timbre.id);
		auto* found = findParamByKey(EPatchMode::Combi, content, jsonName);
		if (found->value >= 6 && found->value <= 16) // GM banks
			found->value -= 2;
		else if (found->value >= 17)
			found->value--;
	}
}

void PCG_Converter::patchSharedConversions(EPatchMode mode, PCG_Converter::ParamList& content, const std::string& prefix, unsigned char* data)
{
	for (auto& conversion : shared_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(mode, content, jsonName, pcgVal);

		if (conversion.jsonParam.find("effect_type") != std::string::npos)
		{
			int index = conversion.jsonParam.find("mfx1") != std::string::npos ? 1 : 2;
			int startOffset = conversion.jsonParam.find("mfx1") != std::string::npos ? 136 : 156;
			auto fxprefix = utils::string_format("%smfx%d", prefix.c_str(), index);
			patchEffect(mode, content, startOffset, data, pcgVal, fxprefix);
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

			patchValue(mode, content, jsonName, pcgVal);

			if (info.jsonParam.find("effect_type") != std::string::npos)
			{
				auto fxprefix = utils::string_format("%sifx%d", prefix.c_str(), ifx_struct.id);
				patchEffect(mode, content, ifx_struct.startOffset, data, pcgVal, fxprefix);
			}
		}
	}
}

void PCG_Converter::patchArpeggiator(PCG_Converter::ParamList& content, const std::string& prefix,
	const std::string& patternNoKey, unsigned char* data, EPatchMode mode)
{
	auto* foundRef = findParamByKey(mode, content, patternNoKey);
	uint32_t patternNo = foundRef->value;

	if (patternNo >= 0 && patternNo <= 4) // Factory patterns
	{
		auto patchEntry = [&](const auto& jsonName, auto val)
		{
			auto* found = findParamByKey(mode, content, jsonName);
			found->value = val;
		};

		patchEntry(utils::string_format("%spattern_parameter_length", prefix.c_str()), 1);
		patchEntry(utils::string_format("%spattern_parameter_tone_mode", prefix.c_str()), 0);
		patchEntry(utils::string_format("%spattern_parameter_fixed_note_mode", prefix.c_str()), 0);

		const int maxTones = 12;
		for (int i = 1; i <= maxTones; i++)
		{
			patchEntry(utils::string_format("%spattern_parameter_tone_note_no.%d", prefix.c_str(), i), 0);
		}

		const int maxSteps = 48;
		for (int i = 0; i < maxSteps; i++)
		{
			patchEntry(utils::string_format("%spattern_parameter_step_%d_gate", prefix.c_str(), i), 0);
			patchEntry(utils::string_format("%spattern_parameter_step_%d_velocity", prefix.c_str(), i), 1);

			for (int j = 0; j < maxTones; j++)
			{
				patchEntry(utils::string_format("%spattern_parameter_step_%d_tone_%d", prefix.c_str(), i, j), 0);
			}
		}
	}
	else
	{
		KorgBanks* arpBanks = m_pcg->Arpeggio;
		if (!m_pcg->Arpeggio)
		{
			static bool bWarnAboutArppegios = true;
			if (bWarnAboutArppegios)
			{
				log("  Important: no user arpeggiator patterns are stored in this PCG -> defaulting to factory PCG\n");
				log("  This message is only printed once.\n");
				bWarnAboutArppegios = false;
			}
			arpBanks = m_factoryPcg->Arpeggio;

			if (!arpBanks)
			{
				std::cerr << "\tFactory PCG doesn't contain user arpeggiators!";
				return;
			}
		}

		patternNo -= 5;

		KorgBank* foundBank = nullptr;
		for (uint32_t i = 0; i < arpBanks->count; i++)
		{
			auto* arpBank = arpBanks->bank[i];
			if (patternNo >= arpBank->count)
			{
				patternNo -= arpBank->count;
				continue;
			}
			foundBank = arpBank;
			break;
		}

		if (!foundBank)
		{
			log(std::format("  Couldn't find arp. pattern {} in PCG\n", foundRef->value));
		}
		else
		{
			auto* item = foundBank->item[patternNo];
			auto* arpData = item->data;

			for (auto& conversion : arpeggiator_global_conversions)
			{
				auto jsonName = utils::string_format("%spattern_parameter_%s", prefix.c_str(), conversion.jsonParam.c_str());
				auto pcgVal = getPCGValue(arpData, conversion);
				patchValue(mode, content, jsonName, pcgVal);
			}

			for (int iStep = 0; iStep < 48; iStep++)
			{
				for (auto& conversion : arpeggiator_step_conversions)
				{
					auto jsonName = utils::string_format("%spattern_parameter_step_%d_%s", prefix.c_str(), iStep, conversion.jsonParam.c_str());

					TritonStruct info = TritonStruct(conversion);
					info.pcgOffset += (6 * iStep);
					auto pcgVal = getPCGValue(arpData, info);
					patchValue(mode, content, jsonName, pcgVal);
				}
			}
		}
	}
}

void PCG_Converter::patchDrumKit(PCG_Converter::ParamList& content, const std::string& prefix, unsigned char* data, EPatchMode mode)
{
	auto* foundRef = findParamByKey(mode, content, utils::string_format("%scommon_oscillator_mode", prefix.c_str()));
	auto oscMode = foundRef->value;

	if (oscMode != 2) // 0:Single 1:Double 2:Drum kit
		return;

	foundRef = findParamByKey(mode, content, utils::string_format("%sosc_1_hi_sample_no.", prefix.c_str()));

	if (foundRef->value > 127)
		foundRef->value -= 9;

	const auto drumKitNo = foundRef->value;

	KorgBanks* drumkitBanks = m_pcg->Drumkit;
	if (!m_pcg->Drumkit)
	{
		static bool bWarnAboutDrumkits = true;
		if (bWarnAboutDrumkits)
		{
			log("  Important: no user Drum Kits are stored in this PCG -> defaulting to factory PCG\n");
			log("  This message is only printed once.\n");
			bWarnAboutDrumkits = false;
		}
		drumkitBanks = m_factoryPcg->Drumkit;

		if (!drumkitBanks)
		{
			log("  Factory PCG doesn't contain user Drum Kits!");
			return;
		}
	}

	uint32_t idLookup = drumKitNo;

	KorgBank* foundBank = nullptr;
	for (uint32_t i = 0; i < drumkitBanks->count; i++)
	{
		auto* bank = drumkitBanks->bank[i];
		if (idLookup >= bank->count)
		{
			idLookup -= bank->count;
			continue;
		}
		foundBank = bank;
		break;
	}

	if (!foundBank)
	{
		log(std::format("  Couldn't find user Drum kit {}\n", drumKitNo));
	}
	else
	{
		auto* item = foundBank->item[idLookup];
		auto* drumData = item->data;

		int noteId = 0;
		for (auto& note : drumkit_notes)
		{
			for (auto& conversion : drumkit_conversions)
			{
				auto jsonName = utils::string_format("%suser_drumkit_parameter_%s_%s", prefix.c_str(), note.c_str(), conversion.jsonParam.c_str());

				TritonStruct info = TritonStruct(conversion);
				info.pcgOffset += (32 * noteId);
				if (info.pcgLSBOffset != -1)
					info.pcgLSBOffset += (32 * noteId);

				auto pcgVal = getPCGValue(drumData, info);

				if (jsonName.find("higher_bank") != std::string::npos || jsonName.find("lower_bank") != std::string::npos)
					pcgVal = convertOSCBank(pcgVal, jsonName, data);

				patchValue(mode, content, jsonName, pcgVal);
			}

			noteId++;
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
		patchValue(mode, content, jsonName, pcgVal);
	}

	if (mode == EPatchMode::Program) // Program shared data (IFX, MFX, Valve..) not used in Combi mode
	{
		patchSharedConversions(mode, content, prefix, data);

		if (m_pcg->model == EnumKorgModel::KORG_TRITON_EXTREME)
		{
			for (auto& conversion : triton_extreme_conversions)
			{
				if (conversion.jsonParam.empty())
					continue;

				auto pcgVal = getPCGValue(data, conversion);
				auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());

				patchValue(mode, content, jsonName, pcgVal);
			}
		}
		else
		{
			for (auto& conversion : triton_extreme_conversions)
			{
				auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
				patchValue(mode, content, jsonName, 0);
			}

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

			if (jsonName.find("hi_bank") != std::string::npos || jsonName.find("low_bank") != std::string::npos)
				pcgVal = convertOSCBank(pcgVal, jsonName, data);

			patchValue(mode, content, jsonName, pcgVal);
		}
	}
}

void PCG_Converter::patchCombiToJson(int bankId, int presetId,
	const std::string& presetName, unsigned char* data, const std::string& userFolder, const std::string& targetLetter)
{
	auto outFilePath = getOutputPatchPath(presetId, userFolder);
	std::ofstream json(outFilePath);
	patchCombiToStream(bankId, presetId, presetName, data, targetLetter, json);
}

void PCG_Converter::patchToStream(EPatchMode mode, int bankId, int presetId, const std::string& presetName, unsigned char* data,
	const std::string& targetLetter, std::ostream& out_stream)
{
	if (mode == EPatchMode::Program)
	{
		patchProgramToStream(0, presetId, presetName, (unsigned char*)data, "A", out_stream);
	}
	else
	{
		patchCombiToStream(0, presetId, presetName, (unsigned char*)data, "A", out_stream);
	}
}

void PCG_Converter::patchCombiToStream(int bankId, int presetId, const std::string& presetName, unsigned char* data,
		const std::string& targetLetter, std::ostream& out_stream)
{
	auto& content = m_dictCombiParams;
	const auto mode = EPatchMode::Combi;

	for (auto& conversion : combi_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto pcgVal = getPCGValue(data, conversion);
		patchValue(mode, content, conversion.jsonParam, pcgVal);
	}

	patchSharedConversions(mode, content, "combi_", data);

	for (auto& conversion : triton_extreme_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("combi_%s", conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(mode, content, jsonName, pcgVal);
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

			if (jsonName.find("hi_bank") != std::string::npos || jsonName.find("low_bank") != std::string::npos)
				pcgVal = convertOSCBank(pcgVal, jsonName, data);

			patchValue(mode, content, jsonName, pcgVal);

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

	// Global fields to patch (IFX...)
	patchProgramUnusedValues(mode, content, "combi_");

	std::vector<Timber> timbersToWrite;

	int iTimber = 0;
	for (auto& prog : associatedPrograms)
	{
		std::string programName = "Unknown";
		auto prefix = utils::string_format("combi_timbre_%d_", iTimber + 1);

		auto processed = false;
		auto* progBank = findDependencyBank(m_pcg, prog.bank);
		if (!progBank)
		{
			static bool bWarnAboutFactoryBanks = true;
			if (bWarnAboutFactoryBanks)
			{
				if (!m_pcg->Program)
					log("  Important: this PCG doesn't contain any Programs -> defaulting to factory PCG\n");
				else
					log(std::format("  Important: the program dependency (timber {}: {}:{}) couldn't be found on this PCG -> defaulting to factory PCG\n",
						iTimber, prog.bank, prog.program));
				log("  This message is only printed once.\n");
				bWarnAboutFactoryBanks = false;
			}

			progBank = findDependencyBank(m_factoryPcg, prog.bank);
		}

		if (progBank)
		{
			auto* progItem = progBank->item[prog.program];
			auto depProgName = std::string((char*)progItem->data, 16);
			patchInnerProgram(content, prefix, progItem->data, depProgName, EPatchMode::Combi);
			programName = depProgName;
			processed = true;
		}
		else if (Helpers::isGMBank(prog.bank))
		{
			// GM Banks are not saved in the PCG, we need to retrieve it ourselves
			auto found = std::find_if(m_mappedGMInfo.begin(), m_mappedGMInfo.end(), [&](auto& e) { return prog.bank == e.bankId && prog.program == e.programId; });
			if (found == m_mappedGMInfo.end() && prog.bank > 6)
			{
				// No specific variation for that GM bank, try to fallback to regular GM bank instead
				found = std::find_if(m_mappedGMInfo.begin(), m_mappedGMInfo.end(), [&](auto& e) { return e.bankId == 6 && prog.program == e.programId; });
			}
			
			if (found != m_mappedGMInfo.end())
			{
				unsigned char* data = (unsigned char*)m_gmData.data() + found->dataOffset;
				auto depProgName = std::string((char*)data, 16);
				patchInnerProgram(content, prefix, data, depProgName, EPatchMode::Combi);
				programName = depProgName;
				processed = true;
			}
		}

		if (!processed)
		{
			if (Helpers::isGMBank(prog.bank))
				log(std::format("  Couldn't locate GM program for timber {}: {}:{}\n", iTimber, prog.bank, prog.program));
			else
				log(std::format("  Unknown bank/program for timber {}: {}:{}\n", iTimber, prog.bank, prog.program));
		}
		else
		{
			patchDrumKit(content, prefix, data, mode);
			patchProgramUnusedValues(mode, content, prefix);
			patchCombiUnusedValues(content, prefix);
		}

		auto timberBankName = Helpers::getVSTProgramBankName(prog.bank, m_targetModel);
		timbersToWrite.emplace_back(timberBankName, prog.program, programName);

		iTimber++;
	}

	// Arpeggiators
	for (int i = 0; i < 2; i++)
	{
		auto arp_letter = i == 0 ? "a" : "b";
		auto prefix = utils::string_format("combi_user_arp_%s_", arp_letter);
		auto patternJsonName = utils::string_format("combi_arpeggiator_%s_pattern_no.", arp_letter);
		patchArpeggiator(content, prefix, patternJsonName, data, EPatchMode::Combi);
	}

	postPatchCombi(content);

	auto bankNumber = Helpers::getVSTBankNumber(EPatchMode::Combi, targetLetter, m_targetModel);

	jsonWriteHeaderBegin(out_stream, presetName, EPatchMode::Combi, m_targetModel, content);
	jsonWriteTimbers(out_stream, timbersToWrite);
	jsonWriteHeaderEnd(out_stream, presetId, bankNumber, targetLetter, "Combi");
	jsonWriteDSPSettings(out_stream, content);

	jsonWriteEnd(out_stream, "combi");
}

void PCG_Converter::convertProgramJsonToBin(PCG_Converter::ParamList& content, const std::string& programName, std::ostream& outStream)
{
	auto mode = EPatchMode::Program;

	constexpr int bufferSize = CustomProgramBufferSize;
	char buffer[bufferSize];
	memset(buffer, 0x20, 16);
	memset(buffer + 16, 0, bufferSize - 16);

	assert(programName.length() <= 16);
	memcpy_s(buffer, 16, programName.c_str(), programName.length());

	auto swapTwoBytes = [](short value)
	{
		int firstByte = (value & 0x00FF) >> 0;
		int secondByte = (value & 0xFF00) >> 8;
		return (firstByte << 8 | secondByte);
	};

	auto swapThreeBytes = [](int value)
	{
		int firstByte = (value & 0x0000FF) >> 0;
		int secondByte = (value & 0x00FF00) >> 8;
		int thirdByte = (value & 0xFF0000) >> 16;

		return (firstByte << 16 | secondByte << 8 | thirdByte);
	};

	auto saveValue = [&](auto& jsonName, auto& conversion)
	{
		auto* found = findParamByKey(mode, content, jsonName);
		int offset = conversion.pcgOffset;

		if (conversion.third.has_value())
		{
			unsigned int temp = swapThreeBytes(found->value << conversion.third->bit_start);
			int* dest = (int*)(buffer + offset);
			*dest |= temp;
		}
		else if (conversion.pcgLSBOffset >= 0)
		{
			unsigned short temp = swapTwoBytes(found->value << conversion.pcgLSBBitStart);
			short* dest = (short*)(buffer + offset);
			*dest |= temp;
		}
		else
		{
			unsigned char temp = found->value << conversion.pcgBitStart;
			char* dest = buffer + offset;
			*dest |= temp;
		}

		return found->value;
	};

	auto saveEffet = [&](auto effectId, auto& fxPrefix, auto startOffset)
	{
		if (effectId == 0) // No effect
			return;

		auto foundEntry = effect_conversions.find(effectId);
		if (foundEntry != effect_conversions.end())
		{
			for (const auto& spec_conv : foundEntry->second)
			{
				if (spec_conv.jsonParam.empty())
					continue;

				auto info = TritonStruct(spec_conv);
				info.pcgOffset += startOffset;
				if (info.pcgLSBOffset != -1)
					info.pcgLSBOffset += startOffset;
				if (info.third.has_value())
					info.third->offset += startOffset;

				auto jsonName = utils::string_format("%s_specific_parameter_%s", fxPrefix.c_str(), spec_conv.jsonParam.c_str());
				saveValue(jsonName, info);
			}
		}
	};

	std::string prefix = "prog_";

	for (auto& conversion : program_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
		saveValue(jsonName, conversion);
	}

	for (auto& conversion : shared_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
		auto val = saveValue(jsonName, conversion);

		if (conversion.jsonParam.find("effect_type") != std::string::npos)
		{
			int index = conversion.jsonParam.find("mfx1") != std::string::npos ? 1 : 2;
			int startOffset = conversion.jsonParam.find("mfx1") != std::string::npos ? 136 : 156;
			auto fxprefix = utils::string_format("%smfx%d", prefix.c_str(), index);
			saveEffet(val, fxprefix, startOffset);
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
			auto val = saveValue(jsonName, info);

			if (info.jsonParam.find("effect_type") != std::string::npos)
			{
				auto fxprefix = utils::string_format("%sifx%d", prefix.c_str(), ifx_struct.id);
				saveEffet(val, fxprefix, ifx_struct.startOffset);
			}
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
			saveValue(jsonName, info);
		}
	}

	outStream.write((char*)buffer, bufferSize);
}

void PCG_Converter::utils_convertGMProgramsToBin(const std::string& sourceFolder, const std::string& outFile)
{
	std::ofstream os(outFile, std::ofstream::binary);

	uint8_t presetsCount = 0;
	auto writeInt = [](std::ostream& os, auto val) { os.write(reinterpret_cast<const char*>(&val), sizeof(val)); };
	writeInt(os, presetsCount);

	for (const auto& entry : std::filesystem::directory_iterator(sourceFolder))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".patch")
			continue;

		auto filename = entry.path().filename().string();
		auto pcg_bank = atoi(filename.substr(0, 2).c_str());
		auto pcg_program = atoi(filename.substr(3, 3).c_str());

		rapidjson::Document tempDoc;

		std::ifstream ifs(entry.path().string());
		assert(ifs.is_open());

		rapidjson::IStreamWrapper refisw{ ifs };
		tempDoc.ParseStream(refisw);

		ParamList allParams;

		auto dspSettings = tempDoc["dsp_settings"].GetArray();
		for (auto& setting : dspSettings)
		{
			auto key = setting["key"].GetString();
			auto id = setting["index"].GetInt();
			allParams[id] = ProgParam(key, setting["value"].GetInt());
		}

		writeInt(os, (uint8_t)pcg_bank);
		writeInt(os, (uint8_t)pcg_program);
		constexpr int programSize = CustomProgramBufferSize;
		writeInt(os, (uint16_t)programSize);

		std::string programName = tempDoc["general_program_information"]["name"].GetString();
		convertProgramJsonToBin(allParams, programName, os);
		presetsCount++;
	}

	os.seekp(0);
	writeInt(os, presetsCount);
}
