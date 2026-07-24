#include "alarm_catalog_widget.h"

#include <QCheckBox>
#include <QColor>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

// Keeps the complete action identity available while painting a width-safe label.
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(const QString& fullText,
                         const QString& accessibleRole,
                         QWidget* parent = nullptr)
        : QLabel(fullText, parent),
          fullText_(fullText),
          displayText_() {
        setToolTip(fullText_);
        setAccessibleName(fullText_);
        setAccessibleDescription(
            QString::fromUtf8("%1：%2").arg(accessibleRole, fullText_));
        updateDisplayText();
    }

    ~ElidedLabel() override {}

    QSize minimumSizeHint() const override {
        QSize hint = QLabel::minimumSizeHint();
        hint.setWidth(0);
        return hint;
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(palette().color(foregroundRole()));
        painter.drawText(contentsRect(), alignment(), displayText_);
    }

    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        updateDisplayText();
    }

    void changeEvent(QEvent* event) override {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::FontChange ||
            event->type() == QEvent::StyleChange) {
            updateDisplayText();
        }
    }

private:
    void updateDisplayText() {
        displayText_ = fontMetrics().elidedText(
            fullText_, Qt::ElideRight, qMax(0, contentsRect().width()));
        setProperty("displayText", displayText_);
        update();
    }

    QString fullText_;
    QString displayText_;
};

} // namespace

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
      dirtyPromptActive_(false),
      actionGroupsByEvent_() {
    ui.setupUi(this);

    QList<int> splitterSizes;
    splitterSizes << 620 << 380;
    ui.configSplitter->setSizes(splitterSizes);

    ui.catalogTree->header()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    ui.catalogTree->header()->setSectionResizeMode(
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
    connect(ui.catalogTree, &QTreeWidget::currentItemChanged,
            this, &AlarmCatalogWidget::switchSelectedEvent);
    loadCatalog();
}

AlarmCatalogWidget::~AlarmCatalogWidget() {}

void AlarmCatalogWidget::loadCatalog() {
    if (hasDirtyChanges()) {
        requestReload();
        return;
    }
    reloadFromBackend();
}

void AlarmCatalogWidget::reloadFromBackend() {
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
    const QSignalBlocker blocker(ui.catalogTree);
    ui.catalogTree->clear();

    // Group by device/module name (first segment of "名称-frameID-字段名")
    // Track whether group is device (all isSystem=false) or system module
    QMap<QString, QVector<int>> groups;
    QMap<QString, bool> groupIsSystem;
    for (int i = 0; i < catalog_.size(); ++i) {
        const QString deviceName = catalog_[i].id.section('-', 0, 0);
        groups[deviceName].append(i);
        if (catalog_[i].isSystem)
            groupIsSystem[deviceName] = true;
    }

    // Sort: device groups alphabetically first, system module groups last
    QStringList sortedKeys = groups.keys();
    sortedKeys.sort();
    QStringList deviceKeys, systemKeys;
    for (int i = 0; i < sortedKeys.size(); ++i) {
        if (groupIsSystem.value(sortedKeys[i], false))
            systemKeys.append(sortedKeys[i]);
        else
            deviceKeys.append(sortedKeys[i]);
    }
    sortedKeys = deviceKeys + systemKeys;

    for (int gi = 0; gi < sortedKeys.size(); ++gi) {
        const QString& deviceName = sortedKeys[gi];
        const QVector<int>& indices = groups[deviceName];

        QTreeWidgetItem* parent = new QTreeWidgetItem(ui.catalogTree);
        parent->setText(0, deviceName);
        parent->setText(1, QString("(%1 个报警)").arg(indices.size()));
        QFont boldFont = parent->font(0);
        boldFont.setBold(true);
        parent->setFont(0, boldFont);
        parent->setFlags(parent->flags() & ~Qt::ItemIsSelectable);

        for (int i = 0; i < indices.size(); ++i) {
            int row = indices[i];
            const BackendBridge::CatalogEntry& entry = catalog_[row];
            const PendingEventConfig& pending = pendingByEvent_[entry.id];

            QTreeWidgetItem* child = new QTreeWidgetItem(parent);
            child->setData(0, Qt::UserRole, entry.id);
            child->setText(0, entry.id);
            child->setForeground(0, QColor(38, 50, 56));
            child->setText(1, entry.description);

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
            child->setText(2, levelText);
            child->setForeground(2, levelColor);

            QCheckBox* downgradeCheckBox = new QCheckBox(ui.catalogTree);
            downgradeCheckBox->setChecked(pending.downgraded);
            downgradeCheckBox->setProperty("eventId", entry.id);
            connect(downgradeCheckBox, &QCheckBox::toggled,
                    this, [this, entry](bool checked) {
                if (loadingUi_) return;
                pendingByEvent_[entry.id].downgraded = checked;
                updateDirtyUi();
            });
            ui.catalogTree->setItemWidget(child, 3, downgradeCheckBox);

            QCheckBox* shieldCheckBox = new QCheckBox(ui.catalogTree);
            shieldCheckBox->setChecked(pending.shielded);
            shieldCheckBox->setProperty("eventId", entry.id);
            connect(shieldCheckBox, &QCheckBox::toggled,
                    this, [this, entry](bool checked) {
                if (loadingUi_) return;
                pendingByEvent_[entry.id].shielded = checked;
                updateDirtyUi();
            });
            ui.catalogTree->setItemWidget(child, 4, shieldCheckBox);

            const BackendBridge::EventActionGroups* groups =
                actionGroupsForEvent(entry.id);
            const int actionCount = groups
                ? groups->activeActions.size() + groups->clearActions.size() : 0;
            QTableWidgetItem* countItem =
                new QTableWidgetItem(actionCount == 0
                    ? QString::fromUtf8("无") : QString::number(actionCount));
            countItem->setTextAlignment(Qt::AlignCenter);
            child->setText(5, actionCount == 0
                ? QString::fromUtf8("无") : QString::number(actionCount));
        }

        parent->setExpanded(true);
    }
}

void AlarmCatalogWidget::selectInitialEvent() {
    QTreeWidgetItem* found = nullptr;
    for (int p = 0; p < ui.catalogTree->topLevelItemCount(); ++p) {
        QTreeWidgetItem* parent = ui.catalogTree->topLevelItem(p);
        for (int c = 0; c < parent->childCount(); ++c) {
            QTreeWidgetItem* child = parent->child(c);
            if (child->data(0, Qt::UserRole).toString() == selectedEventId_) {
                found = child;
                break;
            }
        }
        if (found) break;
    }
    if (!found && ui.catalogTree->topLevelItemCount() > 0) {
        QTreeWidgetItem* firstParent = ui.catalogTree->topLevelItem(0);
        if (firstParent->childCount() > 0)
            found = firstParent->child(0);
    }

    if (found) {
        selectedEventId_ = found->data(0, Qt::UserRole).toString();
        ui.catalogTree->setCurrentItem(found);
        found->parent()->setExpanded(true);
    } else {
        selectedEventId_.clear();
    }
    ui.selectedEventLabel->setText(selectedEventId_);
}

void AlarmCatalogWidget::switchSelectedEvent(QTreeWidgetItem* current,
                                              QTreeWidgetItem* previous) {
    Q_UNUSED(previous);
    if (loadingUi_ || !current) return;
    // Ignore parent (group) items
    if (!current->parent()) return;

    selectedEventId_ = current->data(0, Qt::UserRole).toString();
    ui.selectedEventLabel->setText(selectedEventId_);
    renderSelectedActions();
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
        QTableWidgetItem* nameItem = new QTableWidgetItem();
        nameItem->setData(Qt::UserRole, action.name);
        table->setItem(row, 0, nameItem);

        // Two compact lines distinguish the operator-facing label from the
        // exact backend key without adding source metadata or another column.
        QWidget* nameCell = new QWidget(table);
        nameCell->setProperty("actionName", action.name);
        nameCell->setProperty("displayName", action.displayName);
        QVBoxLayout* nameLayout = new QVBoxLayout(nameCell);
        nameLayout->setContentsMargins(6, 2, 4, 2);
        nameLayout->setSpacing(0);
        QLabel* displayLabel = new ElidedLabel(
            action.displayName, QString::fromUtf8("动作显示名称"), nameCell);
        displayLabel->setStyleSheet(
            QString::fromLatin1("font-weight:600; color:#263238;"));
        QLabel* internalLabel = new ElidedLabel(
            action.name, QString::fromUtf8("动作内部名称"), nameCell);
        internalLabel->setStyleSheet(
            QString::fromLatin1("font-size:11px; color:#607d8b;"));
        nameLayout->addWidget(displayLabel);
        nameLayout->addWidget(internalLabel);
        table->setCellWidget(row, 0, nameCell);
        table->setRowHeight(row, 44);

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
    if (!hasDirtyChanges()) {
        reloadFromBackend();
        return;
    }
    // QMessageBox runs a nested event loop; ignore queued reloads until resolved.
    if (dirtyPromptActive_) return;

    dirtyPromptActive_ = true;
    const DirtyDecision decision = confirmDirtyChanges();
    dirtyPromptActive_ = false;
    if (decision == DirtyDecision::Cancel) return;
    if (decision == DirtyDecision::Apply) {
        on_applyBtn_clicked();
        return;
    }
    reloadFromBackend();
}

bool AlarmCatalogWidget::requestLeave() {
    if (!hasDirtyChanges()) return true;
    if (dirtyPromptActive_) return false;

    dirtyPromptActive_ = true;
    const DirtyDecision decision = confirmDirtyChanges();
    dirtyPromptActive_ = false;
    if (decision == DirtyDecision::Cancel) return false;
    if (decision == DirtyDecision::Apply) {
        on_applyBtn_clicked();
        return true;
    }

    reloadFromBackend();
    return true;
}

void AlarmCatalogWidget::applyActionDiffs(
        const QString& eventId,
        const QMap<QString, bool>& original,
        const QMap<QString, bool>& current,
        bool isActive) {
    QMap<QString, bool>::const_iterator action = current.constBegin();
    for (; action != current.constEnd(); ++action) {
        if (original.value(action.key()) == action.value()) continue;
        if (action.value()) {
            bridge_->enableAction(eventId, action.key(), isActive);
        } else {
            bridge_->disableAction(eventId, action.key(), isActive);
        }
    }
}

void AlarmCatalogWidget::on_applyBtn_clicked() {
    if (!hasDirtyChanges()) return;

    const QString selectedEventId = selectedEventId_;
    QMap<QString, PendingEventConfig>::const_iterator pending =
        pendingByEvent_.constBegin();
    for (; pending != pendingByEvent_.constEnd(); ++pending) {
        const PendingEventConfig& config = pending.value();
        if (!config.isDirty()) continue;

        if (config.originalDowngraded != config.downgraded) {
            if (config.downgraded) {
                bridge_->setDowngrade(pending.key(), 4);
            } else {
                bridge_->removeDowngrade(pending.key());
            }
        }
        if (config.originalShielded != config.shielded) {
            if (config.shielded) {
                bridge_->setShield(pending.key());
            } else {
                bridge_->clearShield(pending.key());
            }
        }

        int originalLevel = 4;
        for (int catalogIndex = 0; catalogIndex < catalog_.size(); ++catalogIndex) {
            if (catalog_[catalogIndex].id == pending.key()) {
                originalLevel = catalog_[catalogIndex].originalLevel;
                break;
            }
        }
        const BackendBridge::EventActionGroups liveGroups =
            bridge_->getEventActionGroups(pending.key(), originalLevel);
        QMap<QString, bool> liveOriginalActive;
        QMap<QString, bool> liveCurrentActive;
        QMap<QString, bool> liveOriginalClear;
        QMap<QString, bool> liveCurrentClear;
        for (int actionIndex = 0;
             actionIndex < liveGroups.activeActions.size(); ++actionIndex) {
            const QString& name = liveGroups.activeActions[actionIndex].name;
            if (!config.activeActions.contains(name)) continue;
            liveOriginalActive.insert(name,
                                      config.originalActiveActions.value(name));
            liveCurrentActive.insert(name, config.activeActions.value(name));
        }
        for (int actionIndex = 0;
             actionIndex < liveGroups.clearActions.size(); ++actionIndex) {
            const QString& name = liveGroups.clearActions[actionIndex].name;
            if (!config.clearActions.contains(name)) continue;
            liveOriginalClear.insert(name,
                                     config.originalClearActions.value(name));
            liveCurrentClear.insert(name, config.clearActions.value(name));
        }

        // Best-effort live membership validation skips removed or rephased edits.
        // Existing void APIs leave a non-transactional query/write boundary.
        applyActionDiffs(pending.key(), liveOriginalActive,
                         liveCurrentActive, true);
        applyActionDiffs(pending.key(), liveOriginalClear,
                         liveCurrentClear, false);
    }

    selectedEventId_ = selectedEventId;
    reloadFromBackend();
    ui.statusLabel->setText(QString::fromUtf8("配置已应用"));
}
