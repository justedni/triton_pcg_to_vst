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

#include "include/rapidjson/document.h"
#include "include/rapidjson/istreamwrapper.h"

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

	bool bErrors = false;

	KorgBanks* container = (type == EPatchMode::Program) ? pcg->Program : pcg->Combination;
	std::string subfolder = (type == EPatchMode::Program) ? "Program" : "Combi";
	std::string jsonPrefix = (type == EPatchMode::Program) ? "prog_" : "combi_";

	auto converter = PCG_Converter(*converterTemplate, unitTestFolder);

	KorgBank* foundBank = nullptr;

	for (int i = 0; i < container->count; i++)
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
			ss << "\terror: General Info " << fieldName << ": " << refVal << " != " << generatedVal << "\n";
			outLog.append(ss.str());

			bErrors = true;
		}
	};

	for (auto it = refGeneralInfo.begin(); it != refGeneralInfo.end(); it++)
	{
		auto fieldName = it->name.GetString();

		if (std::string(fieldName).find("characters") != std::string::npos)
			continue;

		if (std::string(fieldName).find("timbres") != std::string::npos)
		{
			auto refTimbres = refGeneralInfo[fieldName].GetArray();
			auto generatedTimbres = generatedGeneralInfo[fieldName].GetArray();

			for (int timbreId = 0; timbreId < refTimbres.Size(); timbreId++)
			{
				auto refTimbre = refTimbres[timbreId].GetObject();
				auto genTimbre = generatedTimbres[timbreId].GetObject();

				for (auto itSubField = refTimbre.begin(); itSubField != refTimbre.end(); itSubField++)
				{
					auto subFieldName = itSubField->name.GetString();

					if (std::string(subFieldName).find("bank_name") == std::string::npos)
						compareFields(subFieldName, refTimbre[subFieldName], genTimbre[subFieldName]);
				}
			}
		}
		else
		{
			compareFields(fieldName, refGeneralInfo[fieldName], generatedGeneralInfo[fieldName]);
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
					ss << "\n\terror: Missing Param " << refParamName << "(" << refParamVal << ")" << "\n";
					outLog.append(ss.str());
				}

				refParamId++;
				refParam = refParams[refParamId];
				refParamIndex = refParam["index"].GetInt();
			}
		}

		if (!Helpers::isIgnoredParam(refParamName)
			&& (!ifxId || !isIgnoredIFXParamForUnitTest(refParamName, ifxId, ifxStates[ifxId])))
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
	auto t1 = std::chrono::high_resolution_clock::now();

	struct Test
	{
		std::string pcg_bank;
		int pcg_program;
	};

	auto processTests = [](const std::string& subfolder, auto type, const auto& prog_tests)
	{
		const std::string unitTestFolder = "..\\..\\UnitTest\\" + subfolder + "\\";
		const std::string refPCGPath = unitTestFolder + "Ref.PCG";

		EnumKorgModel model;
		auto* pcg = LoadTritonPCG(refPCGPath.c_str(), model);
		assert(pcg);

		assert((subfolder == "Triton" && model == EnumKorgModel::KORG_TRITON)
			|| (subfolder == "TritonExtreme" && model == EnumKorgModel::KORG_TRITON_EXTREME));

		auto converterTemplate = PCG_Converter(pcg->model, pcg, "");

		for (auto& test : prog_tests)
		{
			std::stringstream patchNameStrm;
			patchNameStrm << test.pcg_bank << std::setw(3) << std::setfill('0') << test.pcg_program;

			auto patchName = patchNameStrm.str();;
			patchNameStrm << ".patch";

			std::string outLog;
			auto retValue = doUnitTest(&converterTemplate, pcg, type, unitTestFolder, test.pcg_bank, test.pcg_program, patchNameStrm.str(), outLog);

			std::cout << patchName << (retValue ? ": OK " : ": ERRORS:\n");

			if (!outLog.empty())
				std::cout << outLog;
		}
	};

	std::vector<Test> triton_ext_combi_tests = {};

	std::vector<Test> triton_ext_prog_tests = {};

	processTests("TritonExtreme", EPatchMode::Combi, triton_ext_combi_tests);
	processTests("TritonExtreme", EPatchMode::Program, triton_ext_prog_tests);

	std::vector<Test> triton_prog_tests = {};

	processTests("Triton", EPatchMode::Program, triton_prog_tests);

	auto t2 = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double, std::milli> ms_double = t2 - t1;
	std::cout << "Finished Units Tests in " << std::round((int)(ms_double.count() / 1000)) << "sec \n";
}
