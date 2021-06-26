﻿#include "diagramwidget.h"
#include "data/diagram/trainadapter.h"
#include "trainitem.h"
#include <QPainter>
#include <Qt>
#include <QGraphicsScene>
#include <QScrollbar>
#include <QList>
#include <QPair>
#include <QMouseEvent>
#include <cmath>
#include <QPrinter>
#include <QMessageBox>

DiagramWidget::DiagramWidget(Diagram &diagram, QWidget* parent):
    QGraphicsView(parent),_diagram(diagram),startTime(diagram.config().start_hour,0,0)
{
    //todo: menu...
    setRenderHint(QPainter::Antialiasing, true);
    setAlignment(Qt::AlignTop | Qt::AlignLeft);

    setScene(new QGraphicsScene);
    //todo: label span
    paintGraph();

    setMouseTracking(true);
}

DiagramWidget::~DiagramWidget() noexcept
{
    if (scene()) {
        auto* s = scene();
        setScene(nullptr);
        delete s;
    }
}

void DiagramWidget::autoPaintGraph()
{
    if (_diagram.config().auto_paint)
        paintGraph();
}

void DiagramWidget::paintGraph()
{
    updating = true;
    scene()->clear();
    _selectedTrain = nullptr;
    emit showNewStatus(QString("正在铺画运行图"));

    const Config& cfg = _diagram.config();
    const auto& margins = cfg.margins;
    int hstart = cfg.start_hour, hend = cfg.end_hour;
    if (hend <= hstart)
        hend += 24;
    int hour_count = hend - hstart;    //区间数量
    //width = hour_count * (3600 / UIDict["seconds_per_pix"])
    double width = hour_count * (3600.0 / cfg.seconds_per_pix);

    //暂定上下边距只算一次
    double height = (_diagram.railwayCount()-1) * cfg.margins.gap_between_railways;
    for (const auto& p : _diagram.railways()) {
        height += p->calStationYValue(cfg);
    }
    const QColor& gridColor = cfg.grid_color;
    
    scene()->setSceneRect(0, 0, width + cfg.margins.left + cfg.margins.right,
        height + cfg.margins.up + cfg.margins.down);

    double ystart = margins.up;

    QList<QPair<double, double>> railYRanges;   //每条线路的时间线纵坐标起止点
    QList<QGraphicsItem*> leftItems, rightItems;

    for (auto p : _diagram.railways()) {
        p->setStartYValue(ystart);
        setHLines(p, ystart, width, leftItems, rightItems);
        scene()->addRect(margins.left, ystart, width, p->diagramHeight(), gridColor);
        railYRanges.append(qMakePair(ystart, ystart + p->diagramHeight()));
        ystart += p->diagramHeight() + margins.gap_between_railways;
    }

    setVLines(width, hour_count, railYRanges);

    marginItems.left = scene()->createItemGroup(leftItems);
    marginItems.left->setZValue(15);
    marginItems.right = scene()->createItemGroup(rightItems);
    marginItems.right->setZValue(15);

    //todo: labelSpan
    
    //todo: 绘制提示进度条
    for (auto p : _diagram.trainCollection().trains()) {
        paintTrain(p);
    }

    showAllForbids();
    
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
        this, SLOT(updateTimeAxis()));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
        this, SLOT(updateDistanceAxis()));

    updateTimeAxis();
    updateDistanceAxis();
    emit showNewStatus(QObject::tr("运行图铺画完毕"));
    updating = false;
}

bool DiagramWidget::toPdf(const QString& filename, const QString& title)
{
    marginItems.left->setX(0);
    marginItems.right->setX(0);
    marginItems.top->setY(0);
    marginItems.bottom->setY(0);
    nowItem->setPos(0, 0);
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filename);
    constexpr double note_apdx = 80;

    QSize size(scene()->width(), scene()->height() + 200);
    QPageSize pageSize(size);
    printer.setPageSize(pageSize);

    QPainter painter;
    painter.begin(&printer);

    if (!painter.isActive()) {
        QMessageBox::warning(this, QObject::tr("错误"), QObject::tr("保存PDF失败，可能是文件占用。"));
        return false;
    }
    painter.scale(printer.width() / scene()->width(), printer.width() / scene()->width());
    painter.setPen(QPen(config().text_color));
    QFont font;
    font.setPixelSize(40);
    font.setBold(true);
    painter.setFont(font);

    painter.drawText(margins().left, 80, title);

    font.setPixelSize(20);
    font.setBold(false);
    painter.setFont(font);

    if (!_diagram.note().isEmpty()) {
        QString s(_diagram.note());
        s.replace("\n", " ");
        s = QString("备注：") + s;
        painter.drawText(margins().left, scene()->height() + 100 + 40, s);
    }

    //todo: 版本记号
    QString mark("由qETRC列车运行图系统（开发版）导出");
    painter.drawText(scene()->width() - 400, scene()->height() + 100 + 40, mark);
    scene()->render(&painter, QRectF(0, 100, scene()->width(), scene()->height()));
    painter.end();

    updateDistanceAxis();
    updateTimeAxis();
    return true;
}

