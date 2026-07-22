#include "alarm_catalog_widget.h"

#include <QCheckBox>
#include <QColor>
#include <QHeaderView>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableWidgetItem>

AlarmCatalogWidget::PendingEventConfig::PendingEventConfig()
    : originalDowngraded(false),
      downgraded(false),
      originalShielded(false),
      shielded(false) {}

AlarmCatalogWidget::PendingEventConfig::~PendingEventConfig() {}

bool AlarmCatalogWidget::PendingEventConfig::isDirty() const {
    return originalDowngraded != downgraded ||
           originalShielded != shielded ||
           originalActiveActions != activeActions ||
           originalClearActions != clearActions;
}

AlarmCatalogWidget::AlarmCatalogWidget(BackendBridge* bridge, QWidget* parent)
    : QWidget(parent),
      bridge_(bridge),
      pendingByEvent_(),
      catalog_(),
      selectedEventId_(),
      loadingUi_(false),
      actionGroupsByEvent_() {
    ui.setupUi(this);

    QList<int> splitterSizes;
    splitterSizes << 620 << 380;
    ui.configSplitter->setSizes(splitterSizes);

    ui.catalogTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    ui.catalogTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    ui.activeActionTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    ui.activeActionTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    ui.clearActionTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    ui.clearActionTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);

    connect(ui.refreshBtn, &QPushButton::clicked,
            this, &AlarmCatalogWidget::requestReload);
    connect(ui.catalogTable, &QTableWidget::currentCellChanged,
            this, &AlarmCatalogWidget::switchSelectedEvent);
    loadCatalog();
}

AlarmCatalogWidget::~AlarmCatalogWidget() {}

void AlarmCatalogWidget::loadCatalog() {
    // loadingUi_ prevents table reconstruction from becoming a staged edit.
    loadingUi_ = true;
    catalog_ = bridge_->getCatalog();
    pendingByEvent_.clear();
    actionGroupsByEvent_.clear();

    // Capture each event's backend state once so every UI edit remains staged.
    for (int i = 0; i < catalog_.size(); ++i) {
        const BackendBridge::CatalogEntry& entry = catalog_[i];
        const BackendBridge::EventActionGroups groups =
            bridge_->getEventActionGroups(entry.id, entry.originalLevel);
        PendingEventConfig pending;
        pending.originalDowngraded = entry.downgraded && entry.downgradeTo == 4;
        pending.downgraded = pending.originalDowngraded;
        pending.originalShielded = entry.shielded;
        pending.shielded = entry.shielded;

        for (int actionIndex = 0;
             actionIndex < groups.activeActions.size(); ++actionIndex) {
            const BackendBridge::ActionEntry& action =
                groups.activeActions[actionIndex];
            pending.originalActiveActions.insert(action.name, action.enabled);
            pending.activeActions.insert(action.name, action.enabled);
        }
        for (int actionIndex = 0;
             actionIndex < groups.clearActions.size(); ++actionIndex) {
            const BackendBridge::ActionEntry& action =
                groups.clearActions[actionIndex];
            pending.originalClearActions.insert(action.name, action.enabled);
            pending.clearActions.insert(action.name, action.enabled);
        }

        pendingByEvent_.insert(entry.id, pending);
        actionGroupsByEvent_.insert(entry.id, groups);
    }

    buildCatalogRows();
    selectInitialEvent();
    loadingUi_ = false;
    renderSelectedActions();
    ui.applyBtn->setEnabled(false);
    ui.statusLabel->setText(QString::fromUtf8(
        "共 %1 个报警定义，当前屏蔽 %2 个")
        .arg(catalog_.size()).arg(bridge_->shieldCount()));
}

