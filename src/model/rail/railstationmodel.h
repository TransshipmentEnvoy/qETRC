﻿#pragma once

#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QUndoStack>
#include <memory>

#include "data/rail/rail.h"
#include "model/delegate/qedelegate.h"

/**
 * @brief The RailStationModel class
 * 线路的站表（里程表）的数据模型，用于编辑里程的表格。
 * 注意使用StandartItem来暂存数据。
 */
class RailStationModel : public QStandardItemModel
{
    Q_OBJECT
    std::shared_ptr<Railway> railway;
    QUndoStack* const _undo;
public:
    enum Columns{
        ColName=0,
        ColMile,
        ColCounter,
        ColLevel,
        ColShow,
        ColDir,
        ColPassenger,
        ColFreight,
        ColMAX
    };
    explicit RailStationModel(QUndoStack* undo,
                              QObject *parent = nullptr);

    void setRailway(std::shared_ptr<Railway> rail);

    virtual bool insertRows(int row, int count, const QModelIndex &parent) override;

    /*
     * 默认实现的DisplayRole和EditRole是同一个东西。要想显示的不一样，只有重写
     * See: https://doc.qt.io/qt-5/qstandarditem.html#data
     */
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /**
     * 三个Check的列不能编辑只能Check
     */
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    /**
     * QStandardItemModel似乎没有实现这玩意，因此只有自己写。
     * 这个API有点小题大做。
     * preconditions: 
     * （1）sourceParent==destinationParent （否则返回false）
     * （2）src和dest的范围不重合 （否则UB）
     */
    virtual bool moveRows(const QModelIndex& sourceParent,
        int sourceRow, int count,
        const QModelIndex& destinationParent, int destinationChild)override;

public slots:

    /**
     * 提交数据，即点击了确定
     * 注意不能用submit...这个会被自动Call
     */
    bool actApply();

    /**
     * 撤销数据更改，即点击还原
     */
    void actCancel();

private:
    void setupModel();
};