void DiagramWidget::mousePressEvent(QMouseEvent* e)
{
    if (updating)
        return;
    if (e->button() == Qt::LeftButton) {
        auto pos = mapToScene(e->pos());
        unselectTrain();

        auto* item = scene()->itemAt(pos, transform());
        if (!item)
            return;
        while (item->parentItem())
            item = item->parentItem();
        if (item->type() == TrainItem::Type) {
            selectTrain(qgraphicsitem_cast<TrainItem*>(item));
        }
    }
    else if (e->button() == Qt::RightButton) {
        //todo: context menu
    }
    QGraphicsView::mousePressEvent(e);
}

void DiagramWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    qDebug() << "DiagramWidget: save PDF" << Qt::endl;
    toPdf(R"(D:\QTProject\qETRC\测试数据\sample.pdf)", "qETRC多线路测试样张");
}

void DiagramWidget::resizeEvent(QResizeEvent* e)
{
    QGraphicsView::resizeEvent(e);
    if (!updating) {
        updateDistanceAxis();
        updateTimeAxis();
    }
}

void DiagramWidget::setHLines(std::shared_ptr<Railway> rail, double start_y, double width,
    QList<QGraphicsItem*>& leftItems, QList<QGraphicsItem*>& rightItems)
{
    const Config& cfg = _diagram.config();
    const auto& margins = cfg.margins;
    const QColor& textColor = cfg.text_color;
    QColor brushColor(Qt::white);
    brushColor.setAlpha(200);
    double label_start_x = margins.mile_label_width + margins.ruler_label_width;

    double height = rail->diagramHeight();   //注意这只是当前线路的高度

    auto* rectLeft = scene()->addRect(0,
        start_y - margins.title_row_height - margins.first_row_append,
        margins.label_width + margins.ruler_label_width + margins.mile_label_width + margins.left_white,
        height + 2 * margins.first_row_append + margins.title_row_height
    );
    rectLeft->setBrush(QBrush(brushColor));
    rectLeft->setPen(QPen(Qt::transparent));
    rectLeft->setZValue(-1);
    leftItems.append(rectLeft);

    auto* rectRight = scene()->addRect(
        scene()->width() - margins.label_width - margins.right_white,
        start_y - margins.title_row_height - margins.first_row_append,
        margins.label_width,
        height + 2 * margins.first_row_append + margins.title_row_height
    );
    rectRight->setBrush(brushColor);
    rectRight->setPen(QPen(Qt::transparent));
    rightItems.append(rectRight);

    const QColor& gridColor = cfg.grid_color;
    QPen defaultPen(gridColor, cfg.default_grid_width),
        boldPen(gridColor, cfg.bold_grid_width);

    double rect_start_y = start_y - margins.title_row_height - margins.first_row_append;
    auto* rulerRect = scene()->addRect(
        margins.left_white, rect_start_y,
        margins.mile_label_width + margins.ruler_label_width,
        height + margins.title_row_height + margins.first_row_append*2,
        defaultPen
    );
    rulerRect->setPen(defaultPen);
    leftItems.append(rulerRect);

    //标尺栏纵向界限
    auto* line = scene()->addLine(
        margins.ruler_label_width + margins.left_white,
        rect_start_y,
        margins.ruler_label_width + margins.left_white,
        height + start_y + margins.first_row_append,
        defaultPen
    );
    leftItems.append(line);

    //标尺栏横向界限
    line = scene()->addLine(
        margins.left_white,
        rect_start_y + margins.title_row_height / 2,
        margins.ruler_label_width + margins.left_white,
        rect_start_y + margins.title_row_height / 2,
        defaultPen
    );
    leftItems.append(line);

    //表头下横分界线
    line = scene()->addLine(
        margins.left_white,
        rect_start_y + margins.title_row_height,
        margins.ruler_label_width + margins.mile_label_width + margins.left_white,
        rect_start_y + margins.title_row_height,
        defaultPen
    );
    leftItems.append(line);

    QFont nowFont;
    nowFont.setPointSize(12);
    nowItem = scene()->addSimpleText(" ", nowFont);
    nowItem->setBrush(textColor);
    nowItem->setZValue(16);

    QFont textFont;
    //textFont.setBold(true);

    const char* s0 = rail->ordinate() ? "排图标尺" : "区间距离";
    leftItems.append(addLeftTableText(
        s0, textFont, 0, rect_start_y, margins.ruler_label_width, margins.title_row_height / 2
    ));

    leftItems.append(addLeftTableText(
        "下行", textFont, 0, rect_start_y + margins.title_row_height / 2.0,
        margins.ruler_label_width / 2.0, margins.title_row_height / 2.0
    ));

    leftItems.append(addLeftTableText(
        "上行", textFont, margins.ruler_label_width / 2.0,
        rect_start_y + margins.title_row_height / 2.0, margins.ruler_label_width / 2.0,
        margins.title_row_height / 2.0
    ));

    leftItems.append(addLeftTableText(
        "延长公里", textFont, margins.ruler_label_width, rect_start_y,
        margins.mile_label_width, margins.title_row_height
    ));

    //改为无条件上下行分开标注
    leftItems.append(scene()->addLine(
        margins.ruler_label_width / 2.0 + margins.left_white,
        rect_start_y + margins.title_row_height / 2.0,
        margins.ruler_label_width / 2.0 + margins.left_white,
        start_y + height + margins.first_row_append,
        defaultPen
    ));
    

    //铺画车站。以前已经完成了绑定，这里只需要简单地把所有有y坐标的全画出来就好
    for (auto p : rail->stations()) {
        if (p->y_value.has_value() && p->_show) {
            const auto& pen = p->level <= cfg.bold_line_level ?
                boldPen : defaultPen;
            double h = start_y + p->y_value.value();
            drawSingleHLine(textFont, h, p->name.toDisplayLiteral(),
                pen, width, leftItems, rightItems, label_start_x);
            leftItems.append(addStationTableText(
                QString::number(p->mile, 'f', 1), textFont,
                margins.ruler_label_width, h, margins.mile_label_width
            ));
        }
    }

    auto ruler = rail->ordinate();
    auto upFirst = rail->firstUpInterval();

    //cum: cummulative 和前面的累计
    //必须这样做的原因是可能有车站没有显示，这时也不划线
    double lasty = start_y, cummile = 0.0;
    int cuminterval = 0;
    bool cumvalid = true;

    //标注区间数据  每个区间标注带上区间【终点】的界限
    for (auto p = rail->firstDownInterval(); p; p = rail->nextIntervalCirc(p)) {
        //标注区间终点分划线
        double x = margins.left_white;
        if (!p->isDown()) x += margins.ruler_label_width / 2.0;
        double y = p->toStation()->y_value.value() + start_y;
        if (p->toStation()->_show)
            leftItems.append(scene()->addLine(
                x, y, x + margins.ruler_label_width / 2.0, y, defaultPen
            ));
        //标注数据
        if (p == upFirst) {
            //方向转换，清理旧数据
            lasty = start_y + height;
            cummile = 0.0;
            cuminterval = 0;
        }
        if (ruler) {
            
            if (p->getRulerNode(ruler)->isNull()) {
                if (p->direction() == Direction::Up && !ruler->different())
                    cuminterval += p->inverseInterval()->getRulerNode(ruler)->interval;
                else
                    cumvalid = false;
            }
            else {
                cuminterval += p->getRulerNode(ruler)->interval;
            }
                
        }
        else {
            cummile += p->mile();
        }
        if (p->toStation()->_show) {
            const QString& text = ruler ?
                cumvalid ? QString::asprintf("%d:%02d", cuminterval / 60, cuminterval % 60) : "NA" :
                QString::number(cummile, 'f', 1);
            leftItems.append(alignedTextItem(
                text, textFont,
                margins.ruler_label_width / 2.0, x,
                (lasty + y) / 2, false
            ));
            lasty = y;
            cummile = 0.0;
            cuminterval = 0;
            cumvalid = true;
        }
        
    }
    //补充起点的下行和终点的上行边界
    leftItems.append(scene()->addLine(
        margins.left_white, start_y, margins.left_white + margins.ruler_label_width / 2.0, start_y,
        defaultPen
    ));
    leftItems.append(scene()->addLine(
        margins.left_white + margins.ruler_label_width / 2.0,
        start_y + height, margins.left_white + margins.ruler_label_width, start_y + height,defaultPen
    ));
}

