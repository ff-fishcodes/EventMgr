#include <QtTest>

#include <QCheckBox>
#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QQueue>
#include <QSplitter>
#include <QTableWidget>

#include "action_registry.h"
#include "alarm_catalog_widget.h"
#include "backend_bridge.h"
#include "config_manager.h"
#include "event_manager.h"
#include "event_mgr_module.h"
#include "linkage_engine.h"

namespace {

const QString kBoilerEvent = QString::fromUtf8("锅炉-3-temp_high");
const QString kOtherEvent = QString::fromUtf8("锅炉-3-pressure_low");

template<typename T>
T* requiredChild(QObject* parent, const char* objectName) {
    T* child = parent->findChild<T*>(QString::fromLatin1(objectName));
    if (!child) {
        QTest::qFail(qPrintable(QString("Required object '%1' was not found")
                                   .arg(QString::fromLatin1(objectName))),
                     __FILE__, __LINE__);
    }
    return child;
}

int catalogRow(QTableWidget* table, const QString& eventId) {
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* idItem = table->item(row, 0);
        if (idItem && idItem->text() == eventId) return row;
    }
    QTest::qFail(qPrintable(QString("Catalog row for '%1' was not found")
                               .arg(eventId)),
                 __FILE__, __LINE__);
    return -1;
}

