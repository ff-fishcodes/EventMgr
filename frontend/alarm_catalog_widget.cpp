#include "alarm_catalog_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QCheckBox>
#include <QComboBox>
#include <QColor>

AlarmCatalogWidget::AlarmCatalogWidget(BackendBridge* bridge, QWidget* parent)
    : QWidget(parent), bridge_(bridge) {
    setupUI();
    loadCatalog();
}

void AlarmCatalogWidget::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* title = new QLabel("报警配置管理");
    title->setStyleSheet("font-size:16px; font-weight:bold;");
    layout->addWidget(title);

    QLabel* hint = new QLabel("可在此页对任一报警预先设置降级或屏蔽，报警产生时自动生效。");
    hint->setStyleSheet("color:gray;");
    layout->addWidget(hint);

    // 表格：ID | 描述 | 原始等级 | 降级到提示 | 屏蔽展示
    table_ = new QTableWidget(0, 5, this);
    table_->setHorizontalHeaderLabels({
        "事件编号", "报警描述", "原始等级", "降级为提示", "屏蔽展示"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_);

    // 按钮栏
    QHBoxLayout* btnLayout = new QHBoxLayout();

    QPushButton* loadBtn = new QPushButton("刷新", this);
    connect(loadBtn, SIGNAL(clicked()), this, SLOT(loadCatalog()));

    QPushButton* applyBtn = new QPushButton("应用配置", this);
    applyBtn->setStyleSheet("font-weight:bold;");
    connect(applyBtn, SIGNAL(clicked()), this, SLOT(onApply()));

    btnLayout->addWidget(loadBtn);
    btnLayout->addWidget(applyBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    statusLabel_ = new QLabel();
    layout->addWidget(statusLabel_);
}

void AlarmCatalogWidget::loadCatalog() {
    table_->setRowCount(0);

    QVector<BackendBridge::CatalogEntry> catalog = bridge_->getCatalog();
    for (int i = 0; i < catalog.size(); ++i) {
        const BackendBridge::CatalogEntry& e = catalog[i];
        int row = table_->rowCount();
        table_->insertRow(row);

        // 0: 事件编号
        table_->setItem(row, 0, new QTableWidgetItem(e.id));

        // 1: 描述
        table_->setItem(row, 1, new QTableWidgetItem(e.description));

        // 2: 原始等级（带颜色）
        QString levelStr;
        QColor color;
        switch (e.originalLevel) {
            case 1: levelStr = "紧急"; color = Qt::red;        break;
            case 2: levelStr = "严重"; color = QColor(255,140,0); break;
            case 3: levelStr = "一般"; color = QColor(200,200,0); break;
            default: levelStr = "提示"; color = Qt::blue;      break;
        }
        QTableWidgetItem* levelItem = new QTableWidgetItem(levelStr);
        levelItem->setForeground(color);
        table_->setItem(row, 2, levelItem);

        // 3: 降级为提示 复选框
        QCheckBox* downgradeCb = new QCheckBox();
        downgradeCb->setChecked(e.downgraded && e.downgradeTo == 4);
        downgradeCb->setProperty("eventId", e.id);
        table_->setCellWidget(row, 3, downgradeCb);

        // 4: 屏蔽展示 复选框
        QCheckBox* shieldCb = new QCheckBox();
        shieldCb->setChecked(e.shielded);
        shieldCb->setProperty("eventId", e.id);
        table_->setCellWidget(row, 4, shieldCb);
    }

    statusLabel_->setText(QString("共 %1 个报警定义，当前屏蔽 %2 个")
        .arg(catalog.size()).arg(bridge_->shieldCount()));
}

void AlarmCatalogWidget::onApply() {
    for (int row = 0; row < table_->rowCount(); ++row) {
        QString id = table_->item(row, 0)->text();

        // 降级
        QCheckBox* downgradeCb =
            qobject_cast<QCheckBox*>(table_->cellWidget(row, 3));
        if (downgradeCb) {
            if (downgradeCb->isChecked()) {
                bridge_->setDowngrade(id, 4);  // 降级为提示
            } else {
                bridge_->removeDowngrade(id);
            }
        }

        // 屏蔽
        QCheckBox* shieldCb =
            qobject_cast<QCheckBox*>(table_->cellWidget(row, 4));
        if (shieldCb) {
            if (shieldCb->isChecked()) {
                bridge_->setShield(id);
            } else {
                bridge_->clearShield(id);
            }
        }
    }
    loadCatalog(); // 刷新显示
}