void DiagramWidget::setVLines(double width, int hour_count, 
    const QList<QPair<double, double>> railYRanges)
{
    int gap = config().minutes_per_vertical_line;
    double gap_px = minitesToPixels(gap);
    int minute_thres = config().minute_mark_gap_pix;
    int minute_marks_gap = std::max(int(minute_thres / gap_px), 1);  //每隔多少竖线标注一次分钟
    int vlines = 60 / gap;   //每小时纵线数量+1
    int mark_count = vlines / minute_marks_gap;    //int div
    int centerj = vlines / 2;   //中心那条线的j下标。与它除minute_marks_gap同余的是要标注的

    int line_count = gap * hour_count;    //小区间总数
    QPen
        pen_hour(config().grid_color, config().bold_grid_width),
        pen_half(config().grid_color, config().default_grid_width, Qt::DashLine),
        pen_other(config().grid_color, config().default_grid_width);

    QList<QGraphicsItem*> topItems, bottomItems;

    QColor color(Qt::white);
    color.setAlpha(200);

    topItems.append(scene()->addRect(
        margins().left - 15, 0,
        width + margins().left + 30, 35, QPen(Qt::transparent), color
    ));
    topItems.append(scene()->addLine(
        margins().left - 15, 35, width + margins().left + 15, 35, QPen(config().grid_color, 2)
    ));

    bottomItems.append(scene()->addRect(
        margins().left - 15, scene()->height() - 35,
        width + margins().left + 30, 35, QPen(Qt::transparent), color
    ));
    bottomItems.append(scene()->addLine(
        margins().left - 15, scene()->height() - 35,
        width + margins().left + 15, scene()->height() - 35, QPen(config().grid_color, 2)
    ));

    QFont font;
    font.setPixelSize(25);
    font.setBold(true);

    QFont fontmin;
    fontmin.setPixelSize(15);
    fontmin.setBold(true);

    for (int i = 0; i < hour_count + 1; i++) {
        double x = margins().left + i * 3600 / config().seconds_per_pix;
        int hour = (i + config().start_hour) % 24;
        auto* textItem1 = addTimeAxisMark(hour, font, x);
        textItem1->setY(30 - textItem1->boundingRect().height());
        topItems.append(textItem1);

        auto* textItem2 = addTimeAxisMark(hour, font, x);
        textItem2->setY(scene()->height() - 30);
        bottomItems.append(textItem2);

        if (i == hour_count)
            break;

        //小时线
        if (i) {
            for (const auto& t : railYRanges) {
                scene()->addLine(x, t.first, x, t.second, pen_hour);
            }
        }
        //分钟线
        for (int j = 1; j < vlines; j++) {
            x += gap * 60 / config().seconds_per_pix;
            double minu = j * gap;
            for (const auto& t : railYRanges) {
                auto* line = scene()->addLine(x, t.first, x, t.second);
                if (minu == 30)
                    line->setPen(pen_half);
                else
                    line->setPen(pen_other);
            }
            if (j % minute_marks_gap == centerj % minute_marks_gap) {
                //标记分钟数
                textItem1 = addTimeAxisMark(int(std::round(minu)), fontmin, x);
                textItem1->setY(30 - textItem1->boundingRect().height());
                topItems.append(textItem1);
                textItem2 = addTimeAxisMark(int(std::round(minu)), fontmin, x);
                textItem2->setY(scene()->height() - 30);
                bottomItems.append(textItem2);
            }
        }
    }
    marginItems.top = scene()->createItemGroup(topItems);
    marginItems.top->setZValue(15);
    marginItems.bottom = scene()->createItemGroup(bottomItems);
    marginItems.bottom->setZValue(15);
}


