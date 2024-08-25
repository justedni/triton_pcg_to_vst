#include "Worker.h"

#include "pcg_converter.h"

Worker::Worker(EnumKorgModel in_model,
    KorgPCG* in_pcg,
    const std::string& in_path,
    std::vector<std::string> in_progLetters,
    std::vector<std::string> in_combiLetters)
    : m_model(in_model)
    , m_pcg(in_pcg)
    , m_targetPath(in_path)
    , m_progLetters(in_progLetters)
    , m_combiLetters(in_combiLetters)
{
}

Worker::~Worker()
{
}

void Worker::process()
{
    auto converter = PCG_Converter(
        m_model,
        m_pcg,
        m_targetPath,
        std::bind(&Worker::logCallback, this, std::placeholders::_1)
    );

    if (converter.isInitialized())
    {
        if (!m_progLetters.empty())
            converter.convertPrograms(m_progLetters);

        if (!m_combiLetters.empty())
            converter.convertCombis(m_combiLetters);

        emit finished(true);
    }
    else
    {
        emit finished(false);
    }
}

void Worker::logCallback(const std::string& str)
{
    emit log(str);
}
