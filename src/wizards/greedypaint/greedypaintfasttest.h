﻿#pragma once

#include <QDialog>
#include <data/calculation/greedypainter.h>

class QSpinBox;
class QTimeEdit;
class QLineEdit;
class RailRulerCombo;
class QTextBrowser;
class QCheckBox;
class QTableView;
class GapConstraintModel;
class GreedyPaintFastTest : public QDialog
{
    Q_OBJECT
    Diagram& diagram;
    RailRulerCombo* cbRuler;
    QLineEdit* edTrainName;
    QTimeEdit* edTime;
    QSpinBox* spInt,*spBack;
    QCheckBox* ckDown,*ckSingle,*ckStarting,*ckTerminal,*ckForbid;
    QTextBrowser* txtOut;
    QTableView* table;

    GreedyPainter painter;
    GapConstraintModel* const _model;
public:
    explicit GreedyPaintFastTest(Diagram& diagram, QWidget *parent = nullptr);

private:
    void initUI();
signals:
    void trainAdded(std::shared_ptr<Train>);
    void showStatus(const QString&);

private slots:
    void onApply();
    void onSingleLineChanged(bool on);
};