void AlarmCatalogWidget::buildCatalogRows() {
    const QSignalBlocker blocker(ui.catalogTable);
    ui.catalogTable->clearContents();
    ui.catalogTable->setRowCount(catalog_.size());

    for (int row = 0; row < catalog_.size(); ++row) {
        const BackendBridge::CatalogEntry& entry = catalog_[row];
        const PendingEventConfig& pending = pendingByEvent_[entry.id];
        QTableWidgetItem* idItem = new QTableWidgetItem(entry.id);
        idItem->setForeground(QColor(38, 50, 56));
        ui.catalogTable->setItem(row, 0, idItem);
        ui.catalogTable->setItem(row, 1,
                                 new QTableWidgetItem(entry.description));

        QString levelText;
        QColor levelColor;
        switch (entry.originalLevel) {
            case 1:
                levelText = QString::fromUtf8("紧急");
                levelColor = QColor(183, 28, 28);
                break;
            case 2:
                levelText = QString::fromUtf8("严重");
                levelColor = QColor(230, 81, 0);
                break;
            case 3:
                levelText = QString::fromUtf8("一般");
                levelColor = QColor(130, 119, 23);
                break;
            default:
                levelText = QString::fromUtf8("提示");
                levelColor = QColor(2, 119, 189);
                break;
        }
        QTableWidgetItem* levelItem = new QTableWidgetItem(levelText);
        levelItem->setForeground(levelColor);
        ui.catalogTable->setItem(row, 2, levelItem);

        QCheckBox* downgradeCheckBox = new QCheckBox(ui.catalogTable);
        downgradeCheckBox->setChecked(pending.downgraded);
        downgradeCheckBox->setProperty("eventId", entry.id);
        connect(downgradeCheckBox, &QCheckBox::toggled,
                this, [this, entry](bool checked) {
            if (loadingUi_) return;
            pendingByEvent_[entry.id].downgraded = checked;
            updateDirtyUi();
        });
        ui.catalogTable->setCellWidget(row, 3, downgradeCheckBox);

        QCheckBox* shieldCheckBox = new QCheckBox(ui.catalogTable);
        shieldCheckBox->setChecked(pending.shielded);
        shieldCheckBox->setProperty("eventId", entry.id);
        connect(shieldCheckBox, &QCheckBox::toggled,
                this, [this, entry](bool checked) {
            if (loadingUi_) return;
            pendingByEvent_[entry.id].shielded = checked;
            updateDirtyUi();
        });
        ui.catalogTable->setCellWidget(row, 4, shieldCheckBox);

        const BackendBridge::EventActionGroups* groups =
            actionGroupsForEvent(entry.id);
        const int actionCount = groups
            ? groups->activeActions.size() + groups->clearActions.size() : 0;
        QTableWidgetItem* countItem =
            new QTableWidgetItem(QString::number(actionCount));
        countItem->setTextAlignment(Qt::AlignCenter);
        ui.catalogTable->setItem(row, 5, countItem);
    }
}

void AlarmCatalogWidget::selectInitialEvent() {
    int selectedRow = -1;
    for (int row = 0; row < catalog_.size(); ++row) {
        if (catalog_[row].id == selectedEventId_) {
            selectedRow = row;
            break;
        }
    }
    if (selectedRow < 0 && !catalog_.isEmpty()) selectedRow = 0;

    if (selectedRow >= 0) {
        selectedEventId_ = catalog_[selectedRow].id;
        ui.catalogTable->setCurrentCell(selectedRow, 0);
        ui.catalogTable->selectRow(selectedRow);
    } else {
        selectedEventId_.clear();
    }
    ui.selectedEventLabel->setText(selectedEventId_);
}

void AlarmCatalogWidget::switchSelectedEvent(int currentRow, int previousRow) {
    Q_UNUSED(previousRow);
    if (loadingUi_ || currentRow < 0 || currentRow >= catalog_.size()) return;

    persistVisibleActions();
    selectedEventId_ = catalog_[currentRow].id;
    ui.selectedEventLabel->setText(selectedEventId_);
    renderSelectedActions();
}

void AlarmCatalogWidget::persistVisibleActions() {
    if (loadingUi_ || selectedEventId_.isEmpty() ||
        !pendingByEvent_.contains(selectedEventId_)) return;

    QTableWidget* tables[] = {ui.activeActionTable, ui.clearActionTable};
    for (int phaseIndex = 0; phaseIndex < 2; ++phaseIndex) {
        QTableWidget* table = tables[phaseIndex];
        QMap<QString, bool>& values = phaseIndex == 0
            ? pendingByEvent_[selectedEventId_].activeActions
            : pendingByEvent_[selectedEventId_].clearActions;
        for (int row = 0; row < table->rowCount(); ++row) {
            QCheckBox* checkBox =
                qobject_cast<QCheckBox*>(table->cellWidget(row, 1));
            if (!checkBox) continue;
            values[checkBox->property("actionName").toString()] =
                checkBox->isChecked();
        }
    }
}

void AlarmCatalogWidget::renderSelectedActions() {
    loadingUi_ = true;
    const BackendBridge::EventActionGroups* groups =
        actionGroupsForEvent(selectedEventId_);
    if (!groups) {
        renderActionTable(ui.activeActionTable,
                          QVector<BackendBridge::ActionEntry>(), true);
        renderActionTable(ui.clearActionTable,
                          QVector<BackendBridge::ActionEntry>(), false);
    } else {
        renderActionTable(ui.activeActionTable, groups->activeActions, true);
        renderActionTable(ui.clearActionTable, groups->clearActions, false);
    }
    loadingUi_ = false;
}

