#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtPCGToVSTUI.h"

#include <vector>

class QCheckBox;
enum class EnumKorgModel : uint8_t;
struct KorgPCG;
class PCG_Converter;

class QtPCGToVSTUI : public QMainWindow
{
    Q_OBJECT

public:
    QtPCGToVSTUI(QWidget* parent = nullptr);
    ~QtPCGToVSTUI();

private slots:
    void on_browsePCG_clicked();
    void on_browseTargetFolder_clicked();
    void on_generateButton_clicked();

    void on_checkBoxStateChanged(Qt::CheckState state, QCheckBox* checkbox);

    void logCallback(const std::string& text);
    void workerFinished(bool success);

private:
    void analysePCG();

    void disableEverything(bool disable);
    void updateVisibility();
    void updateCheckboxes();

    EnumKorgModel model;
    KorgPCG* m_pcg = nullptr;

    Ui::QtPCGToVSTUIClass ui;

    std::vector<QCheckBox*> programCheckboxes;
    std::vector<QCheckBox*> combiCheckboxes;

    std::vector<std::string> selectedProgramLetters;
    std::vector<std::string> selectedCombiLetters;
};