double DiagramWidget::minitesToPixels(int minutes) const
{
    return minutes * 60.0 / config().seconds_per_pix;
}


void DiagramWidget::paintTrain(std::shared_ptr<Train> train)
{
    train->clearItems();
    if (!train->isShow())
        return;
    for (auto adp : train->adapters()) {
        for (auto line : adp->lines()) {
            if (line->isNull()) {
                //这个是不应该的
                qDebug() << "DiagramWidget::paintTrain: WARNING: " <<
                    "Unexpected null TrainLine! " << train->trainName().full() << Qt::endl;
            }
            else {
                auto* item = new TrainItem(*line, adp->railway(), _diagram);
                line->setItem(item);
                item->setZValue(5);
                scene()->addItem(item);
            }
        }
    }
}

QGraphicsSimpleTextItem* DiagramWidget::addLeftTableText(const char* str, 
    const QFont& textFont, double start_x, double start_y, double width, double height)
{
    return addLeftTableText(QObject::tr(str), textFont, start_x, start_y, width, height);
}

QGraphicsSimpleTextItem* DiagramWidget::addLeftTableText(const QString& str, 
    const QFont& textFont, double start_x, double start_y, double width, double height)
{
    const QColor& textColor = _diagram.config().text_color;
    const auto& margins = _diagram.config().margins;
    start_x += margins.left_white;

    auto* text = scene()->addSimpleText(str);
    QFont font(textFont);   //copy
    double width1 = text->boundingRect().width();
    if (width1 > width) {
        int stretch = 100 * width / width1;   //int div
        font.setStretch(stretch);
    }
    text->setFont(font);
    text->setBrush(textColor);
    const auto& r = text->boundingRect();
    text->setX(start_x + (width - r.width()) / 2);
    text->setY(start_y + (height - r.height()) / 2);
    return text;
}

