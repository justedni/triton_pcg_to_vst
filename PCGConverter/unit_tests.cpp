#include "unit_tests.h"

#include "helpers.h"
#include "alchemist.h"
#include "pcg_converter.h"

#include <sstream>
#include <map>
#include <iostream>
#include <filesystem>
#include <assert.h>
#include <chrono>
#include <stdint.h>
#include <string>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

bool doUnitTest(const PCG_Converter* converterTemplate, KorgPCG* pcg, EPatchMode type, const std::string& unitTestFolder,
	const std::string& pcgBankLetter, int pcgProgId, const std::string& patchRefName, std::string& outLog)
{
	auto isIgnoredIFXParamForUnitTest = [](const std::string& paramName, int ifxId, bool ifxState)
	{
		auto ifxPos = paramName.find("ifx");
		assert(ifxPos != std::string::npos);
		int paramIfxId = atoi(paramName.substr(ifxPos + 3, 1).c_str());

		if (ifxId == paramIfxId && !ifxState) // Disabled IFX : ignore param
		{
			return (paramName.find("control_channel") != std::string::npos
				|| paramName.find("chain") != std::string::npos
				|| paramName.find("send_1_level") != std::string::npos
				|| paramName.find("send_2_level") != std::string::npos
				);
		}

		return false;
	};

	auto isIgnoredParamForUnitTest = [](const std::string& paramName)
	{
		// There's a bug in the VST: factory Combis are not setting this param correctly (it's different from the value in the Program)
		return (paramName.find("combi_timbre") != std::string::npos && paramName.find("low_start_offset") != std::string::npos);
	};

	bool bErrors = false;

	KorgBanks* container = (type == EPatchMode::Program) ? pcg->Program : pcg->Combination;
	std::string subfolder = (type == EPatchMode::Program) ? "Program" : "Combi";
	std::string jsonPrefix = (type == EPatchMode::Program) ? "prog_" : "combi_";

	auto converter = PCG_Converter(*converterTemplate, unitTestFolder);

	KorgBank* foundBank = nullptr;

	for (uint32_t i = 0; i < container->count; i++)
	{
		auto* bank = container->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(bank->bank);

		if (bankLetter == pcgBankLetter)
		{
			foundBank = bank;
			break;
		}
	}

	assert(pcgProgId >= 0 && pcgProgId < 128);
	assert(foundBank);
	auto* item = foundBank->item[pcgProgId];
	std::string pcgPresetName = std::string((char*)item->data, 16);

	std::stringstream generatedStream;
	converter.patchToStream(type, 0, pcgProgId, pcgPresetName, (unsigned char*)item->data, "A", generatedStream);

	auto refPath = unitTestFolder + subfolder + "\\" + patchRefName;

	std::ifstream refIfs(refPath);
	if (!refIfs.is_open())
	{
		std::cerr << "Reference json file " << refPath << " not found!!\n";
		assert(refIfs.is_open());
	}

	rapidjson::IStreamWrapper refisw{ refIfs };
	rapidjson::Document refJson;
	refJson.ParseStream(refisw);

	rapidjson::Document generatedJson;
	generatedJson.Parse(generatedStream.str().c_str());

	auto refGeneralInfo = refJson["general_program_information"].GetObject();
	auto generatedGeneralInfo = generatedJson["general_program_information"].GetObject();

	auto removeStrSpaces = [](auto& str)
	{
		str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
	};

	auto compareFields = [&](auto& fieldName, auto& refField, auto& generatedField)
	{
		std::string refVal = refField.IsString() ? refField.GetString() : std::to_string(refField.GetInt());
		std::string generatedVal = generatedField.IsString() ? generatedField.GetString() : std::to_string(generatedField.GetInt());

		removeStrSpaces(refVal);
		removeStrSpaces(generatedVal);

		if (refVal != generatedVal)
		{
			std::stringstream ss;
			ss << "\terror: General Info " << fieldName << ": " << refVal << " != " << generatedVal;
			outLog.append(ss.str());

			bErrors = true;
		}

		return (refVal == generatedVal);
	};

	for (auto it = refGeneralInfo.begin(); it != refGeneralInfo.end(); it++)
	{
		auto fieldName = it->name.GetString();

		if (std::string(fieldName).find("characters") != std::string::npos
			|| std::string(fieldName).find("user_sample_name_osc") != std::string::npos)
			continue;

		if (std::string(fieldName).find("timbres") != std::string::npos)
		{
			auto refTimbres = refGeneralInfo[fieldName].GetArray();
			auto generatedTimbres = generatedGeneralInfo[fieldName].GetArray();

			for (uint32_t timbreId = 0; timbreId < refTimbres.Size(); timbreId++)
			{
				auto refTimbre = refTimbres[timbreId].GetObject();
				auto genTimbre = generatedTimbres[timbreId].GetObject();

				for (auto itSubField = refTimbre.begin(); itSubField != refTimbre.end(); itSubField++)
				{
					auto subFieldName = itSubField->name.GetString();

					if (std::string(subFieldName).find("bank_name") == std::string::npos
						&& std::string(subFieldName).find("user_sample_name_osc") == std::string::npos)
					{
						auto success = compareFields(subFieldName, refTimbre[subFieldName], genTimbre[subFieldName]);
						if (!success)
						{
							std::stringstream ss;
							ss << "(" << refTimbre["bank_name"].GetString() << ":" << refTimbre["program_number"].GetInt() << ")\n";
							outLog.append(ss.str());
						}
					}
				}
			}
		}
		else
		{
			auto success = compareFields(fieldName, refGeneralInfo[fieldName], generatedGeneralInfo[fieldName]);
			if (!success)
				outLog.append("\n");
		}
	}

	auto refPresetName = std::string(refGeneralInfo["name"].GetString());
	auto generatedPresetName = std::string(generatedGeneralInfo["name"].GetString());

	removeStrSpaces(refPresetName);
	removeStrSpaces(generatedPresetName);
	assert(refPresetName.compare(generatedPresetName.c_str()) == 0);

	auto refParams = refJson["dsp_settings"].GetArray();
	auto generatedParams = generatedJson["dsp_settings"].GetArray();

	bool bPrintErrorHeader = true;

	std::map<int, bool> ifxStates;

	const int generatedParamsCount = generatedParams.Size();
	const int refParamsCount = refParams.Size();

	int generatedParamId = 0;
	for (int refParamId = 0; refParamId < refParamsCount; )
	{
		auto& refParam = refParams[refParamId];

		auto refParamIndex = refParam["index"].GetInt();
		std::string refParamName = refParam["key"].GetString();
		auto refParamVal = refParam["value"].GetInt();

		auto ifxPos = refParamName.find("ifx");
		int ifxId = 0;
		if (ifxPos != std::string::npos)
		{
			ifxId = atoi(refParamName.substr(ifxPos + 3, 1).c_str());

			if (refParamName.find("onoff") != std::string::npos)
			{
				ifxStates[ifxId] = refParamVal;
			}
		}

		if (generatedParamId >= generatedParamsCount)
			break;

		auto& generatedParam = generatedParams[generatedParamId];
		auto generatedParamIndex = generatedParam["index"].GetInt();

		assert(generatedParamIndex >= refParamIndex);
		if (generatedParamIndex > refParamIndex)
		{
			while (generatedParamIndex != refParamIndex)
			{
				if (!Helpers::isIgnoredParam(refParamName))
				{
					std::stringstream ss;
					ss << "\n\terror: Missing Param " << refParamName << "(" << refParamVal << ")";
					outLog.append(ss.str());
				}

				refParamId++;
				refParam = refParams[refParamId];
				refParamIndex = refParam["index"].GetInt();
			}
		}

		if (!Helpers::isIgnoredParam(refParamName)
			&& (!ifxId || !isIgnoredIFXParamForUnitTest(refParamName, ifxId, ifxStates[ifxId]))
			&& !isIgnoredParamForUnitTest(refParamName))
		{
			auto generatedParamVal = generatedParam["value"].GetInt();
			if (refParamVal != generatedParamVal)
			{
				if (bPrintErrorHeader)
				{
					outLog.append("\t ---- Left: Ref Json ---- Right: Converted from PCG\n");
					bPrintErrorHeader = false;
				}

				std::stringstream ss;
				ss << "\terror: Param " << refParamName << ": " << refParamVal << " != " << generatedParamVal << "\n";
				outLog.append(ss.str());

				bErrors = true;
			}
		}

		refParamId++;
		generatedParamId++;
	}

	return !bErrors;
}

