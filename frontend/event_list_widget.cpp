#include "event_list_widget.h"
#include <QCheckBox>
#include <QColor>

static QColor levelColor(int level) {
    switch (level) {
        case 1: return Qt::red;
        case 2: return QColor(255, 140, 0);
        case 3: return QColor(200, 200, 0);
        default: return Qt::blue;
    }
}

static QString levelText(int level) {
    switch (level) {
        case 1: return "紧急";
        case 2: return "严重";
        case 3: return "一般";
        default: return "提示";
    }
}

EventListWidget::EventListWidget(BackendBridge* bridge, QWidget* parent)
    : QWidget(parent), bridge_(bridge) {
    ui.setupUi(this);
    fillCombo();
    refresh();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, SIGNAL(timeout()), this, SLOT(refresh()));
    refreshTimer_->start(1000);
}

void EventListWidget::fillCombo() {
    QVector<BackendBridge::CatalogEntry> catalog = bridge_->getCatalog();
    for (int i = 0; i < catalog.size(); ++i) {
        ui.alarmCombo->addItem(
            catalog[i].description + " (" + catalog[i].id + ")",
            catalog[i].id);
    }
}

void EventListWidget::on_simBtn_clicked() {
    QString id = ui.alarmCombo->currentData().toString();
    if (id.isEmpty()) return;
    bridge_->triggerAlarm(id, true);
    refresh();
}

void EventListWidget::refresh() {
    ui.eventTable->setRowCount(0);

    QVector<BackendBridge::EventEntry> events = bridge_->getActiveEvents();

    for (int i = 0; i < events.size(); ++i) {
        const BackendBridge::EventEntry& e = events[i];
        if (e.shielded) continue;  // 被屏蔽的事件不显示
        int row = ui.eventTable->rowCount();
        ui.eventTable->insertRow(row);

        ui.eventTable->setItem(row, 0, new QTableWidgetItem(e.id));
        ui.eventTable->setItem(row, 1, new QTableWidgetItem(e.description));
        ui.eventTable->setItem(row, 2, new QTableWidgetItem(e.timestamp));

        QTableWidgetItem* levelItem = new QTableWidgetItem(levelText(e.level));
        levelItem->setForeground(levelColor(e.level));
        ui.eventTable->setItem(row, 3, levelItem);

        // 降级/屏蔽状态直接来自 ConfigManager（设备事件和系统事件通用）
        ui.eventTable->setItem(row, 4,
            new QTableWidgetItem(e.downgraded ? "已降级" : "正常"));

        // 降级复选框
        QCheckBox* downgradeCb = new QCheckBox();
        downgradeCb->setChecked(e.downgraded);
        QString rowId = e.id;
        connect(downgradeCb, &QCheckBox::clicked, this, [this, rowId](bool checked) {
            if (checked) bridge_->setDowngrade(rowId, 4);
            else         bridge_->removeDowngrade(rowId);
            refresh();
        });
        ui.eventTable->setCellWidget(row, 5, downgradeCb);

        // 屏蔽复选框
        QCheckBox* shieldCb = new QCheckBox();
        shieldCb->setChecked(e.shielded);
        connect(shieldCb, &QCheckBox::clicked, this, [this, rowId](bool checked) {
            if (checked) bridge_->setShield(rowId);
            else         bridge_->clearShield(rowId);
            refresh();
        });
        ui.eventTable->setCellWidget(row, 6, shieldCb);
    }

    ui.statusLabel->setText(QString("活跃 %1 个  屏蔽 %2 个")
        .arg(events.size()).arg(bridge_->shieldCount()));
}
