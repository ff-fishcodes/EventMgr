#include <QtTest>
#include <QtMath>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QPixmap>
#include <QPushButton>
#include <QQueue>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTabWidget>

#include "action_registry.h"
#include "alarm_catalog_widget.h"
#include "backend_bridge.h"
#include "config_manager.h"
#include "event_manager.h"
#include "event_mgr_module.h"
#include "eventmgr_widget.h"
#include "linkage_engine.h"

namespace {

const QString kBoilerEvent = QString::fromUtf8("锅炉-3-temp_high");
const QString kOtherEvent = QString::fromUtf8("锅炉-3-pressure_low");
const QString kUntouchedEvent = QString::fromUtf8("锅炉-5-overload");

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

int actionRowByInternalName(QTableWidget* table, const QString& actionName) {
    for (int row = 0; row < table->rowCount(); ++row) {
        QWidget* cell = table->cellWidget(row, 0);
        if (cell && cell->property("actionName").toString() == actionName) {
            return row;
        }
    }
    QTest::qFail(qPrintable(QString::fromLatin1("Internal action '%1' was not found")
                               .arg(actionName)),
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

bool containsSourceMetadata(const QString& text) {
    return text.contains(QString::fromUtf8("默认")) ||
           text.contains(QString::fromUtf8("专属"));
}

bool actionTableExposesSourceMetadata(QTableWidget* table) {
    for (int column = 0; column < table->columnCount(); ++column) {
        QTableWidgetItem* header = table->horizontalHeaderItem(column);
        if (header && containsSourceMetadata(header->text())) return true;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        for (int column = 0; column < table->columnCount(); ++column) {
            QTableWidgetItem* item = table->item(row, column);
            if (item && containsSourceMetadata(item->text())) return true;
            QWidget* cell = table->cellWidget(row, column);
            if (!cell) continue;
            QLabel* directLabel = qobject_cast<QLabel*>(cell);
            if (directLabel && directLabel->isVisible() &&
                containsSourceMetadata(directLabel->text())) return true;
            const QList<QLabel*> labels = cell->findChildren<QLabel*>();
            for (int i = 0; i < labels.size(); ++i) {
                if (labels[i]->isVisible() &&
                    containsSourceMetadata(labels[i]->text())) return true;
            }
        }
    }
    return false;
}

bool actionIdentityIsPainted(QTableWidget* table, int row,
                             const QString& actionName,
                             const QString& displayName) {
    QTableWidgetItem* item = table->item(row, 0);
    if (item && item->text().contains(actionName) &&
        item->text().contains(displayName)) return true;

    QWidget* cell = table->cellWidget(row, 0);
    if (!cell) return false;
    const QList<QLabel*> labels = cell->findChildren<QLabel*>();
    QString visibleText;
    for (int i = 0; i < labels.size(); ++i) {
        if (labels[i]->isVisible()) {
            visibleText += labels[i]->property("displayText").toString();
        }
    }
    return visibleText.contains(actionName) && visibleText.contains(displayName);
}

QRect widgetRectIn(const QWidget* child, const QWidget* ancestor) {
    return QRect(child->mapTo(ancestor, QPoint(0, 0)), child->size());
}

void processPendingLayouts(QWidget* widget) {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::PolishRequest);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    if (widget->layout()) widget->layout()->activate();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

void verifyContainedAndVisible(QWidget* child, QWidget* container,
                               const char* description) {
    QVERIFY2(child->isVisible(), description);
    const QRect childRect = widgetRectIn(child, container);
    QVERIFY2(container->rect().contains(childRect),
             qPrintable(QString::fromLatin1("%1 is outside the widget: %2,%3 %4x%5")
                 .arg(QString::fromLatin1(description))
                 .arg(childRect.x()).arg(childRect.y())
                 .arg(childRect.width()).arg(childRect.height())));
}

void verifyButtonTextFits(QPushButton* button) {
    const int textWidth = QFontMetrics(button->font())
        .boundingRect(button->text()).width();
    QVERIFY2(textWidth <= button->contentsRect().width(),
             qPrintable(QString::fromLatin1("Button text '%1' needs %2px but has %3px")
                 .arg(button->text()).arg(textWidth)
                 .arg(button->contentsRect().width())));
}

void verifyVisibleActionCellsStayInViewport(QTableWidget* table) {
    const QRect viewportRect = table->viewport()->rect();
    for (int row = 0; row < table->rowCount(); ++row) {
        for (int column = 0; column < table->columnCount(); ++column) {
            QWidget* cell = table->cellWidget(row, column);
            if (!cell || !cell->isVisible()) continue;
            const QRect cellRect = widgetRectIn(cell, table->viewport());
            if (!cellRect.intersects(viewportRect)) continue;
            QVERIFY2(cellRect.left() >= viewportRect.left() &&
                         cellRect.right() <= viewportRect.right(),
                     qPrintable(QString::fromLatin1(
                         "Action cell %1,%2 is outside viewport: %3,%4 %5x%6 vs %7x%8")
                         .arg(row).arg(column).arg(cellRect.x()).arg(cellRect.y())
                         .arg(cellRect.width()).arg(cellRect.height())
                         .arg(viewportRect.width()).arg(viewportRect.height())));
        }
    }
}

// Validates the width-dependent paint contract without assuming a particular
// font metric or desktop overflow outcome.
void verifyActionTextRendering(QTableWidget* table,
                               bool requireAtLeastOneOverflow) {
    bool foundOverflow = false;
    for (int row = 0; row < table->rowCount(); ++row) {
        QWidget* cell = table->cellWidget(row, 0);
        if (!cell) continue;
        const QList<QLabel*> labels = cell->findChildren<QLabel*>();
        for (int i = 0; i < labels.size(); ++i) {
            QLabel* label = labels[i];
            const QString fullText = label->text();
            const QString displayText = label->property("displayText").toString();
            const QFontMetrics metrics(label->font());
            const bool overflows = metrics.horizontalAdvance(fullText) >
                label->contentsRect().width();

            QVERIFY2(!displayText.isEmpty(), "Action label lacks rendered text contract");
            QVERIFY(metrics.horizontalAdvance(displayText) <=
                    label->contentsRect().width());
            QCOMPARE(label->toolTip(), fullText);
            QCOMPARE(label->accessibleName(), fullText);
            QVERIFY(label->accessibleDescription().contains(fullText));

            if (overflows) {
                foundOverflow = true;
                QVERIFY2(!displayText.isEmpty(),
                         "Overflowing action label has no rendered text");
                QVERIFY2(metrics.horizontalAdvance(displayText) <=
                             label->contentsRect().width(),
                         "Elided action text still exceeds its contents rect");
                QVERIFY(displayText != fullText);
                QVERIFY2(displayText.contains(QChar(0x2026)) ||
                             displayText.contains(QString::fromLatin1("...")),
                         "Overflowing action text must use explicit right elision");
            } else {
                QCOMPARE(displayText, fullText);
            }
        }
    }
    if (requireAtLeastOneOverflow) {
        QVERIFY2(foundOverflow,
                 "Narrow active-action fixture must exercise text elision");
    }
}

// Finds the label that paints an action's backend key. The lookup deliberately
// combines the cell identity with the accessible role instead of label order.
QLabel* internalNameLabel(QTableWidget* table, const QString& actionName) {
    const int row = actionRowByInternalName(table, actionName);
    QWidget* cell = table->cellWidget(row, 0);
    if (!cell || cell->property("actionName").toString() != actionName) {
        QTest::qFail("Action identity cell is missing", __FILE__, __LINE__);
        return nullptr;
    }

    const QString internalRole = QString::fromUtf8("动作内部名称");
    const QList<QLabel*> labels = cell->findChildren<QLabel*>();
    for (int i = 0; i < labels.size(); ++i) {
        QLabel* label = labels[i];
        if (label->accessibleName() == actionName &&
            label->accessibleDescription().contains(internalRole)) {
            return label;
        }
    }
    QTest::qFail(qPrintable(QString::fromLatin1(
        "Internal-name painted label for '%1' was not found").arg(actionName)),
        __FILE__, __LINE__);
    return nullptr;
}

QRect logicalRectToPixels(const QRect& logicalRect, qreal devicePixelRatio) {
    const int left = qFloor(logicalRect.left() * devicePixelRatio);
    const int top = qFloor(logicalRect.top() * devicePixelRatio);
    const int right = qCeil((logicalRect.right() + 1) * devicePixelRatio) - 1;
    const int bottom = qCeil((logicalRect.bottom() + 1) * devicePixelRatio) - 1;
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

void verifyRegionHasRenderedContent(const QImage& image,
                                    const QRect& logicalRect,
                                    qreal devicePixelRatio,
                                    const char* description) {
    const QRect pixelRect = logicalRectToPixels(logicalRect, devicePixelRatio)
        .intersected(image.rect());
    QVERIFY2(!pixelRect.isEmpty(), description);

    QHash<QRgb, int> frequencies;
    int sampleCount = 0;
    int darkest = 255;
    int lightest = 0;
    const int sampleStep = qMax(1, qRound(devicePixelRatio * 2.0));
    for (int y = pixelRect.top(); y <= pixelRect.bottom(); y += sampleStep) {
        for (int x = pixelRect.left(); x <= pixelRect.right(); x += sampleStep) {
            const QRgb pixel = image.pixel(x, y);
            ++frequencies[pixel];
            ++sampleCount;
            const int luminance = qGray(pixel);
            darkest = qMin(darkest, luminance);
            lightest = qMax(lightest, luminance);
        }
    }

    int dominantCount = 0;
    for (QHash<QRgb, int>::const_iterator it = frequencies.constBegin();
         it != frequencies.constEnd(); ++it) {
        dominantCount = qMax(dominantCount, it.value());
    }
    const int foregroundSamples = sampleCount - dominantCount;
    QVERIFY2(frequencies.size() >= 4,
             qPrintable(QString::fromLatin1("%1 has insufficient pixel variation")
                 .arg(QString::fromLatin1(description))));
    QVERIFY2(lightest - darkest >= 20,
             qPrintable(QString::fromLatin1("%1 lacks foreground contrast")
                 .arg(QString::fromLatin1(description))));
    QVERIFY2(foregroundSamples >= qMax(6, sampleCount / 1000),
             qPrintable(QString::fromLatin1("%1 appears blank")
                 .arg(QString::fromLatin1(description))));
}

// Samples high-frequency luminance changes inside an expected text region.
// This tolerates palette and antialiasing differences while a blank paint path
// still fails even when the surrounding widget itself has borders or shading.
void verifyRegionHasTextInk(const QImage& image,
                            const QRect& logicalRect,
                            qreal devicePixelRatio,
                            const char* description) {
    const QRect pixelRect = logicalRectToPixels(logicalRect, devicePixelRatio)
        .intersected(image.rect());
    QVERIFY2(!pixelRect.isEmpty(), description);

    int darkest = 255;
    int lightest = 0;
    int edgeTransitions = 0;
    const int edgeThreshold = 10;
    for (int y = pixelRect.top(); y <= pixelRect.bottom(); ++y) {
        for (int x = pixelRect.left(); x <= pixelRect.right(); ++x) {
            const int luminance = qGray(image.pixel(x, y));
            darkest = qMin(darkest, luminance);
            lightest = qMax(lightest, luminance);
            if (x > pixelRect.left() &&
                qAbs(luminance - qGray(image.pixel(x - 1, y))) >= edgeThreshold) {
                ++edgeTransitions;
            }
            if (y > pixelRect.top() &&
                qAbs(luminance - qGray(image.pixel(x, y - 1))) >= edgeThreshold) {
                ++edgeTransitions;
            }
        }
    }

    QVERIFY2(lightest - darkest >= 16,
             qPrintable(QString::fromLatin1("%1 lacks text contrast")
                 .arg(QString::fromLatin1(description))));
    QVERIFY2(edgeTransitions >= 6,
             qPrintable(QString::fromLatin1("%1 has no painted text edges")
                 .arg(QString::fromLatin1(description))));
}

QRect mappedContentsRect(const QWidget* child, const QWidget* ancestor) {
    const QRect contents = child->contentsRect();
    return QRect(child->mapTo(ancestor, contents.topLeft()), contents.size());
}

// Returns the expected glyph area for ordinary QLabel/QPushButton text. It is
// intentionally expanded slightly for style-dependent font bearings.
QRect mappedTextRect(const QWidget* widget, const QString& text,
                     Qt::Alignment alignment, const QWidget* ancestor) {
    const QFontMetrics metrics(widget->font());
    const QSize textSize(metrics.horizontalAdvance(text), metrics.height());
    QRect textRect = QStyle::alignedRect(widget->layoutDirection(), alignment,
                                         textSize, widget->contentsRect());
    textRect.adjust(-2, -2, 2, 2);
    textRect = textRect.intersected(widget->contentsRect());
    return QRect(widget->mapTo(ancestor, textRect.topLeft()), textRect.size());
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

class ReentrantAlarmCatalogWidget : public AlarmCatalogWidget {
    Q_OBJECT
public:
    explicit ReentrantAlarmCatalogWidget(BackendBridge* bridge,
                                         QWidget* parent = nullptr)
        : AlarmCatalogWidget(bridge, parent), promptCount_(0), reentered_(false) {}
    ~ReentrantAlarmCatalogWidget() override {}

    int promptCount() const { return promptCount_; }

protected:
    DirtyDecision confirmDirtyChanges() override {
        ++promptCount_;
        if (!reentered_) {
            reentered_ = true;
            requestReload();
        }
        return DirtyDecision::Cancel;
    }

private:
    int promptCount_;
    bool reentered_;
};

class TestableEventMgrWidget : public EventMgrWidget {
    Q_OBJECT
public:
    explicit TestableEventMgrWidget(QWidget* parent = nullptr)
        : EventMgrWidget(parent), guardCallCount_(0) {}
    ~TestableEventMgrWidget() override {}

    void enqueueLeaveResult(bool result) { leaveResults_.enqueue(result); }
    int guardCallCount() const { return guardCallCount_; }

protected:
    bool canLeaveCatalogPage() override {
        ++guardCallCount_;
        if (leaveResults_.isEmpty()) return true;
        return leaveResults_.dequeue();
    }

private:
    QQueue<bool> leaveResults_;
    int guardCallCount_;
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
    void cleanRequestLeaveKeepsVisibleSnapshot();
    void requestLeaveAppliesDirtyChanges();
    void requestLeaveDiscardsDirtyChanges();
    void requestLeaveCancelKeepsStagedState();
    void applySkipsActionRephasedAfterLoad();
    void applyPreservesUntouchedActionChangedAfterLoad();
    void applyKeepsSameActionPhasesIndependent();
    void reentrantReloadDoesNotPromptTwice();
    void eventMgrCleanTabSwitchTracksCurrentIndex();
    void eventMgrTabGuardRestoresThenAllowsTransition();
    void capturesResponsiveScreenshots();

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
    QCOMPARE(active->columnCount(), 2);
    QCOMPARE(clear->columnCount(), 2);
    QVERIFY(active->horizontalHeaderItem(0));
    QVERIFY(active->horizontalHeaderItem(1));
    QVERIFY(clear->horizontalHeaderItem(0));
    QVERIFY(clear->horizontalHeaderItem(1));
    QCOMPARE(active->horizontalHeaderItem(0)->text(), QString::fromUtf8("联动动作"));
    QCOMPARE(active->horizontalHeaderItem(1)->text(), QString::fromUtf8("启用"));
    QCOMPARE(clear->horizontalHeaderItem(0)->text(), QString::fromUtf8("联动动作"));
    QCOMPARE(clear->horizontalHeaderItem(1)->text(), QString::fromUtf8("启用"));

    selectCatalogRow(catalog, kBoilerEvent);
    QVERIFY(selected->text().contains(kBoilerEvent));
    QVERIFY2(!actionTableExposesSourceMetadata(active),
             "The active action table must not expose default/dedicated metadata");
    QVERIFY2(!actionTableExposesSourceMetadata(clear),
             "The clear action table must not expose default/dedicated metadata");

    const int stopRow = actionRow(active, "cooler_stop", QString::fromUtf8("关冷却塔"));
    const int fanRow = actionRow(active, "cooler_fan", QString::fromUtf8("调风扇"));
    QVERIFY(stopRow < fanRow);
    QVERIFY2(actionIdentityIsPainted(active, stopRow, "cooler_stop",
                                     QString::fromUtf8("关冷却塔")),
             "Each action row must visibly show its display and internal names");
    QVERIFY2(actionIdentityIsPainted(active, fanRow, "cooler_fan",
                                     QString::fromUtf8("调风扇")),
             "Each action row must visibly show its display and internal names");

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
    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(), {"cooler_fan"}, {"buzzer_alert"});
    const BackendBridge::EventActionGroups configuredBoilerGroups =
        bridge_.getEventActionGroups(kBoilerEvent, static_cast<int>(EventLevel::Emergency));
    QCOMPARE(phaseActionEnabled(configuredBoilerGroups, "cooler_fan", true), true);
    QCOMPARE(phaseActionEnabled(configuredBoilerGroups, "buzzer_alert", false), true);

    AlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QTableWidget* clear = requiredChild<QTableWidget>(&widget, "clearActionTable");
    QVERIFY(clear);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QVERIFY(status);

    const int untouchedRow = catalogRow(catalog, kUntouchedEvent);
    QCOMPARE(checked(tableCheckBox(catalog, untouchedRow, 4)), false);
    const BackendBridge::EventActionGroups untouchedBefore =
        bridge_.getEventActionGroups(kUntouchedEvent,
                                     static_cast<int>(EventLevel::Emergency));
    QCOMPARE(phaseActionEnabled(untouchedBefore, "cooler_stop", true), true);
    ConfigManager::instance().setShield(kUntouchedEvent.toStdString());
    ConfigManager::instance().setDowngrade(
        kUntouchedEvent.toStdString(), EventLevel::Info);
    LinkageEngine::instance().disableAction(
        kUntouchedEvent.toStdString(), "cooler_stop", true);
    QVERIFY(ConfigManager::instance().isShielded(kUntouchedEvent.toStdString()));
    QVERIFY(ConfigManager::instance().hasDowngrade(kUntouchedEvent.toStdString()));
    QCOMPARE(static_cast<int>(ConfigManager::instance().getEffectiveLevel(
                 kUntouchedEvent.toStdString(), EventLevel::Emergency)),
             static_cast<int>(EventLevel::Info));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kUntouchedEvent.toStdString(), "cooler_stop", true));

    selectCatalogRow(catalog, kBoilerEvent);
    const int row = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, row, 3), true);
    toggle(tableCheckBox(catalog, row, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);
    toggle(tableCheckBox(clear,
                         actionRow(clear, "buzzer_alert", QString::fromUtf8("蜂鸣器")), 1),
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
    QCOMPARE(phaseActionEnabled(boilerGroups, "buzzer_alert", false), false);
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "buzzer_alert", false));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "buzzer_alert", true));

    const BackendBridge::EventActionGroups otherGroups =
        bridge_.getEventActionGroups(kUntouchedEvent,
                                     static_cast<int>(EventLevel::Emergency));
    QCOMPARE(phaseActionEnabled(otherGroups, "cooler_stop", true), false);
    QCOMPARE(otherGroups.clearActions.size(), 0);
    QVERIFY(ConfigManager::instance().hasDowngrade(kUntouchedEvent.toStdString()));
    QCOMPARE(static_cast<int>(ConfigManager::instance().getEffectiveLevel(
                 kUntouchedEvent.toStdString(), EventLevel::Emergency)),
             static_cast<int>(EventLevel::Info));
    QVERIFY(ConfigManager::instance().isShielded(kUntouchedEvent.toStdString()));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kUntouchedEvent.toStdString(), "cooler_stop", true));
    QVERIFY(!apply->isEnabled());
    QCOMPARE(status->text(), QString::fromUtf8("配置已应用"));
}

