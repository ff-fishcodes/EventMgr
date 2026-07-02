#include "event_list_widget.h"
#include <QPushButton>
#include <QColor>
#include <QHeaderView>

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

void EventListWidget::on_clearBtn_clicked() {
    int row = ui.eventTable->currentRow();
    if (row < 0) return;
    QTableWidgetItem* item = ui.eventTable->item(row, 0);
    if (!item) return;
    bridge_->triggerAlarm(item->text(), false);
    refresh();
}

void EventListWidget::onClearRow(const QString& id) {
    bridge_->triggerAlarm(id, false);
    refresh();
}

void EventListWidget::refresh() {
    ui.eventTable->setRowCount(0);

    QVector<BackendBridge::EventEntry> events = bridge_->getActiveEvents();
    for (int i = 0; i < events.size(); ++i) {
        const BackendBridge::EventEntry& e = events[i];
        int row = ui.eventTable->rowCount();
        ui.eventTable->insertRow(row);

        ui.eventTable->setItem(row, 0, new QTableWidgetItem(e.id));
        ui.eventTable->setItem(row, 1, new QTableWidgetItem(e.description));

        QTableWidgetItem* levelItem = new QTableWidgetItem(levelText(e.level));
        levelItem->setForeground(levelColor(e.level));
        ui.eventTable->setItem(row, 2, levelItem);

        ui.eventTable->setItem(row, 3, new QTableWidgetItem(e.shielded ? "是" : "否"));

        QPushButton* clearRowBtn = new QPushButton("消除");
        QString rowId = e.id;
        connect(clearRowBtn, &QPushButton::clicked, this, [this, rowId]() {
            onClearRow(rowId);
        });
        ui.eventTable->setCellWidget(row, 4, clearRowBtn);
    }

    ui.statusLabel->setText(QString("活跃 %1 个  屏蔽 %2 个")
        .arg(events.size()).arg(bridge_->shieldCount()));
}