void selectCatalogRow(QTableWidget* table, const QString& eventId) {
    const int row = catalogRow(table, eventId);
    table->setCurrentCell(row, 0);
    table->selectRow(row);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

QCheckBox* tableCheckBox(QTableWidget* table, int row, int column) {
    QWidget* cell = table->cellWidget(row, column);
    QCheckBox* checkBox = qobject_cast<QCheckBox*>(cell);
    if (!checkBox && cell) checkBox = cell->findChild<QCheckBox*>();
    if (!checkBox) {
        QTest::qFail(qPrintable(QString("Checkbox at row %1, column %2 was not found")
                                   .arg(row).arg(column)),
                     __FILE__, __LINE__);
    }
    return checkBox;
}

int actionRow(QTableWidget* table,
              const QString& actionName,
              const QString& displayName) {
    for (int row = 0; row < table->rowCount(); ++row) {
        for (int column = 0; column < table->columnCount(); ++column) {
            QTableWidgetItem* item = table->item(row, column);
            if (item && (item->text() == actionName || item->text() == displayName)) {
                return row;
            }
            QWidget* cell = table->cellWidget(row, column);
            if (cell && (cell->property("actionName").toString() == actionName ||
                         cell->property("displayName").toString() == displayName)) {
                return row;
            }
        }
    }
    QTest::qFail(qPrintable(QString("Action '%1' (%2) was not found")
                               .arg(actionName, displayName)),
                 __FILE__, __LINE__);
    return -1;
}

bool phaseActionEnabled(const BackendBridge::EventActionGroups& groups,
                        const QString& actionName,
                        bool activePhase) {
    const QVector<BackendBridge::ActionEntry>& actions =
        activePhase ? groups.activeActions : groups.clearActions;
    for (int i = 0; i < actions.size(); ++i) {
        if (actions[i].name == actionName) return actions[i].enabled;
    }
    QTest::qFail(qPrintable(QString("Backend action '%1' was not found in %2 phase")
                               .arg(actionName, activePhase ? "active" : "clear")),
                 __FILE__, __LINE__);
    return false;
}

void toggle(QCheckBox* checkBox, bool checked) {
    if (!checkBox) return;
    if (checkBox->isChecked() != checked) checkBox->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

bool checked(QCheckBox* checkBox) {
    return checkBox && checkBox->isChecked();
}

} // namespace

class TestableAlarmCatalogWidget : public AlarmCatalogWidget {
    Q_OBJECT
public:
    explicit TestableAlarmCatalogWidget(BackendBridge* bridge,
                                        QWidget* parent = nullptr)
        : AlarmCatalogWidget(bridge, parent) {}
    ~TestableAlarmCatalogWidget() override {}

    void enqueueDecision(DirtyDecision decision) { decisions_.enqueue(decision); }

protected:
    DirtyDecision confirmDirtyChanges() override {
        if (decisions_.isEmpty()) return DirtyDecision::Cancel;
        return decisions_.dequeue();
    }

private:
    QQueue<DirtyDecision> decisions_;
};

class AlarmCatalogWidgetTest : public QObject {
    Q_OBJECT
public:
    explicit AlarmCatalogWidgetTest(QObject* parent = nullptr)
        : QObject(parent) {}
    ~AlarmCatalogWidgetTest() override {}

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void showsSplitCatalogAndPhasedActions();
    void preservesEditsAcrossSelectionChanges();
    void appliesStagedConfigurationDiffs();
    void refreshAppliesDirtyChanges();
    void refreshDiscardsDirtyChanges();
    void refreshCancelKeepsStagedStateAndSelection();

private:
    BackendBridge bridge_;
};

void AlarmCatalogWidgetTest::initTestCase() {
    EventMgrModule::init();
}

void AlarmCatalogWidgetTest::init() {
    ConfigManager::instance().clearAll();
    LinkageEngine::instance().clearAll();
    ActionRegistry::setup(LinkageEngine::instance());
    EventManager::instance().setNotifyCallback(EventManager::NotifyCallback());
    LinkageEngine::instance().setFallback(LinkageEngine::FallbackCallback());
}

void AlarmCatalogWidgetTest::cleanup() {
    EventManager::instance().setNotifyCallback(EventManager::NotifyCallback());
    LinkageEngine::instance().setFallback(LinkageEngine::FallbackCallback());
}

void AlarmCatalogWidgetTest::showsSplitCatalogAndPhasedActions() {
    AlarmCatalogWidget widget(&bridge_);
    widget.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(requiredChild<QSplitter>(&widget, "configSplitter"));
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QTableWidget* clear = requiredChild<QTableWidget>(&widget, "clearActionTable");
    QVERIFY(clear);
    QLabel* selected = requiredChild<QLabel>(&widget, "selectedEventLabel");
    QVERIFY(selected);
    QCOMPARE(catalog->columnCount(), 6);

    selectCatalogRow(catalog, kBoilerEvent);
    QVERIFY(selected->text().contains(kBoilerEvent));

    const int stopRow = actionRow(active, "cooler_stop", QString::fromUtf8("关冷却塔"));
    const int fanRow = actionRow(active, "cooler_fan", QString::fromUtf8("调风扇"));
    QVERIFY(stopRow < fanRow);

    bool showsEmptyPhase = false;
    bool hasEnabledSwitch = false;
    for (int row = 0; row < clear->rowCount(); ++row) {
        for (int column = 0; column < clear->columnCount(); ++column) {
            QTableWidgetItem* item = clear->item(row, column);
            if (item && item->text() == QString::fromUtf8("无联动")) showsEmptyPhase = true;
            QWidget* cell = clear->cellWidget(row, column);
            QCheckBox* checkBox = qobject_cast<QCheckBox*>(cell);
            if (!checkBox && cell) checkBox = cell->findChild<QCheckBox*>();
            if (checkBox && checkBox->isEnabled()) hasEnabledSwitch = true;
        }
    }
    QVERIFY2(showsEmptyPhase, "The empty clear phase must be represented as '无联动'");
    QVERIFY2(!hasEnabledSwitch, "The empty clear phase must not expose an enabled switch");

    QTableWidgetItem* linkageCount = catalog->item(catalogRow(catalog, kBoilerEvent), 5);
    QVERIFY2(linkageCount, "The catalog linkage-count cell is missing");
    const BackendBridge::EventActionGroups groups = bridge_.getEventActionGroups(
        kBoilerEvent, static_cast<int>(EventLevel::Emergency));
    QCOMPARE(linkageCount->text().toInt(),
             groups.activeActions.size() + groups.clearActions.size());
    QCOMPARE(groups.activeActions.size() + groups.clearActions.size(), 2);
}

void AlarmCatalogWidgetTest::preservesEditsAcrossSelectionChanges() {
    AlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QVERIFY(status);

    selectCatalogRow(catalog, kBoilerEvent);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, boilerRow, 3), true);
    toggle(tableCheckBox(catalog, boilerRow, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);

    selectCatalogRow(catalog, kOtherEvent);
    selectCatalogRow(catalog, kBoilerEvent);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 3)), true);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             false);

    QVERIFY(!ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(!ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QVERIFY(apply->isEnabled());
    QVERIFY2(status->text().contains(QString::fromUtf8("未应用")),
             "Dirty status must indicate that edits are not yet applied");
}

void AlarmCatalogWidgetTest::appliesStagedConfigurationDiffs() {
    AlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QVERIFY(status);

    selectCatalogRow(catalog, kBoilerEvent);
    const int row = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, row, 3), true);
    toggle(tableCheckBox(catalog, row, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);
    apply->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QCOMPARE(static_cast<int>(ConfigManager::instance().getEffectiveLevel(
                 kBoilerEvent.toStdString(), EventLevel::Emergency)),
             static_cast<int>(EventLevel::Info));
    QVERIFY(ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));

    const BackendBridge::EventActionGroups boilerGroups =
        bridge_.getEventActionGroups(kBoilerEvent, static_cast<int>(EventLevel::Emergency));
    QCOMPARE(phaseActionEnabled(boilerGroups, "cooler_fan", true), false);
    QCOMPARE(phaseActionEnabled(boilerGroups, "cooler_stop", true), true);
    QCOMPARE(boilerGroups.clearActions.size(), 0);

    const BackendBridge::EventActionGroups otherGroups =
        bridge_.getEventActionGroups(QString::fromUtf8("锅炉-5-overload"),
                                     static_cast<int>(EventLevel::Emergency));
    QCOMPARE(phaseActionEnabled(otherGroups, "cooler_stop", true), true);
    QCOMPARE(otherGroups.clearActions.size(), 0);
    QVERIFY(!ConfigManager::instance().hasDowngrade("锅炉-5-overload"));
    QVERIFY(!ConfigManager::instance().isShielded("锅炉-5-overload"));
    QVERIFY(!apply->isEnabled());
    QCOMPARE(status->text(), QString::fromUtf8("配置已应用"));
}