void AlarmCatalogWidgetTest::refreshAppliesDirtyChanges() {
    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(), {"cooler_fan"}, {"buzzer_alert"});
    TestableAlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QTableWidget* clear = requiredChild<QTableWidget>(&widget, "clearActionTable");
    QVERIFY(clear);
    QLabel* selected = requiredChild<QLabel>(&widget, "selectedEventLabel");
    QVERIFY(selected);
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QVERIFY(status);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    const int row = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, row, 3), true);
    toggle(tableCheckBox(catalog, row, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);
    toggle(tableCheckBox(clear,
                         actionRow(clear, "buzzer_alert", QString::fromUtf8("蜂鸣器")), 1),
           false);

    widget.enqueueDecision(AlarmCatalogWidget::DirtyDecision::Apply);
    widget.requestReload();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "buzzer_alert", false));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "buzzer_alert", true));
    const int reloadedRow = catalogRow(catalog, kBoilerEvent);
    QCOMPARE(catalog->currentRow(), reloadedRow);
    QCOMPARE(catalog->item(reloadedRow, 0)->text(), kBoilerEvent);
    QVERIFY(selected->text().contains(kBoilerEvent));
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 3)), true);
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             false);
    QCOMPARE(checked(tableCheckBox(
                 clear,
                 actionRow(clear, "buzzer_alert", QString::fromUtf8("蜂鸣器")), 1)),
             false);
    QVERIFY(!apply->isEnabled());
    QCOMPARE(status->text(), QString::fromUtf8("配置已应用"));
}

