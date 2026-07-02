#include "event_list_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QColor>
#include <QComboBox>

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
    setupUI();
    refresh();

    // 每秒自动刷新
    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, SIGNAL(timeout()), this, SLOT(refresh()));
    refreshTimer_->start(1000);
}

void EventListWidget::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* title = new QLabel("当前活跃事件");
    title->setStyleSheet("font-size:16px; font-weight:bold;");
    layout->addWidget(title);

    // 表格：ID | 描述 | 等级 | 屏蔽 | 操作
    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels(
        {"事件编号", "描述", "等级", "屏蔽", "操作"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_);

    // 按钮栏：模拟报警 + 清除选中
    QHBoxLayout* btnLayout = new QHBoxLayout();

    QComboBox* combo = new QComboBox(this);
    QVector<BackendBridge::CatalogEntry> catalog = bridge_->getCatalog();
    for (int i = 0; i < catalog.size(); ++i) {
        combo->addItem(catalog[i].description + " (" + catalog[i].id + ")",
                       catalog[i].id);
    }
    combo->setObjectName("alarmCombo");

    QPushButton* simBtn = new QPushButton("模拟报警", this);
    connect(simBtn, SIGNAL(clicked()), this, SLOT(onSimulateBtn()));

    QPushButton* clearBtn = new QPushButton("清除选中", this);
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(onClearSelected()));

    btnLayout->addWidget(combo);
    btnLayout->addWidget(simBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    statusLabel_ = new QLabel("就绪");
    layout->addWidget(statusLabel_);
}

void EventListWidget::onSimulateBtn() {
    QComboBox* combo = findChild<QComboBox*>("alarmCombo");
    if (!combo) return;
    QString id = combo->currentData().toString();

    // 使用 triggerAlarm（observe 对接方式）
    bridge_->triggerAlarm(id, true);
    refresh();
}

void EventListWidget::onClearSelected() {
    int row = table_->currentRow();
    if (row < 0) return;

    QTableWidgetItem* item = table_->item(row, 0);
    if (!item) return;

    bridge_->triggerAlarm(item->text(), false);
    refresh();
}

void EventListWidget::refresh() {
    table_->setRowCount(0);

    QVector<BackendBridge::EventEntry> events = bridge_->getActiveEvents();
    for (int i = 0; i < events.size(); ++i) {
        const BackendBridge::EventEntry& e = events[i];
        int row = table_->rowCount();
        table_->insertRow(row);

        table_->setItem(row, 0, new QTableWidgetItem(e.id));
        table_->setItem(row, 1, new QTableWidgetItem(e.description));

        QTableWidgetItem* levelItem = new QTableWidgetItem(levelText(e.level));
        levelItem->setForeground(levelColor(e.level));
        table_->setItem(row, 2, levelItem);

        QTableWidgetItem* shieldItem = new QTableWidgetItem(e.shielded ? "是" : "否");
        table_->setItem(row, 3, shieldItem);

        QPushButton* clearRowBtn = new QPushButton("消除");
        connect(clearRowBtn, &QPushButton::clicked, this, [this, id = e.id]() {
            bridge_->triggerAlarm(id, false);
            refresh();
        });
        table_->setCellWidget(row, 4, clearRowBtn);
    }

    statusLabel_->setText(QString("活跃 %1 个  屏蔽 %2 个")
        .arg(events.size()).arg(bridge_->shieldCount()));
}