QGraphicsSimpleTextItem* DiagramWidget::addStationTableText(const QString& str,
    const QFont& textFont, double start_x, double center_y, double width)
{
    const QColor& textColor = _diagram.config().text_color;
    const auto& margins = _diagram.config().margins;
    start_x += margins.left_white;

    auto* text = scene()->addSimpleText(str);
    QFont font(textFont);   //copy
    double width1 = text->boundingRect().width();
    if (width1 > width) {
        int stretch = 100 * width / width1;   //int div
        font.setStretch(stretch);
    }
    text->setFont(font);
    text->setBrush(textColor);
    const auto& r = text->boundingRect();
    text->setX(start_x + (width - r.width()) / 2);
    text->setY(center_y-r.height()/2);
    return text;
}

void DiagramWidget::drawSingleHLine(const QFont& textFont, double y, 
    const QString& name, const QPen& pen, double width, 
    QList<QGraphicsItem*>& leftItems, 
    QList<QGraphicsItem*>& rightItems, double label_start_x)
{
    scene()->addLine(margins().left, y, width + margins().left, y, pen);

    leftItems.append(alignedTextItem(name, textFont, margins().label_width - 5,
        label_start_x + margins().left_white + 5, y));
    
    rightItems.append(alignedTextItem(name, textFont, margins().label_width,
        scene()->width() - margins().label_width - margins().right_white, y));
}

QGraphicsSimpleTextItem* DiagramWidget::alignedTextItem(const QString& text, 
    const QFont& baseFont, double label_width, double start_x, double center_y, bool use_stretch)
{
    auto* textItem = scene()->addSimpleText(text, baseFont);
    textItem->setBrush(config().text_color);
    textItem->setY(center_y - textItem->boundingRect().height() / 2);
    double textWidth = textItem->boundingRect().width();
    QFont font(baseFont);
    if (textWidth > label_width) {
        int stretch = 100 * label_width / textWidth;
        font.setStretch(stretch);
        textItem->setFont(font);
    }
    textWidth = textItem->boundingRect().width();

    int cnt = text.size();
    if (use_stretch ) {
        if (textWidth < label_width && cnt > 1) {
            //两端对齐
            font.setLetterSpacing(QFont::AbsoluteSpacing, (label_width - textWidth) / (cnt - 1));
            textItem->setFont(font);
        }
        textItem->setX(start_x);
    }
    else {
        textItem->setX(start_x + (label_width - textWidth) / 2);
    }
    
    return textItem;
}

QGraphicsSimpleTextItem* DiagramWidget::addTimeAxisMark(int value, const QFont& font, int x)
{
    auto* item = scene()->addSimpleText(QString::number(value), font);
    item->setX(x - item->boundingRect().width() / 2);
    item->setBrush(config().grid_color);
    return item;
}

void DiagramWidget::selectTrain(TrainItem* item)
{
    if (updating)
        return;
    if (!item)
        return;
    _selectedTrain = &(item->train());
    _selectedTrain->highlightItems();

    nowItem->setText(_selectedTrain->trainName().full());

    //todo: ensure visible ?
    //todo: emit?
}