void AlarmCatalogWidgetTest::refreshDiscardsDirtyChanges() {
    ConfigManager::instance().setShield(kOtherEvent.toStdString());
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
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
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
    const int reloadedRow = catalogRow(catalog, kBoilerEvent);
    QCOMPARE(catalog->currentRow(), reloadedRow);
    QCOMPARE(catalog->item(reloadedRow, 0)->text(), kBoilerEvent);
    QVERIFY(selected->text().contains(kBoilerEvent));
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 3)), false);
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kBoilerEvent), 4)), false);
    QCOMPARE(checked(tableCheckBox(
                 catalog, catalogRow(catalog, kOtherEvent), 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             true);
    QVERIFY(!apply->isEnabled());
    const int catalogSize = bridge_.getCatalog().size();
    QCOMPARE(catalog->rowCount(), catalogSize);
    QCOMPARE(status->text(),
             QString::fromUtf8("共 %1 个报警定义，当前屏蔽 %2 个")
                 .arg(catalogSize)
                 .arg(bridge_.shieldCount()));
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
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
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
    QVERIFY(apply->isEnabled());
    QCOMPARE(status->text(), statusBefore);
}

void AlarmCatalogWidgetTest::cleanRequestLeaveKeepsVisibleSnapshot() {
    TestableAlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QLabel* selected = requiredChild<QLabel>(&widget, "selectedEventLabel");
    QVERIFY(selected);
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QVERIFY(status);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    const QString statusBefore = status->text();

    ConfigManager::instance().setShield(kBoilerEvent.toStdString());
    QVERIFY(widget.requestLeave());

    QVERIFY(ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QCOMPARE(selected->text(), kBoilerEvent);
    QCOMPARE(catalog->currentRow(), boilerRow);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 4)), false);
    QVERIFY(!apply->isEnabled());
    QCOMPARE(status->text(), statusBefore);
}

