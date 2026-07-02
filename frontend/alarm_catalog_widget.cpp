#include "alarm_catalog_widget.h"
#include <QCheckBox>
#include <QColor>
#include <QHeaderView>

AlarmCatalogWidget::AlarmCatalogWidget(BackendBridge* bridge, QWidget* parent)
    : QWidget(parent), bridge_(bridge) {
    ui.setupUi(this);
    connect(ui.refreshBtn, SIGNAL(clicked()), this, SLOT(loadCatalog()));
    loadCatalog();
}

void AlarmCatalogWidget::loadCatalog() {
    ui.catalogTable->setRowCount(0);

    QVector<BackendBridge::CatalogEntry> catalog = bridge_->getCatalog();
    for (int i = 0; i < catalog.size(); ++i) {
        const BackendBridge::CatalogEntry& e = catalog[i];
        int row = ui.catalogTable->rowCount();
        ui.catalogTable->insertRow(row);

        // 0: 事件编号
        ui.catalogTable->setItem(row, 0, new QTableWidgetItem(e.id));

        // 1: 描述
        ui.catalogTable->setItem(row, 1, new QTableWidgetItem(e.description));

        // 2: 原始等级（带颜色）
        QString levelStr;
        QColor color;
        switch (e.originalLevel) {
            case 1: levelStr = "紧急"; color = Qt::red;          break;
            case 2: levelStr = "严重"; color = QColor(255,140,0); break;
            case 3: levelStr = "一般"; color = QColor(200,200,0); break;
            default: levelStr = "提示"; color = Qt::blue;        break;
        }
        QTableWidgetItem* levelItem = new QTableWidgetItem(levelStr);
        levelItem->setForeground(color);
        ui.catalogTable->setItem(row, 2, levelItem);

        // 3: 降级复选框
        QCheckBox* downgradeCb = new QCheckBox();
        downgradeCb->setChecked(e.downgraded && e.downgradeTo == 4);
        downgradeCb->setProperty("eventId", e.id);
        ui.catalogTable->setCellWidget(row, 3, downgradeCb);

        // 4: 屏蔽复选框
        QCheckBox* shieldCb = new QCheckBox();
        shieldCb->setChecked(e.shielded);
        shieldCb->setProperty("eventId", e.id);
        ui.catalogTable->setCellWidget(row, 4, shieldCb);
    }

    ui.statusLabel->setText(QString("共 %1 个报警定义，当前屏蔽 %2 个")
        .arg(catalog.size()).arg(bridge_->shieldCount()));
}

void AlarmCatalogWidget::on_applyBtn_clicked() {
    for (int row = 0; row < ui.catalogTable->rowCount(); ++row) {
        QString id = ui.catalogTable->item(row, 0)->text();

        QCheckBox* downgradeCb =
            qobject_cast<QCheckBox*>(ui.catalogTable->cellWidget(row, 3));
        if (downgradeCb) {
            if (downgradeCb->isChecked()) {
                bridge_->setDowngrade(id, 4);
            } else {
                bridge_->removeDowngrade(id);
            }
        }

        QCheckBox* shieldCb =
            qobject_cast<QCheckBox*>(ui.catalogTable->cellWidget(row, 4));
        if (shieldCb) {
            if (shieldCb->isChecked()) {
                bridge_->setShield(id);
            } else {
                bridge_->clearShield(id);
            }
        }
    }
    loadCatalog();
}