void DiagramWidget::unselectTrain()
{
    if (updating)
        return;
    if (_selectedTrain) {
        _selectedTrain->unhighlightItems();
        _selectedTrain = nullptr;
        nowItem->setText(" ");
    }
}

void DiagramWidget::showAllForbids()
{
    for (auto p : _diagram.railways()) {
        for (auto f : p->forbids()) {
            showForbid(f, Direction::Down);
            showForbid(f, Direction::Up);
        }
    }
}

void DiagramWidget::showForbid(std::shared_ptr<Forbid> forbid, Direction dir)
{
    removeForbid(forbid, dir);
    QPen pen(Qt::transparent);
    bool isService = (forbid->index() == 0);
    QColor color;
    if (isService) {
        color = QColor(85, 85, 85);
    }
    else {
        color = QColor(85, 85, 255);
    }
    color.setAlpha(200);
    QBrush brush(color);
    if (!forbid->different()) {
        if (dir != Direction::Down)
            return;
        brush.setStyle(Qt::DiagCrossPattern);
    }
    else if (dir == Direction::Down) {
        brush.setStyle(Qt::FDiagPattern);
    }
    else {
        brush.setStyle(Qt::BDiagPattern);
    }
    QBrush brush2(Qt::transparent);
    if (!isService) {
        brush2 = QColor(170, 170, 170, 80);
    }

    for (auto node = forbid->firstDirNode(dir); node; node = node->nextNode()) {
        addForbidNode(forbid, node, brush, pen);
        if (!isService) {
            addForbidNode(forbid, node, brush2, pen);
        }
    }
}

void DiagramWidget::removeForbid(std::shared_ptr<Forbid> forbid, Direction dir)
{
    auto& items = forbid->dirItems(dir);
    for (auto p : items) {
        scene()->removeItem(p);
    }
    items.clear();
}

void DiagramWidget::addForbidNode(std::shared_ptr<Forbid> forbid, 
    std::shared_ptr<ForbidNode> node, const QBrush& brush, const QPen& pen)
{
    auto& railint = node->railInterval();
    auto& railway = forbid->railway();
    double start_y = railway.startYValue();
    double y1 = railint.fromStation()->y_value.value(),
        y2 = railint.toStation()->y_value.value();
    if (y1 > y2)
        std::swap(y1, y2);
    //保证y1<=y2  方便搞方框
    double xstart = calXFromStart(node->beginTime),
        xend = calXFromStart(node->endTime);
    double width = config().diagramWidth();    //图形总宽度
    if (xstart == xend)   //莫得数据，再见
        return;
    else if (xstart < xend) {
        //没有跨界的简单情况
        if (xstart <= width) {
            //存在界内部分
            xend = std::min(xend, width);
            auto* item = scene()->addRect(xstart + margins().left, y1 + start_y, xend - xstart, y2 - y1,
                pen, brush);
            forbid->addItem(railint.direction(), item);
        }
    }
    else {
        //存在跨界的情况  先处理右半部分
        if (xstart <= width) {
            forbid->addItem(railint.direction(), scene()->addRect(
                xstart + margins().left, y1 + start_y, width - xstart, y2 - y1, pen, brush
            ));
        }
        //左半部分  一定有
        forbid->addItem(railint.direction(), scene()->addRect(
            margins().left, y1 + start_y, xend, y2 - y1, pen, brush
        ));
    }
}

double DiagramWidget::calXFromStart(const QTime& time) const
{
    int sec = startTime.secsTo(time);
    if (sec < 0)
        sec += 24 * 3600;
    return sec / config().seconds_per_pix;
}


void DiagramWidget::updateTimeAxis()
{
    QPoint p(0, 0);
    auto ps = mapToScene(p);
    marginItems.top->setY(ps.y());
    nowItem->setY(ps.y());
    QPoint p1(0, height());
    auto ps1 = mapToScene(p1);
    marginItems.bottom->setY(ps1.y() - scene()->height() - 27);
}

void DiagramWidget::updateDistanceAxis()
{
    QPoint point(0, 0);
    auto scenepoint = mapToScene(point);
    marginItems.left->setX(scenepoint.x());
    nowItem->setX(scenepoint.x());
    QPoint p2(width(), 0);
    auto sp2 = mapToScene(p2);
    marginItems.right->setX(sp2.x() - scene()->width() - 20);
}