void AlarmCatalogWidgetTest::requestLeaveAppliesDirtyChanges() {
    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(), {"cooler_fan"}, {"buzzer_alert"});
    TestableAlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QTableWidget* clear = requiredChild<QTableWidget>(&widget, "clearActionTable");
    QVERIFY(clear);
    QLabel* selected = requiredChild<QLabel>(&widget, "selectedEventLabel");
    QVERIFY(selected);
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QVERIFY(status);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, boilerRow, 3), true);
    toggle(tableCheckBox(catalog, boilerRow, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);
    toggle(tableCheckBox(clear,
                         actionRow(clear, "buzzer_alert", QString::fromUtf8("蜂鸣器")), 1),
           false);

    widget.enqueueDecision(AlarmCatalogWidget::DirtyDecision::Apply);
    QVERIFY(widget.requestLeave());
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "buzzer_alert", false));
    QCOMPARE(selected->text(), kBoilerEvent);
    QCOMPARE(catalog->currentRow(), catalogRow(catalog, kBoilerEvent));
    QCOMPARE(checked(tableCheckBox(catalog, catalog->currentRow(), 3)), true);
    QCOMPARE(checked(tableCheckBox(catalog, catalog->currentRow(), 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             false);
    QCOMPARE(checked(tableCheckBox(
                 clear,
                 actionRow(clear, "buzzer_alert", QString::fromUtf8("蜂鸣器")), 1)),
             false);
    QVERIFY(!apply->isEnabled());
    QCOMPARE(status->text(), QString::fromUtf8("配置已应用"));
}

void AlarmCatalogWidgetTest::requestLeaveDiscardsDirtyChanges() {
    ConfigManager::instance().setShield(kOtherEvent.toStdString());
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
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, boilerRow, 3), true);
    toggle(tableCheckBox(catalog, boilerRow, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);

    widget.enqueueDecision(AlarmCatalogWidget::DirtyDecision::Discard);
    QVERIFY(widget.requestLeave());
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(!ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(!ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(ConfigManager::instance().isShielded(kOtherEvent.toStdString()));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QCOMPARE(selected->text(), kBoilerEvent);
    QCOMPARE(catalog->currentRow(), catalogRow(catalog, kBoilerEvent));
    QCOMPARE(checked(tableCheckBox(catalog, catalog->currentRow(), 3)), false);
    QCOMPARE(checked(tableCheckBox(catalog, catalog->currentRow(), 4)), false);
    QCOMPARE(checked(tableCheckBox(catalog, catalogRow(catalog, kOtherEvent), 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             true);
    QVERIFY(!apply->isEnabled());
    QCOMPARE(status->text(),
             QString::fromUtf8("共 %1 个报警定义，当前屏蔽 %2 个")
                 .arg(catalog->rowCount())
                 .arg(bridge_.shieldCount()));
}

void AlarmCatalogWidgetTest::requestLeaveCancelKeepsStagedState() {
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
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, boilerRow, 3), true);
    toggle(tableCheckBox(catalog, boilerRow, 4), true);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);
    const QString statusBefore = status->text();

    widget.enqueueDecision(AlarmCatalogWidget::DirtyDecision::Cancel);
    QVERIFY(!widget.requestLeave());
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(!ConfigManager::instance().hasDowngrade(kBoilerEvent.toStdString()));
    QVERIFY(!ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QCOMPARE(selected->text(), kBoilerEvent);
    QCOMPARE(catalog->currentRow(), boilerRow);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 3)), true);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 4)), true);
    QCOMPARE(checked(tableCheckBox(
                 active,
                 actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1)),
             false);
    QVERIFY(apply->isEnabled());
    QCOMPARE(status->text(), statusBefore);
}