void AlarmCatalogWidget::renderActionTable(
        QTableWidget* table,
        const QVector<BackendBridge::ActionEntry>& orderedActions,
        bool activePhase) {
    const QSignalBlocker blocker(table);
    table->clearContents();
    if (orderedActions.isEmpty()) {
        table->setRowCount(1);
        QTableWidgetItem* emptyItem =
            new QTableWidgetItem(QString::fromUtf8("无联动"));
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor(117, 117, 117));
        table->setItem(0, 0, emptyItem);
        return;
    }

    table->setRowCount(orderedActions.size());
    for (int row = 0; row < orderedActions.size(); ++row) {
        const BackendBridge::ActionEntry& action = orderedActions[row];
        const QString eventId = selectedEventId_;
        QTableWidgetItem* nameItem = new QTableWidgetItem(action.displayName);
        nameItem->setData(Qt::UserRole, action.name);
        table->setItem(row, 0, nameItem);

        QCheckBox* checkBox = new QCheckBox(table);
        const PendingEventConfig& pending = pendingByEvent_[eventId];
        const QMap<QString, bool>& values = activePhase
            ? pending.activeActions : pending.clearActions;
        checkBox->setChecked(values.value(action.name, action.enabled));
        checkBox->setProperty("actionName", action.name);
        checkBox->setProperty("displayName", action.displayName);
        checkBox->setProperty("eventId", eventId);
        checkBox->setProperty("phase", activePhase ? "active" : "clear");
        connect(checkBox, &QCheckBox::toggled,
                this, [this, action, eventId, activePhase](bool checked) {
            if (loadingUi_ || !pendingByEvent_.contains(eventId)) return;
            PendingEventConfig& pending = pendingByEvent_[eventId];
            if (activePhase) {
                pending.activeActions[action.name] = checked;
            } else {
                pending.clearActions[action.name] = checked;
            }
            updateDirtyUi();
        });
        table->setCellWidget(row, 1, checkBox);
    }
}

bool AlarmCatalogWidget::hasDirtyChanges() const {
    QMap<QString, PendingEventConfig>::const_iterator it =
        pendingByEvent_.constBegin();
    for (; it != pendingByEvent_.constEnd(); ++it) {
        if (it.value().isDirty()) return true;
    }
    return false;
}

void AlarmCatalogWidget::updateDirtyUi() {
    const bool dirty = hasDirtyChanges();
    ui.applyBtn->setEnabled(dirty);
    if (dirty) {
        ui.statusLabel->setText(QString::fromUtf8("有未应用配置"));
    } else {
        ui.statusLabel->setText(QString::fromUtf8(
            "共 %1 个报警定义，当前屏蔽 %2 个")
            .arg(catalog_.size()).arg(bridge_->shieldCount()));
    }
}

const BackendBridge::EventActionGroups*
AlarmCatalogWidget::actionGroupsForEvent(const QString& eventId) const {
    QMap<QString, BackendBridge::EventActionGroups>::const_iterator it =
        actionGroupsByEvent_.constFind(eventId);
    return it == actionGroupsByEvent_.constEnd() ? nullptr : &it.value();
}

AlarmCatalogWidget::DirtyDecision
AlarmCatalogWidget::confirmDirtyChanges() {
    QMessageBox messageBox(this);
    messageBox.setWindowTitle(QString::fromUtf8("未应用配置"));
    messageBox.setText(QString::fromUtf8("当前配置尚未应用。"));
    QPushButton* applyButton = messageBox.addButton(
        QString::fromUtf8("应用"), QMessageBox::AcceptRole);
    QPushButton* discardButton = messageBox.addButton(
        QString::fromUtf8("放弃"), QMessageBox::DestructiveRole);
    messageBox.addButton(QString::fromUtf8("取消"), QMessageBox::RejectRole);
    messageBox.exec();
    if (messageBox.clickedButton() == applyButton) return DirtyDecision::Apply;
    if (messageBox.clickedButton() == discardButton) return DirtyDecision::Discard;
    return DirtyDecision::Cancel;
}

void AlarmCatalogWidget::requestReload() {
    persistVisibleActions();
    if (!hasDirtyChanges()) {
        loadCatalog();
        return;
    }

    const DirtyDecision decision = confirmDirtyChanges();
    if (decision == DirtyDecision::Cancel) return;
    if (decision == DirtyDecision::Apply) {
        on_applyBtn_clicked();
        return;
    }
    loadCatalog();
}

void AlarmCatalogWidget::on_applyBtn_clicked() {
    // Task 6 retains the legacy catalog-only submission. Task 7 will submit
    // complete pending diffs, including both linkage phases, as one operation.
    for (int row = 0; row < ui.catalogTable->rowCount(); ++row) {
        QTableWidgetItem* idItem = ui.catalogTable->item(row, 0);
        if (!idItem) continue;
        const QString eventId = idItem->text();

        QCheckBox* downgradeCheckBox =
            qobject_cast<QCheckBox*>(ui.catalogTable->cellWidget(row, 3));
        if (downgradeCheckBox && downgradeCheckBox->isChecked()) {
            bridge_->setDowngrade(eventId, 4);
        } else {
            bridge_->removeDowngrade(eventId);
        }

        QCheckBox* shieldCheckBox =
            qobject_cast<QCheckBox*>(ui.catalogTable->cellWidget(row, 4));
        if (shieldCheckBox && shieldCheckBox->isChecked()) {
            bridge_->setShield(eventId);
        } else {
            bridge_->clearShield(eventId);
        }
    }
    loadCatalog();
}
