#include "unit_tests.h"

#include "helpers.h"
#include "alchemist.h"
#include "pcg_converter.h"

#include <sstream>
#include <map>
#include <iostream>
#include <assert.h>
#include <chrono>

#include "include/rapidjson/document.h"
#include "include/rapidjson/istreamwrapper.h"

UnitTest::UnitTest(const PCG_Converter* converterTemplate, KorgPCG* pcg, EPatchMode type, const std::string& unitTestFolder,
	const std::string& pcgBankLetter, int pcgProgId, const std::string& patchRefName)
	: m_converterTemplate(converterTemplate)
	, m_pcg(pcg)
	, m_type(type)
	, m_unitTestFolder(unitTestFolder)
	, m_pcgBankLetter(pcgBankLetter)
	, m_pcgProgId(pcgProgId)
	, m_patchRefName(patchRefName)
{
}

UnitTest::~UnitTest()
{
	if (m_thread)
	{
		delete m_thread;
		m_thread = nullptr;
	}
}

void UnitTest::startThread()
{
	if (m_bUseThread)
	{
		m_thread = new std::thread(
			&UnitTest::perform, this);
	}
}

void UnitTest::log(std::string&& txt)
{
	if (m_bUseThread)
		m_outputLog.push_back(std::move(txt));
	else
		std::cout << txt;
}

void UnitTest::wait()
{
	if (m_bUseThread)
	{
		m_thread->join();
	}
	else
	{
		perform();
	}

	for (auto& log : m_outputLog)
	{
		std::cout << log;
	}

	if (!m_retValue) std::cerr << "...Failed!\n";
	else std::cout << " ...Success!\n";
}

bool UnitTest::perform()
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

	{
		std::stringstream ss;
		ss << "Unit Test: " << m_pcgBankLetter << ":" << m_pcgProgId << " and " << m_patchRefName;
		log(ss.str());
	}

	bool bErrors = false;

	KorgBanks* container = (m_type == EPatchMode::Program) ? m_pcg->Program : m_pcg->Combination;
	std::string subfolder = (m_type == EPatchMode::Program) ? "Program" : "Combi";
	std::string jsonPrefix = (m_type == EPatchMode::Program) ? "prog_" : "combi_";

	std::string generatedFolder;
	{
		std::string tempFolder = m_unitTestFolder + "Temp\\";
		Helpers::createFolder(tempFolder);
		std::string tempSubFolder = tempFolder + subfolder + "\\";
		Helpers::createFolder(tempSubFolder);
		generatedFolder = tempSubFolder + m_pcgBankLetter + "\\";
		Helpers::createFolder(generatedFolder);
	}

	auto converter = PCG_Converter(*m_converterTemplate, generatedFolder);

	for (int i = 0; i < container->count; i++)
	{
		auto* bank = container->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(bank->bank);

		if (bankLetter != m_pcgBankLetter)
			continue;

		assert(m_pcgProgId >= 0 && m_pcgProgId < 128);

		auto* item = bank->item[m_pcgProgId];
		auto name = std::string((char*)item->data, 16);

		if (m_type == EPatchMode::Program)
		{
			converter.patchProgramToJson(0, m_pcgProgId, name, (unsigned char*)item->data, generatedFolder, "A");
		}
		else
		{
			converter.patchCombiToJson(0, m_pcgProgId, name, (unsigned char*)item->data, generatedFolder, "A");
		}
	}

	auto refPath = m_unitTestFolder + subfolder + "\\" + m_patchRefName;

	std::ifstream refIfs(refPath);
	if (!refIfs.is_open())
	{
		std::cerr << "Reference json file " << refPath << " not found!!\n";
		assert(refIfs.is_open());
	}

	rapidjson::IStreamWrapper refisw{ refIfs };
	rapidjson::Document refJson;
	refJson.ParseStream(refisw);

	std::ostringstream ss;
	ss << generatedFolder << std::setw(3) << std::setfill('0') << m_pcgProgId << ".patch";
	std::string generatedPath = ss.str();

	std::ifstream genIfs(generatedPath);
	if (!genIfs.is_open())
	{
		std::cerr << "Generated json file " << generatedPath << " not found!!\n";
		assert(genIfs.is_open());
	}

	rapidjson::IStreamWrapper isw{ genIfs };
	rapidjson::Document generatedJson;
	generatedJson.ParseStream(isw);

	auto refPresetName = std::string(refJson["general_program_information"]["name"].GetString());
	auto generatedPresetName = std::string(generatedJson["general_program_information"]["name"].GetString());

	refPresetName.erase(std::remove_if(refPresetName.begin(), refPresetName.end(), ::isspace), refPresetName.end());
	generatedPresetName.erase(std::remove_if(generatedPresetName.begin(), generatedPresetName.end(), ::isspace), generatedPresetName.end());
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
					ss << "\terror: Missing Param " << refParamName << "(" << refParamVal << ")" << "\n";
					log(ss.str());
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
					log("\t ---- Left: Ref Json ---- Right: Converted from PCG\n");
					bPrintErrorHeader = false;
				}

				std::stringstream ss;
				ss << "\terror: Param " << refParamName << ": " << refParamVal << " != " << generatedParamVal << "\n";
				log(ss.str());

				bErrors = true;
			}
		}

		refParamId++;
		generatedParamId++;
	}

	m_retValue = !bErrors;
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
		std::vector<UnitTest*> m_jobs;

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
			patchNameStrm << test.pcg_bank << std::setw(3) << std::setfill('0') << test.pcg_program << ".patch";

			auto* newTest = new UnitTest(&converterTemplate, pcg, type, unitTestFolder, test.pcg_bank, test.pcg_program, patchNameStrm.str());
			m_jobs.push_back(newTest);

			newTest->startThread();
		}

		for (auto it = m_jobs.begin(); it != m_jobs.end();)
		{
			auto* job = (*it);
			job->wait();
			delete job;
			it = m_jobs.erase(it);
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