void AlarmCatalogWidgetTest::applySkipsActionRephasedAfterLoad() {
    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(), {}, {"buzzer_alert"});
    AlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* clear = requiredChild<QTableWidget>(&widget, "clearActionTable");
    QVERIFY(clear);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(clear,
                         actionRow(clear, "buzzer_alert", QString::fromUtf8("蜂鸣器")), 1),
           false);

    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(), {"buzzer_alert"}, {});
    apply->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    const BackendBridge::EventActionGroups groups =
        bridge_.getEventActionGroups(kBoilerEvent,
                                     static_cast<int>(EventLevel::Emergency));
    QCOMPARE(phaseActionEnabled(groups, "buzzer_alert", true), true);
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "buzzer_alert", true));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "buzzer_alert", false));
}

void AlarmCatalogWidgetTest::applyPreservesUntouchedActionChangedAfterLoad() {
    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(), {"cooler_fan", "cooler_stop"}, {});
    AlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);

    LinkageEngine::instance().disableAction(
        kBoilerEvent.toStdString(), "cooler_stop", true);
    apply->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_stop", true));
}

void AlarmCatalogWidgetTest::applyKeepsSameActionPhasesIndependent() {
    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(), {"cooler_fan"}, {"cooler_fan"});
    AlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QVERIFY(active);
    QTableWidget* clear = requiredChild<QTableWidget>(&widget, "clearActionTable");
    QVERIFY(clear);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    selectCatalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(active,
                         actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")), 1),
           false);
    apply->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    const BackendBridge::EventActionGroups groups =
        bridge_.getEventActionGroups(kBoilerEvent,
                                     static_cast<int>(EventLevel::Emergency));
    QCOMPARE(phaseActionEnabled(groups, "cooler_fan", true), false);
    QCOMPARE(phaseActionEnabled(groups, "cooler_fan", false), true);
    QVERIFY(LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", true));
    QVERIFY(!LinkageEngine::instance().isActionDisabled(
        kBoilerEvent.toStdString(), "cooler_fan", false));
}

