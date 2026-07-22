#ifndef ALARM_CATALOG_WIDGET_H
#define ALARM_CATALOG_WIDGET_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

#include "backend_bridge.h"
#include "ui_alarm_catalog_widget.h"

class AlarmCatalogWidget : public QWidget {
    Q_OBJECT
public:
    enum class DirtyDecision {
        Apply,
        Discard,
        Cancel
    };

    explicit AlarmCatalogWidget(BackendBridge* bridge, QWidget* parent = nullptr);
    ~AlarmCatalogWidget() override;

public slots:
    void loadCatalog();
    void requestReload();
    bool requestLeave();
    void on_applyBtn_clicked();

protected:
    virtual DirtyDecision confirmDirtyChanges();

private:
    // Holds the backend snapshot and the operator's staged values for one event.
    struct PendingEventConfig {
        bool originalDowngraded;
        bool downgraded;
        bool originalShielded;
        bool shielded;
        QMap<QString, bool> originalActiveActions;
        QMap<QString, bool> activeActions;
        QMap<QString, bool> originalClearActions;
        QMap<QString, bool> clearActions;

        PendingEventConfig();
        ~PendingEventConfig();
        bool isDirty() const;
    };

    void reloadFromBackend();
    void buildCatalogRows();
    void selectInitialEvent();
    void switchSelectedEvent(int currentRow);
    void renderSelectedActions();
    void renderActionTable(QTableWidget* table,
                           const QVector<BackendBridge::ActionEntry>& orderedActions,
                           bool activePhase);
    void updateDirtyUi();
    bool hasDirtyChanges() const;
    void applyActionDiffs(const QString& eventId,
                          const QMap<QString, bool>& original,
                          const QMap<QString, bool>& current,
                          bool isActive);
    const BackendBridge::EventActionGroups* actionGroupsForEvent(
        const QString& eventId) const;

    Ui::AlarmCatalogWidget ui;
    BackendBridge* bridge_; // Non-owning; the application owns the bridge.
    QMap<QString, PendingEventConfig> pendingByEvent_;
    QVector<BackendBridge::CatalogEntry> catalog_;
    QString selectedEventId_;
    bool loadingUi_;
    // Keeps backend-provided action ordering alongside the map-based values.
    QMap<QString, BackendBridge::EventActionGroups> actionGroupsByEvent_;
};

#endif
