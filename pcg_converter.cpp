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

#include "include/rapidjson/document.h"
#include "include/rapidjson/istreamwrapper.h"

std::vector<std::string> PCG_Converter::vst_bank_letters = { "A", "B", "C", "D" };
std::map<std::string, int> PCG_Converter::m_mapProgram_keyToId;
std::map<std::string, int> PCG_Converter::m_mapCombi_keyToId;
std::vector<PCG_Converter::GMBankData> PCG_Converter::m_gmPrograms;

const int CustomProgramBufferSize = 540;

PCG_Converter::PCG_Converter(
	EnumKorgModel model,
	KorgPCG* pcg,
	const std::string destFolder)
	: m_model(model)
	, m_pcg(pcg)
	, m_destFolder(destFolder)
{
	initEffectConversions();
	retrieveTemplatesData();
	retrieveProgramNamesList();
}

PCG_Converter::PCG_Converter(const PCG_Converter& other, const std::string destFolder)
	: m_model(other.m_model)
	, m_pcg(other.m_pcg)
	, m_destFolder(destFolder)
{
	m_dictProgParams = other.m_dictProgParams;
	m_dictCombiParams = other.m_dictCombiParams;
	m_mapProgramsNames = other.m_mapProgramsNames;
}

template<typename T>
T readBytes(std::ifstream& stream)
{
	T a;
	stream.read((char*)&a, sizeof(T));
	return a;
}

void PCG_Converter::retrieveTemplatesData()
{
	auto parse = [](std::string path, auto& target)
	{
		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			std::cerr << "Template " << path << " not found!!\n";
			assert(ifs.is_open());
		}

		rapidjson::IStreamWrapper refisw{ ifs };
		target.ParseStream(refisw);
	};

	rapidjson::Document templateProgDoc;
	rapidjson::Document templateCombiDoc;

	if (m_model == EnumKorgModel::KORG_TRITON_EXTREME)
	{
		parse("Extreme_Program.patch", templateProgDoc);
		parse("Extreme_Combi.patch", templateCombiDoc);
	}
	else
	{
		parse("Triton_Program.patch", templateProgDoc);
		parse("Triton_Combi.patch", templateCombiDoc);
	}

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

	// Retrieving GM Program data
	if (!m_gmPrograms.empty())
		return;

	std::ifstream file("GM_Data.bin", std::ios::in | std::ios::binary | std::ios::ate);
	if (!file.is_open())
		return;

	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	uint8_t numPrograms = readBytes<uint8_t>(file);

	for (int i = 0; i < numPrograms; i++)
	{
		uint8_t bankId = readBytes<uint8_t>(file);
		uint8_t programId = readBytes<uint8_t>(file);
		uint16_t bufferSize = readBytes<uint16_t>(file);

		std::vector<char> buffer(bufferSize);
		if (!file.read(buffer.data(), bufferSize))
			continue;

		m_gmPrograms.emplace_back(bankId, programId, std::move(buffer));
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

	for (int i = positions.size() - 1; i >= 0; i--)
	{
		safePresetName.replace(positions[i], backslash.size(), escapedBackslash);
	}

	return safePresetName;
}

std::pair<int, std::string> PCG_Converter::getCategory(EPatchMode mode, const PCG_Converter::ParamList& content)
{
	const bool isCombi = (mode == EPatchMode::Combi);
	auto index = isCombi ? 5702 : 6474; // combi_category / prog_common_category
	auto found = content.find(index);
	assert(found != content.end());
	auto categoryId = found->second.value;

	std::string name;

	switch (categoryId)
	{
	case 0:  name = "Keyboard"; break;
	case 1:  name = "Organ"; break;
	case 2:  name = "Bell/Mallet"; break;
	case 3:  name = "Strings"; break;
	case 4:  name = "Vocal/Airy"; break;
	case 5:  name = "Brass"; break;
	case 6:  name = "Woodwind/Reed"; break;
	case 7:  name = "Guitar/Plucked"; break;
	case 8:  name = "Bass"; break;
	case 9:  name = "Slow Synth"; break;
	case 10: name = "Fast Synth"; break;
	case 11: name = "Lead Synth"; break;
	case 12: name = "Motion Synth"; break;
	case 13: name = "SE"; break;
	case 14: name = "Hit/Arpg"; break;
	case 15: name = "Drums"; break;
	default:
		assert(false);
	}

	auto whitespace = std::string(16 - name.length(), ' ');
	name += whitespace;
	return { categoryId, name };
}

void PCG_Converter::jsonWriteHeaderBegin(std::ostream& json, const std::string& presetName, EPatchMode mode, const PCG_Converter::ParamList& content)
{
	auto safePresetName = getPresetNameSafe(presetName);
	auto [categoryId, categoryName] = getCategory(mode, content);
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
	auto bankNumber = Helpers::getVSTBankNumber(mode, targetLetter, m_model);

	patchInnerProgram(content, "prog_", data, presetName, EPatchMode::Program);
	patchProgramUnusedValues(mode, content, "prog_");

	jsonWriteHeaderBegin(out_stream, presetName, mode, content);
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
		auto msg = std::format(" Unhandled effect {} \n", effectId);
		std::cout << msg;
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
		{ "osc_2_low_start_offset", 0 },
		{ "knob1", 0 },
		{ "knob2", 0 },
		{ "knob3", 0 },
		{ "knob4", 0 },
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

		if (m_model == EnumKorgModel::KORG_TRITON_EXTREME)
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

			if (conversion.optionalConverter)
				pcgVal = conversion.optionalConverter(pcgVal, jsonName, data);

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
		auto letter = Helpers::pcgProgBankIdToLetter(prog.bank);
		auto uniqueId = Helpers::getUniqueId(letter, prog.program);
		if (m_mapProgramsNames.find(uniqueId) != m_mapProgramsNames.end())
		{
			programName = m_mapProgramsNames[uniqueId];
		}

		auto prefix = utils::string_format("combi_timbre_%d_", iTimber + 1);

		auto processed = false;
		auto* progBank = findDependencyBank(m_pcg, prog.bank);
		if (progBank)
		{
			auto* progItem = progBank->item[prog.program];
			auto depProgName = std::string((char*)progItem->data, 16);
			patchInnerProgram(content, prefix, progItem->data, depProgName, EPatchMode::Combi);
			processed = true;
		}
		else
		{
			// GM Banks are not saved in the PCG, we need to retrieve it ourselves
			auto found = std::find_if(m_gmPrograms.begin(), m_gmPrograms.end(), [&](auto& e) { return prog.bank == e.bankId && prog.program == e.programId; });
			if (found != m_gmPrograms.end())
			{
				unsigned char* data = (unsigned char*)found->data.data();
				auto depProgName = std::string((char*)data, 16);
				patchInnerProgram(content, prefix, data, depProgName, EPatchMode::Combi);
				programName = depProgName;
				processed = true;
			}
			else
			{
				std::cout << std::format(" Unknown bank/program for timber {}: {}:{}\n", iTimber, prog.bank, prog.program);
			}
		}

		if (processed)
		{
			patchProgramUnusedValues(mode, content, prefix);
			patchCombiUnusedValues(content, prefix);
		}

		auto timberBankName = Helpers::getVSTProgramBankName(prog.bank, m_model);
		timbersToWrite.emplace_back(timberBankName, prog.program, programName);

		iTimber++;
	}

	postPatchCombi(content);

	auto bankNumber = Helpers::getVSTBankNumber(EPatchMode::Combi, targetLetter, m_model);

	jsonWriteHeaderBegin(out_stream, presetName, EPatchMode::Combi, content);
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
