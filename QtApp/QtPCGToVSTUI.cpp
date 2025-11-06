#include "QtPCGToVSTUI.h"

#include <QLabel>
#include <QSizePolicy>
#include <QFileDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QFuture>
#include <QDir>
#include <QDirIterator>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>

#include "Worker.h"

#include "alchemist.h"
#include "pcg_converter.h"
#include "helpers.h"

void BankSelection::cleanup(QHBoxLayout* layout)
{
    auto remove = [&](auto*& widget)
    {
        if (widget)
        {
            layout->removeWidget(widget);
            delete widget;
            widget = nullptr;
        }
    };

    remove(targetCombo);
    remove(label);
}

QtPCGToVSTUI::QtPCGToVSTUI(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    auto font = QFont(ui.textEditLog->font());
    font.setPointSize(8);
    ui.textEditLog->setFont(font);

    analysePCG();
    updateVisibility();

    programBankSelection.reserve(4);
    combiBankSelection.reserve(4);
}

QtPCGToVSTUI::~QtPCGToVSTUI()
{
    for (auto& s : programBankSelection)
    {
        s.cleanup(ui.hLayout_targetPrograms);
    }

    for (auto& s : combiBankSelection)
    {
        s.cleanup(ui.hLayout_targetCombis);
    }
}

void QtPCGToVSTUI::on_checkBoxStateChanged(Qt::CheckState state, QCheckBox* checkbox)
{
    const auto letter = checkbox->text().toStdString();
    auto it = std::find(programCheckboxes.begin(), programCheckboxes.end(), checkbox);

    auto handleLetter = [&](auto& selectionCont, auto& checkBoxVec, auto* uiLayout)
    {
        if (state == Qt::CheckState::Checked)
        {
            auto* label = new QLabel(checkbox->text() + ":");

            QComboBox* combo = new QComboBox(this);
            char targetLetter = 'A';
            for (int i = 0; i < 4; i++)
            {
                combo->addItem(QString::asprintf("USER-%c", targetLetter));
                targetLetter++;
            }

            combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            combo->setMaximumSize(65, 22);

            int targetBankIdx;
            std::vector<int> allIds = { 0, 1, 2, 3 };
            for (auto& s : selectionCont)
            {
                auto it = std::find(allIds.begin(), allIds.end(), s.targetBankId);
                if (it != allIds.end())
                {
                    allIds.erase(it);
                }
            }
            targetBankIdx = *allIds.begin();
            combo->setCurrentIndex(targetBankIdx);

            connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](auto index) { on_targetComboboxChanged(index, combo); });

            auto widgetCount = uiLayout->count();
            uiLayout->insertWidget(widgetCount - 1, combo);
            uiLayout->insertWidget(widgetCount - 1, label);
            selectionCont.emplace_back(letter, combo, label, targetBankIdx);
        }
        else
        {
            auto letterIt = std::find_if(selectionCont.begin(), selectionCont.end(), [letter](auto& s) { return s.srcBank == letter; });
            Q_ASSERT(letterIt != selectionCont.end());
            letterIt->cleanup(uiLayout);
            selectionCont.erase(letterIt);
        }
    };

    if (it != programCheckboxes.end())
    {
        handleLetter(programBankSelection, programCheckboxes, ui.hLayout_targetPrograms);
    }
    else
    {
        it = std::find(combiCheckboxes.begin(), combiCheckboxes.end(), checkbox);
        if (it != combiCheckboxes.end())
        {
            handleLetter(combiBankSelection, combiCheckboxes, ui.hLayout_targetCombis);
        }
    }

    updateCheckboxes();
}

void QtPCGToVSTUI::on_targetComboboxChanged(int index, QComboBox* combo)
{
    auto swapIndexes = [index, combo](auto& cont, auto& previousIndex)
    {
        for (auto& s : cont)
        {
            if (combo != s.targetCombo && s.targetCombo->currentIndex() == index)
            {
                s.targetCombo->blockSignals(true);
                s.targetCombo->setCurrentIndex(previousIndex);
                s.targetBankId = previousIndex;
                s.targetCombo->blockSignals(false);
                break;
            }
        }
    };

    auto it = std::find_if(programBankSelection.begin(), programBankSelection.end(), [combo](auto& s) { return s.targetCombo == combo; });
    if (it != programBankSelection.end())
    {
        auto previousIndex = it->targetBankId;
        swapIndexes(programBankSelection, previousIndex);
        it->targetBankId = index;
    }
    else
    {
        auto it = std::find_if(combiBankSelection.begin(), combiBankSelection.end(), [combo](auto& s) { return s.targetCombo == combo; });
        if (it != combiBankSelection.end())
        {
            auto previousIndex = it->targetBankId;
            swapIndexes(combiBankSelection, previousIndex);
            it->targetBankId = index;
        }
    }
}

void QtPCGToVSTUI::updateCheckboxes()
{
    auto verify = [](auto& container)
    {
        int selectedCount = 0;
        for (auto* checkbox : container)
        {
            if (checkbox->isChecked())
                selectedCount++;
        }

        if (selectedCount >= 4)
        {
            for (auto* checkbox : container)
            {
                if (!checkbox->isChecked())
                    checkbox->setEnabled(false);
            }
        }
        else
        {
            for (auto* checkbox : container)
            {
                checkbox->setEnabled(true);
            }
        }
    };

    verify(programCheckboxes);
    verify(combiCheckboxes);

    updateVisibility();
}

