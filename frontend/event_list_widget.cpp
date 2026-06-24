#include "event_list_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QColor>
#include <QComboBox>

EventListWidget::EventListWidget(BackendBridge* bridge, QWidget* parent)
    : QWidget(parent), bridge_(bridge) {
    setupUI();
}

void EventListWidget::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);

    // 标题
    QLabel* title = new QLabel("当前活跃事件");
    title->setStyleSheet("font-size:16px; font-weight:bold;");
    layout->addWidget(title);

    // 表格
    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels({"事件编号", "描述", "等级", "状态", "屏蔽"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_);

    // 模拟报警按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QComboBox* combo = new QComboBox(this);
    QVector<BackendBridge::CatalogEntry> catalog = bridge_->getCatalog();
    for (int i = 0; i < catalog.size(); ++i) {
        combo->addItem(catalog[i].description + " (" + catalog[i].id + ")",
                       catalog[i].id);
    }
    combo->setObjectName("alarmCombo");

    QPushButton* btn = new QPushButton("模拟报警", this);
    btn->setObjectName("simBtn");
    connect(btn, SIGNAL(clicked()), this, SLOT(onSimulateBtn()));

    btnLayout->addWidget(combo);
    btnLayout->addWidget(btn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // 状态
    statusLabel_ = new QLabel("就绪");
    layout->addWidget(statusLabel_);
}

void EventListWidget::onSimulateBtn() {
    QComboBox* combo = findChild<QComboBox*>("alarmCombo");
    if (!combo) return;
    QString id = combo->currentData().toString();

    // 从 catalog 找定义
    QVector<BackendBridge::CatalogEntry> catalog = bridge_->getCatalog();
    for (int i = 0; i < catalog.size(); ++i) {
        if (catalog[i].id == id) {
            const BackendBridge::CatalogEntry& e = catalog[i];
            Event event = bridge_->api().createAlarm(
                e.id.section('-', 0, 0).toInt(),
                e.id.section('-', 1, 1).toInt(),
                e.id.section('-', 2).toStdString(),
                static_cast<EventLevel>(e.originalLevel),
                e.description.toStdString());
            bridge_->api().addEvent(event);
            break;
        }
    }
    refresh();
}

void EventListWidget::refresh() {
    table_->setRowCount(0);

    QVector<BackendBridge::CatalogEntry> catalog = bridge_->getCatalog();
    // 遍历 catalog，对已在 ConfigManager 有配置的项展示当前状态
    // 实际操作中应结合活跃事件表，此处简化展示

    statusLabel_->setText("已刷新");
}

void EventListWidget::addRow(const BackendBridge::CatalogEntry& entry) {
    int row = table_->rowCount();
    table_->insertRow(row);

    table_->setItem(row, 0, new QTableWidgetItem(entry.id));
    table_->setItem(row, 1, new QTableWidgetItem(entry.description));

    QString levelStr;
    QColor color;
    switch (entry.originalLevel) {
        case 1: levelStr = "紧急"; color = Qt::red;     break;
        case 2: levelStr = "严重"; color = QColor(255,140,0); break;
        case 3: levelStr = "一般"; color = QColor(200,200,0); break;
        default: levelStr = "提示"; color = Qt::blue;   break;
    }

    QTableWidgetItem* levelItem = new QTableWidgetItem(levelStr);
    levelItem->setForeground(color);
    table_->setItem(row, 2, levelItem);

    table_->setItem(row, 3, new QTableWidgetItem("活跃"));
    table_->setItem(row, 4, new QTableWidgetItem(entry.shielded ? "是" : "否"));
}
