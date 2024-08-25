#pragma once

#include <QObject>

enum class EnumKorgModel : uint8_t;
struct KorgPCG;

class Worker : public QObject
{
    Q_OBJECT

public:
    Worker(EnumKorgModel in_model, KorgPCG* in_pcg, const std::string& in_path,
        std::vector<std::string> in_progLetters,
        std::vector<std::string> in_combiLetters);
    ~Worker();

    void logCallback(const std::string& str);

public slots:
    void process();

signals:
    void finished(bool success);
    void log(const std::string& str);

private:
    EnumKorgModel m_model;
    KorgPCG* m_pcg = nullptr;
    std::string m_targetPath;

    std::vector<std::string> m_progLetters;
    std::vector<std::string> m_combiLetters;
};