void AlarmCatalogWidgetTest::reentrantReloadDoesNotPromptTwice() {
    ReentrantAlarmCatalogWidget widget(&bridge_);
    widget.show();
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QVERIFY(catalog);
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QVERIFY(apply);
    const int boilerRow = catalogRow(catalog, kBoilerEvent);
    toggle(tableCheckBox(catalog, boilerRow, 4), true);

    widget.requestReload();

    QCOMPARE(widget.promptCount(), 1);
    QCOMPARE(checked(tableCheckBox(catalog, boilerRow, 4)), true);
    QVERIFY(apply->isEnabled());
    QVERIFY(!ConfigManager::instance().isShielded(kBoilerEvent.toStdString()));
}

void AlarmCatalogWidgetTest::eventMgrCleanTabSwitchTracksCurrentIndex() {
    EventMgrWidget widget;
    widget.show();
    QTabWidget* tabs = widget.findChild<QTabWidget*>();
    QVERIFY(tabs);
    AlarmCatalogWidget* catalog = widget.findChild<AlarmCatalogWidget*>();
    QVERIFY(catalog);
    QCOMPARE(tabs->currentIndex(), 0);
    QSignalSpy changedSpy(tabs, SIGNAL(currentChanged(int)));

    tabs->setCurrentIndex(1);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(changedSpy.count(), 1);

    tabs->setCurrentIndex(0);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(tabs->currentIndex(), 0);
    QCOMPARE(changedSpy.count(), 2);
}

void AlarmCatalogWidgetTest::eventMgrTabGuardRestoresThenAllowsTransition() {
    TestableEventMgrWidget widget;
    widget.show();
    QTabWidget* tabs = widget.findChild<QTabWidget*>();
    QVERIFY(tabs);
    tabs->setCurrentIndex(1);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(tabs->currentIndex(), 1);

    widget.enqueueLeaveResult(false);
    widget.enqueueLeaveResult(true);
    QSignalSpy changedSpy(tabs, SIGNAL(currentChanged(int)));
    tabs->setCurrentIndex(0);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(widget.guardCallCount(), 1);
    QCOMPARE(changedSpy.count(), 1);

    tabs->setCurrentIndex(0);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(tabs->currentIndex(), 0);
    QCOMPARE(widget.guardCallCount(), 2);
    QCOMPARE(changedSpy.count(), 2);
}