void AlarmCatalogWidgetTest::refreshAppliesDirtyChanges() {
    TestableAlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    selectCatalogRow(catalog, kBoilerEvent);
    const int row = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, row, 3), true);
    toggle(tableCheckBox(catalog, row, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);

    widget.enqueueDecision(AlarmCatalogWidget::DirtyDecision::Apply);
    widget.requestReload();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 3)), true);
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             false);
}

void AlarmCatalogWidgetTest::refreshDiscardsDirtyChanges() {
    ConfigManager::instance().setShield(kOtherEvent.toStdString());
    TestableAlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    selectCatalogRow(catalog, kBoilerEvent);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, boilerRow, 3), true);
    toggle(tableCheckBox(catalog, boilerRow, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);

    widget.enqueueDecision(AlarmCatalogWidget::DirtyDecision::Discard);
    widget.requestReload();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(!ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(!ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(ConfigManager::instance().isShielded(kOtherEvent.toStdString()));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 3)), false);
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 4)), false);
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kOtherEvent), 4)), true);
    selectCatalogRow(catalog, kBoilerEvent);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             true);
}

void AlarmCatalogWidgetTest::refreshCancelKeepsStagedStateAndSelection() {
    TestableAlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QLabel* selected = requiredChild<QLabel>(&widget, "selectedEventLabel");
    QVERIFY(selected);
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QVERIFY(status);
    selectCatalogRow(catalog, kBoilerEvent);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, boilerRow, 3), true);
    toggle(tableCheckBox(catalog, boilerRow, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);
    const QString statusBefore = status->text();

    widget.enqueueDecision(AlarmCatalogWidget::DirtyDecision::Cancel);
    widget.requestReload();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QCOMPARE(selected->text(), kBoilerEvent);
    QCOMPARE(catalog->currentRow(), boilerRow);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 3)), true);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             false);
    QVERIFY(!ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(!ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QCOMPARE(status->text(), statusBefore);
}

QTEST_MAIN(AlarmCatalogWidgetTest)
#include "test_alarm_catalog_widget.moc"
