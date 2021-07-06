﻿#include "trainlistwidget.h"
#include "util/buttongroup.hpp"

#include "mainwindow/mainwindow.h"

#include <QtWidgets>
#include <QString>

TrainListWidget::TrainListWidget(TrainCollection& coll_, QWidget* parent):
	QWidget(parent), coll(coll_),model(new TrainListModel(coll_,this)),
	table(new QTableView),editSearch(new QLineEdit)
{
	initUI();

	connect(model, SIGNAL(trainsRemovedUndone(const QList<std::shared_ptr<Train>>&)),
		this, SIGNAL(trainsRemovedUndone(const QList<std::shared_ptr<Train>>&)));
	connect(model, SIGNAL(trainsRemovedRedone(const QList<std::shared_ptr<Train>>&)),
		this, SIGNAL(trainsRemovedRedone(const QList<std::shared_ptr<Train>>&)));
}

void TrainListWidget::refreshData()
{
	model->endResetModel();
	table->update();
	table->resizeColumnsToContents();
}

void TrainListWidget::initUI()
{
	auto* vlay = new QVBoxLayout;

	auto* hlay = new QHBoxLayout;
	hlay->addWidget(editSearch);   //搜索行为改为filt?
	connect(editSearch, SIGNAL(editingFinished()), this, SLOT(searchTrain()));
	auto* btn = new QPushButton(QObject::tr("搜索"));
	connect(btn, SIGNAL(clicked()), this, SLOT(searchTrain()));
	hlay->addWidget(btn);
	vlay->addLayout(hlay);

	table->verticalHeader()->setDefaultSectionSize(20);
	table->setModel(model);
	table->setSelectionBehavior(QTableView::SelectRows);
	table->horizontalHeader()->setSortIndicatorShown(true);
	connect(table->horizontalHeader(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)),
		table, SLOT(sortByColumn(int, Qt::SortOrder)));
	table->resizeColumnsToContents();
	vlay->addWidget(table);
	connect(table, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(editButtonClicked()));
	connect(model, SIGNAL(informTrainSorted()), this, SIGNAL(trainReordered()));
	connect(model, SIGNAL(trainSorted(const QList<std::shared_ptr<Train>>&, TrainListModel*)),
		this, SIGNAL(trainSorted(const QList<std::shared_ptr<Train>>&, TrainListModel*)));
	
	connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged,
		this, &TrainListWidget::onCurrentRowChanged);

	auto* h = new ButtonGroup<2>({ "上移","下移"});
	h->setMinimumWidth(50);
	vlay->addLayout(h);
	//todo: connect

	auto* g = new ButtonGroup<4>({ "编辑","批量调整","添加","删除" });
	g->setMinimumWidth(50);
	vlay->addLayout(g);
	g->connectAll(SIGNAL(clicked()), this, {
		SLOT(editButtonClicked()),SLOT(batchChange()),SIGNAL(addNewTrain()),SLOT(actRemoveTrains())
		});

	setLayout(vlay);
}

void TrainListWidget::searchTrain()
{
	//todo...
}


void TrainListWidget::editButtonClicked()
{
	auto index = table->currentIndex();
	if (index.isValid()) {
		emit editTrain(coll.trainAt(index.row()));
		qDebug() << "edit train: " << coll.trainAt(index.row())->trainName().full() << Qt::endl;
	}
}

void TrainListWidget::batchChange()
{

}


void TrainListWidget::actRemoveTrains()
{
	auto lst = table->selectionModel()->selectedRows(0);
	if (lst.empty())
		return;

	QList<int> rows;
	rows.reserve(lst.size());
	for (const auto& p : lst) {
		rows.append(p.row());
	}

	std::sort(rows.begin(), rows.end());
	QList<std::shared_ptr<Train>> trains;
	trains.reserve(rows.size());

	//注意实际的删除由Model执行。这里生成列表，发给Main来执行CMD压栈
	for (auto p = rows.rbegin(); p != rows.rend(); ++p) {
		std::shared_ptr<Train> train(coll.trainAt(*p));   //move 
		trains.prepend(train);
	}

	model->redoRemoveTrains(trains, rows);

	emit trainsRemoved(trains, rows, model);
}

void TrainListWidget::onCurrentRowChanged(const QModelIndex& idx)
{
	if (!idx.isValid())
		return;
	int row = idx.row();
	emit currentTrainChanged(coll.trainAt(row));
}

qecmd::RemoveTrains::RemoveTrains(const QList<std::shared_ptr<Train>>& trains,
	const QList<int>& indexes, TrainCollection& coll_, TrainListModel* model_,
	MainWindow* mw_, QUndoCommand* parent) :
	QUndoCommand(QObject::tr("删除") + QString::number(trains.size()) + QObject::tr("个车次"), parent),
	_trains(trains), _indexes(indexes), coll(coll_), model(model_), mw(mw_)
{
}

void qecmd::RemoveTrains::undo()
{
	model->undoRemoveTrains(_trains, _indexes);
}

void qecmd::RemoveTrains::redo()
{
	if (first) {
		first = false;
		return;
	}
	model->redoRemoveTrains(_trains, _indexes);
}

qecmd::SortTrains::SortTrains(const QList<std::shared_ptr<Train>>& ord_, 
	TrainListModel* model_, QUndoCommand* parent):
	QUndoCommand(QObject::tr("列车排序"),parent),ord(ord_),model(model_)
{
}

void qecmd::SortTrains::undo()
{
	model->undoRedoSort(ord);
}

void qecmd::SortTrains::redo()
{
	if (first) {
		first = false;
		return;
	}
	model->undoRedoSort(ord);
}

bool qecmd::SortTrains::mergeWith(const QUndoCommand* another)
{
	if (id() != another->id())
		return false;
	auto cmd = static_cast<const qecmd::SortTrains*>(another);
	if (model == cmd->model) {
		//只针对同一个model的排序做合并
		//成功合并：抛弃中间状态
		return true;
	}
	return false;
}