void doUnitTests()
{
	auto processTests = [](const std::string& subfolder, auto type)
	{
		const std::string unitTestFolderRoot = "..\\..\\UnitTest\\" + subfolder + "\\";
		const std::string refPCGPath = "Data\\Factory_" + subfolder + ".PCG";

		EnumKorgModel model;
		auto* pcg = LoadTritonPCG(refPCGPath.c_str(), model);
		assert(pcg);

		assert((subfolder == "Triton" && model == EnumKorgModel::KORG_TRITON)
			|| (subfolder == "TritonExtreme" && model == EnumKorgModel::KORG_TRITON_EXTREME));

		auto converterTemplate = PCG_Converter(pcg->model, pcg, "");

		std::string typeStr = type == EPatchMode::Combi ? "Combi" : "Program";
		std::cout << "\n### Unit Tests for: " << subfolder << " - " << typeStr << "### \n";

		const std::string unitTestsFolder = unitTestFolderRoot + typeStr + "\\";

		for (const auto& entry : std::filesystem::directory_iterator(unitTestsFolder))
		{
			if (!entry.is_regular_file())
				continue;

			if (entry.path().extension() != ".patch")
				continue;

			auto filename = entry.path().filename().string();
			auto pcg_bank = filename.substr(0, 1);
			auto pcg_program = atoi(filename.substr(1, 3).c_str());

			std::stringstream patchNameStrm;
			patchNameStrm << pcg_bank << std::setw(3) << std::setfill('0') << pcg_program;

			auto patchName = patchNameStrm.str();;
			patchNameStrm << ".patch";

			std::string outLog;
			auto retValue = doUnitTest(&converterTemplate, pcg, type, unitTestFolderRoot, pcg_bank, pcg_program, patchNameStrm.str(), outLog);

			std::cout << patchName << (retValue ? ": OK " : ": ERRORS:\n");

			if (!outLog.empty())
				std::cout << outLog;
		}
	};

	auto t1 = std::chrono::high_resolution_clock::now();

	processTests("TritonExtreme", EPatchMode::Combi);
	processTests("TritonExtreme", EPatchMode::Program);
	processTests("Triton", EPatchMode::Combi);
	processTests("Triton", EPatchMode::Program);

	auto t2 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> ms_double = t2 - t1;
	std::cout << "\n\nFinished Units Tests in " << std::round((int)(ms_double.count() / 1000)) << "sec \n";
}