void QtPCGToVSTUI::on_browsePCG_clicked()
{
    QString pcgPath = QFileDialog::getOpenFileName(this, "Open PCG", "", "Triton/Triton Extreme PCG file (*.pcg)");
    if (pcgPath.isEmpty())
        return;

    ui.linePCGPath->setText(pcgPath);
    analysePCG();
    updateVisibility();
}


void QtPCGToVSTUI::analysePCG()
{
    auto pcgPath = ui.linePCGPath->text();
    EnumKorgModel model;
    m_pcg = LoadTritonPCG(pcgPath.toStdString().c_str(), model);
    m_model = model;

    bool bIsTritonExtreme = (m_model == EnumKorgModel::KORG_TRITON_EXTREME);
    ui.radioTriton->setChecked(!bIsTritonExtreme);
    ui.radioTritonExtreme->setChecked(bIsTritonExtreme);

    programBankSelection.clear();
    combiBankSelection.clear();

    auto addCheckboxes = [&](auto* pcgContainer, auto& guiCheckboxes, auto* layout)
    {
        for (auto* checkbox : guiCheckboxes)
        {
            layout->removeWidget(checkbox);
            delete checkbox;
        }

        if (pcgContainer)
        {
            for (uint32_t i = 0; i < pcgContainer->count; i++)
            {
                auto* bank = pcgContainer->bank[i];
                auto bankLetter = Helpers::bankIdToLetter(bank->bank);

                QCheckBox* checkbox = new QCheckBox(bankLetter.c_str(), this);
                connect(checkbox, &QCheckBox::checkStateChanged, this, [this, checkbox](auto state) { on_checkBoxStateChanged(state, checkbox); });
                guiCheckboxes.push_back(checkbox);
                layout->addWidget(checkbox);
            }
        }

        if (guiCheckboxes.size() <= 4)
        {
            for (auto* checkbox : guiCheckboxes)
            {
                checkbox->setChecked(true);
            }
        }
    };

    if (m_pcg)
    {
        addCheckboxes(m_pcg->Program, programCheckboxes, ui.hLayout_progLetters);
        addCheckboxes(m_pcg->Combination, combiCheckboxes, ui.hLayout_combiLetters);
    }
}

void QtPCGToVSTUI::on_browseTargetFolder_clicked()
{
    QString targetFolder = QFileDialog::getExistingDirectory(this, "Choose target directory");
    if (targetFolder.isEmpty())
        return;

    ui.lineTargetFolder->setText(targetFolder);
    updateVisibility();
}

void QtPCGToVSTUI::on_generateButton_clicked()
{
    auto outPath = ui.lineTargetFolder->text();

    if (outPath.isEmpty())
        return;

    int fileCount = 0;
    QDirIterator it(outPath, QStringList() << "*.patch", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        fileCount++;
        it.next();
    }

    if (fileCount > 0)
    {
        QMessageBox::StandardButton reply;
        auto msg = QString::asprintf("Target folder %s already contains %d .patch files.\nThe process will overwrite them. Continue?",
            outPath.toStdString().c_str(), fileCount);
        reply = QMessageBox::question(this, "Overwrite files in folder?", msg,
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
            return;
    }

    auto targetModel = ui.radioTritonExtreme->isChecked() ? EnumKorgModel::KORG_TRITON_EXTREME : EnumKorgModel::KORG_TRITON;

    QThread* thread = new QThread();
    Worker* worker = new Worker(targetModel, m_pcg, outPath.toStdString(), programBankSelection, combiBankSelection);
    worker->moveToThread(thread);
    connect(worker, SIGNAL(log(const std::string&)), this, SLOT(logCallback(const std::string&)));
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, &Worker::finished, thread, [thread](bool) { thread->quit(); });
    connect(worker, &Worker::finished, thread, [worker](bool state){ worker->deleteLater(); });
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(finished(bool)), this, SLOT(workerFinished(bool)));
    thread->start();

    disableEverything(true);
}

void QtPCGToVSTUI::logCallback(const std::string& text)
{
    ui.textEditLog->moveCursor(QTextCursor::End);
    ui.textEditLog->insertPlainText(QString(text.c_str()));
    ui.textEditLog->moveCursor(QTextCursor::End);
}

void QtPCGToVSTUI::workerFinished(bool success)
{
    if (success)
        logCallback("~~ finished! ~~");
    disableEverything(false);
}

void QtPCGToVSTUI::disableEverything(bool disable)
{
    ui.generateButton->setDisabled(disable);
    ui.browsePCG->setDisabled(disable);
    ui.browseTargetFolder->setDisabled(disable);

    if (disable)
    {
        for (auto* checkbox : programCheckboxes)
        {
            checkbox->setDisabled(true);
        }

        for (auto* checkbox : combiCheckboxes)
        {
            checkbox->setDisabled(true);
        }
    }
    else
    {
        updateCheckboxes();
        updateVisibility();
    }
}

void QtPCGToVSTUI::updateVisibility()
{
    bool bEnableGenerate = (!ui.linePCGPath->text().isEmpty())
        && (!ui.lineTargetFolder->text().isEmpty() && m_pcg && (!programBankSelection.empty() || !combiBankSelection.empty()));
    ui.generateButton->setEnabled(bEnableGenerate);
}