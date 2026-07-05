#include "alarm_log_widget.h"
#include <QColor>

static QColor levelColor(int level) {
    switch (level) {
        case 1: return Qt::red;
        case 2: return QColor(255, 140, 0);
        case 3: return QColor(200, 200, 0);
        default: return Qt::blue;
    }
}

AlarmLogWidget::AlarmLogWidget(BackendBridge* bridge, QWidget* parent)
    : QWidget(parent), bridge_(bridge) {
    ui.setupUi(this);

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, SIGNAL(timeout()), this, SLOT(refresh()));
    refreshTimer_->start(1000);
    refresh();
}

void AlarmLogWidget::refresh() {
    ui.logTable->setRowCount(0);

    QVector<BackendBridge::EventEntry> events = bridge_->getActiveEvents();
    for (int i = 0; i < events.size(); ++i) {
        const BackendBridge::EventEntry& e = events[i];
        int row = ui.logTable->rowCount();
        ui.logTable->insertRow(row);

        // 第 1 列：时间
        QTableWidgetItem* timeItem = new QTableWidgetItem(e.timestamp);
        ui.logTable->setItem(row, 0, timeItem);

        // 第 2 列：报警内容描述（带等级颜色）
        QTableWidgetItem* descItem = new QTableWidgetItem(e.description);
        descItem->setForeground(levelColor(e.level));
        ui.logTable->setItem(row, 1, descItem);
    }
}