void AlarmCatalogWidgetTest::capturesResponsiveScreenshots() {
    const std::string longInternalName =
        "boiler_emergency_cooling_override_with_confirmed_remote_interlock";
    const QString longDisplayName = QString::fromUtf8(
        "锅炉紧急冷却联锁远程确认与运行状态复核动作");
    const QString duplicateDisplayName = QString::fromUtf8("现场联锁复位");

    LinkageEngine::instance().registerAction(
        longInternalName, longDisplayName.toStdString(), []() {});
    LinkageEngine::instance().registerAction(
        "boiler_reset_primary", duplicateDisplayName.toStdString(), []() {});
    LinkageEngine::instance().registerAction(
        "boiler_reset_secondary", duplicateDisplayName.toStdString(), []() {});
    LinkageEngine::instance().configureEvent(
        kBoilerEvent.toStdString(),
        {"boiler_reset_primary", "boiler_reset_secondary", longInternalName,
         "cooler_fan"},
        {"buzzer_alert"});

    AlarmCatalogWidget widget(&bridge_);
    QTableWidget* catalog = requiredChild<QTableWidget>(&widget, "catalogTable");
    QTableWidget* active = requiredChild<QTableWidget>(&widget, "activeActionTable");
    QTableWidget* clear = requiredChild<QTableWidget>(&widget, "clearActionTable");
    QLabel* selected = requiredChild<QLabel>(&widget, "selectedEventLabel");
    QLabel* activeTitle = requiredChild<QLabel>(&widget, "activeActionTitleLabel");
    QLabel* clearTitle = requiredChild<QLabel>(&widget, "clearActionTitleLabel");
    QLabel* status = requiredChild<QLabel>(&widget, "statusLabel");
    QPushButton* refresh = requiredChild<QPushButton>(&widget, "refreshBtn");
    QPushButton* apply = requiredChild<QPushButton>(&widget, "applyBtn");
    QSplitter* splitter = requiredChild<QSplitter>(&widget, "configSplitter");

    widget.resize(1280, 760);
    widget.show();
    processPendingLayouts(&widget);
    selectCatalogRow(catalog, kBoilerEvent);
    processPendingLayouts(&widget);
    QCOMPARE(selected->text(), kBoilerEvent);
    const int primaryRow = actionRowByInternalName(
        active, QString::fromLatin1("boiler_reset_primary"));
    const int secondaryRow = actionRowByInternalName(
        active, QString::fromLatin1("boiler_reset_secondary"));
    QVERIFY(primaryRow != secondaryRow);
    QVERIFY2(actionIdentityIsPainted(active, primaryRow,
                                     "boiler_reset_primary", duplicateDisplayName),
             "Duplicate display names need visible active-phase internal names");
    QVERIFY2(actionIdentityIsPainted(active, secondaryRow,
                                     "boiler_reset_secondary", duplicateDisplayName),
             "Duplicate display names need visible active-phase internal names");
    const int longActionRow = actionRowByInternalName(
        active, QString::fromStdString(longInternalName));
    QVERIFY(longActionRow >= 0);
    QVERIFY(actionRow(active, "cooler_stop", QString::fromUtf8("关冷却塔")) >= 0);
    QVERIFY(actionRow(active, "cooler_fan", QString::fromUtf8("调风扇")) >= 0);
    QVERIFY(actionRow(clear, "buzzer_alert", QString::fromUtf8("蜂鸣器")) >= 0);

    struct CaptureSize {
        int width;
        int height;
        const char* fileName;
    };
    const CaptureSize sizes[] = {
        {1280, 760, "catalog-desktop.png"},
        {760, 720, "catalog-narrow.png"}
    };
    const QByteArray screenshotDir = qgetenv("EVENTMGR_SCREENSHOT_DIR");
    if (!screenshotDir.isEmpty()) {
        QVERIFY2(QDir().mkpath(QString::fromLocal8Bit(screenshotDir)),
                 "Could not create screenshot directory");
    }

    for (const CaptureSize& size : sizes) {
        widget.resize(size.width, size.height);
        widget.show();
        active->scrollToItem(active->item(longActionRow, 0),
                             QAbstractItemView::PositionAtBottom);
        processPendingLayouts(&widget);

        QCOMPARE(widget.size(), QSize(size.width, size.height));
        QCOMPARE(selected->text(), kBoilerEvent);
        QCOMPARE(activeTitle->text(), QString::fromUtf8("事件产生时"));
        QCOMPARE(clearTitle->text(), QString::fromUtf8("事件消除时"));
        QVERIFY(!status->text().isEmpty());
        QCOMPARE(refresh->text(), QString::fromUtf8("刷新"));
        QCOMPARE(apply->text(), QString::fromUtf8("应用配置"));
        QVERIFY(actionIdentityIsPainted(active, primaryRow,
                                        "boiler_reset_primary",
                                        duplicateDisplayName));
        QVERIFY(actionIdentityIsPainted(active, secondaryRow,
                                        "boiler_reset_secondary",
                                        duplicateDisplayName));
        verifyContainedAndVisible(splitter, &widget, "configuration splitter");
        verifyContainedAndVisible(activeTitle, &widget, "active phase title");
        verifyContainedAndVisible(active, &widget, "active phase table");
        verifyContainedAndVisible(clearTitle, &widget, "clear phase title");
        verifyContainedAndVisible(clear, &widget, "clear phase table");
        verifyContainedAndVisible(selected, &widget, "selected event");
        verifyContainedAndVisible(refresh, &widget, "refresh button");
        verifyContainedAndVisible(apply, &widget, "apply button");
        verifyContainedAndVisible(status, &widget, "status label");

        const QRect activeTitleRect = widgetRectIn(activeTitle, &widget);
        const QRect activeRect = widgetRectIn(active, &widget);
        const QRect clearTitleRect = widgetRectIn(clearTitle, &widget);
        const QRect clearRect = widgetRectIn(clear, &widget);
        const QRect refreshRect = widgetRectIn(refresh, &widget);
        const QRect applyRect = widgetRectIn(apply, &widget);
        const QRect statusRect = widgetRectIn(status, &widget);
        QVERIFY(activeTitleRect.bottom() < activeRect.top());
        QVERIFY(activeRect.bottom() < clearTitleRect.top());
        QVERIFY(clearTitleRect.bottom() < clearRect.top());
        QVERIFY(splitter->geometry().bottom() < refreshRect.top());
        QVERIFY(!refreshRect.intersects(applyRect));
        QVERIFY(refreshRect.bottom() < statusRect.top());
        QVERIFY(applyRect.bottom() < statusRect.top());
        verifyButtonTextFits(refresh);
        verifyButtonTextFits(apply);
        verifyVisibleActionCellsStayInViewport(active);
        verifyVisibleActionCellsStayInViewport(clear);
        verifyActionTextRendering(active, size.width == 760);
        verifyActionTextRendering(clear, false);

        if (size.width == 760) {
            QLabel* primaryInternal = internalNameLabel(
                active, QString::fromLatin1("boiler_reset_primary"));
            QLabel* secondaryInternal = internalNameLabel(
                active, QString::fromLatin1("boiler_reset_secondary"));
            QVERIFY(primaryInternal);
            QVERIFY(secondaryInternal);
            QCOMPARE(primaryInternal->accessibleName(),
                     QString::fromLatin1("boiler_reset_primary"));
            QCOMPARE(secondaryInternal->accessibleName(),
                     QString::fromLatin1("boiler_reset_secondary"));
            const QString primaryPaint =
                primaryInternal->property("displayText").toString();
            const QString secondaryPaint =
                secondaryInternal->property("displayText").toString();
            QVERIFY2(!primaryPaint.isEmpty(),
                     "Primary internal-name label paints no identity");
            QVERIFY2(!secondaryPaint.isEmpty(),
                     "Secondary internal-name label paints no identity");
            QVERIFY2(primaryPaint != secondaryPaint,
                     "Duplicate display names need distinct painted internal keys");
        }

        const QPixmap pixmap = widget.grab();
        QVERIFY(!pixmap.isNull());
        const qreal devicePixelRatio = pixmap.devicePixelRatio();
        QVERIFY(devicePixelRatio > 0.0);
        const QSize expectedPixelSize(qRound(size.width * devicePixelRatio),
                                      qRound(size.height * devicePixelRatio));
        QCOMPARE(pixmap.size(), expectedPixelSize);
        const QImage image = pixmap.toImage();
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), pixmap.size());
        verifyRegionHasRenderedContent(image, widgetRectIn(catalog, &widget),
                                       devicePixelRatio, "catalog table");
        verifyRegionHasRenderedContent(image, activeRect, devicePixelRatio,
                                       "active action table");
        verifyRegionHasRenderedContent(image, clearRect, devicePixelRatio,
                                       "clear action table");
        QTableWidget* actionTables[] = {active, clear};
        for (QTableWidget* actionTable : actionTables) {
            int sampledActionLabels = 0;
            const QRect viewportRect = actionTable->viewport()->rect();
            for (int row = 0; row < actionTable->rowCount(); ++row) {
                QWidget* cell = actionTable->cellWidget(row, 0);
                if (!cell) continue;
                const QRect cellRect = widgetRectIn(
                    cell, actionTable->viewport());
                if (!cellRect.intersects(viewportRect)) continue;
                const QList<QLabel*> labels = cell->findChildren<QLabel*>();
                for (int i = 0; i < labels.size(); ++i) {
                    QLabel* label = labels[i];
                    const QRect labelRect = mappedContentsRect(label, &widget);
                    verifyRegionHasTextInk(image, labelRect, devicePixelRatio,
                                           "mapped action label contents");
                    ++sampledActionLabels;
                }
            }
            QVERIFY2(sampledActionLabels >= 2,
                     "Each action table must sample painted label contents");
        }
        verifyRegionHasTextInk(
            image, mappedTextRect(refresh, refresh->text(), Qt::AlignCenter,
                                  &widget),
            devicePixelRatio, "refresh button text");
        verifyRegionHasTextInk(
            image, mappedTextRect(apply, apply->text(), Qt::AlignCenter,
                                  &widget),
            devicePixelRatio, "apply button text");
        verifyRegionHasTextInk(
            image, mappedTextRect(status, status->text(), status->alignment(),
                                  &widget),
            devicePixelRatio, "status text");

        if (!screenshotDir.isEmpty()) {
            const QString path = QDir(QString::fromLocal8Bit(screenshotDir))
                .filePath(QString::fromLatin1(size.fileName));
            QVERIFY2(image.save(path, "PNG"), qPrintable(
                QString::fromLatin1("Could not save screenshot to %1").arg(path)));
        }
    }
}

QTEST_MAIN(AlarmCatalogWidgetTest)
#include "test_alarm_catalog_widget.moc"
