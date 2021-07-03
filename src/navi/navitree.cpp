﻿#include "navitree.h"

#include <QtWidgets>
#include "model/diagram/diagramnavimodel.h"
#include "addpagedialog.h"


NaviTree::NaviTree(DiagramNaviModel* model_, QUndoStack* undo, QWidget *parent):
    QTreeView(parent),mePageList(new QMenu(this)),meRailList(new QMenu(this)),
    _model(model_),_undo(undo)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
        this, SLOT(showContextMenu(const QPoint&)));
    initContextMenus();
    setModel(_model);
    expandAll();
}

void NaviTree::initContextMenus()
{
    //PageList
    QAction* act = mePageList->addAction(tr("添加运行图视窗"));
    connect(act, SIGNAL(triggered()), this, SLOT(addNewPage()));

    //RailList
    act = meRailList->addAction(tr("导入线路"));
    connect(act, SIGNAL(triggered()), this, SLOT(importRailways()));
}

NaviTree::ACI* NaviTree::getItem(const QModelIndex& idx)
{
    if (!idx.isValid())
        return nullptr;
    return static_cast<ACI*>(idx.internalPointer());
}

void NaviTree::addNewPage()
{
    auto* dialog = new AddPageDialog(_model->diagram(), this);
    connect(dialog, SIGNAL(creationDone(std::shared_ptr<DiagramPage>)),
        this, SIGNAL(pageAdded(std::shared_ptr<DiagramPage>)));
    dialog->open();
}

void NaviTree::importRailways()
{
    QString res = QFileDialog::getOpenFileName(this, tr("导入线路"), QString(),
        QObject::tr("pyETRC运行图文件(*.pyetgr;*.json)\nETRC运行图文件(*.trc)\n所有文件(*.*)"));
    if (res.isEmpty())
        return;

    Diagram _diatmp;
    bool flag = _diatmp.fromJson(res);
    if (!flag || _diatmp.isNull()) {
        QMessageBox::warning(this, tr("错误"), tr("文件错误或为空，无法导入"));
        return;
    }

    QList<std::shared_ptr<Railway>> rails = std::move(_diatmp.railways());
    for (auto p : rails) {
        p->setName(_model->diagram().validRailwayName(p->name()));
    }

    _undo->push(new qecmd::ImportRailways(_model, rails));
}

void NaviTree::currentChanged(const QModelIndex& cur, const QModelIndex& prev)
{
    ACI* ipre = getItem(prev), *icur = getItem(cur);
    QTreeView::currentChanged(cur, prev);
    //先处理取消选择的
    if (ipre&&(!icur||ipre->type()!=icur->type())) {
        switch (ipre->type()) {
        case navi::PageItem::Type:emit focusOutPage(); break;
        case navi::TrainModelItem::Type:emit focusOutTrain(); break;
        }
    }

    //处理新选择的
    if (icur) {
        switch (icur->type()) {
        case navi::PageItem::Type:emit focusInPage(static_cast<navi::PageItem*>(icur)->page()); break;
        case navi::TrainModelItem::Type:
            emit focusInTrain(static_cast<navi::TrainModelItem*>(icur)->train()); break;
        }
    }
}


void NaviTree::showContextMenu(const QPoint& pos)
{
    QModelIndex idx = currentIndex();
    if (!idx.isValid())return;
    auto* item = static_cast<navi::AbstractComponentItem*>(idx.internalPointer());

    auto p = mapToGlobal(pos);
    
    switch (item->type()) {
    case navi::PageListItem::Type:mePageList->popup(p); break;
    case navi::RailwayListItem::Type:meRailList->popup(p); break;
    }
    
}

qecmd::ImportRailways::ImportRailways(DiagramNaviModel* navi_,
    const QList<std::shared_ptr<Railway>>& rails_, QUndoCommand* parent) :
    QUndoCommand(QObject::tr("导入") + QString::number(rails_.size()) + QObject::tr("条线路"), parent),
    navi(navi_), rails(rails_)
{
}

void qecmd::ImportRailways::undo()
{
    navi->removeTailRailways(rails.size());
}

void qecmd::ImportRailways::redo()
{
    //添加到线路表后面，然后inform change
    //现在不需要通知别人发生变化...
    navi->importRailways(rails);
}