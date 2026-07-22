#include <QtTest>

#include "backend/linkage_engine.h"

#include <QAtomicInt>
#include <QSemaphore>
#include <QStringList>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

Event makeEvent(const EventId& id, EventLevel level) {
    Event value;
    value.id = id;
    value.source = EventSource::Device;
    value.deviceName = "E";
    value.frameID = 1;
    value.alarmField = "A";
    value.description = "linkage test event";
    value.originalLevel = level;
    value.effectiveLevel = level;
    value.state = EventState::Active;
    value.timestamp = "2026-07-22 00:00:00";
    return value;
}

template <typename Actions>
QStringList names(const Actions& actions) {
    QStringList result;
    for (typename Actions::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        result.append(QString::fromStdString(it->name));
    }
    return result;
}

template <typename Actions>
QStringList displayNames(const Actions& actions) {
    QStringList result;
    for (typename Actions::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        result.append(QString::fromStdString(it->displayName));
    }
    return result;
}

// Keep enabled lookup dependent on the future ActionInfo contract. This lets the
// current build stop at the intentionally missing grouped API instead of emitting
// an unrelated helper-definition error.
template <typename Actions>
bool enabledFor(const Actions& actions, const std::string& name) {
    for (typename Actions::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        if (it->name == name) return it->enabled;
    }
    return false;
}

template <typename Actions>
QList<bool> enabledStates(const Actions& actions) {
    QList<bool> result;
    for (typename Actions::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        result.append(it->enabled);
    }
    return result;
}

template <typename Actions>
bool hasDuplicateNames(const Actions& actions) {
    std::unordered_set<std::string> seen;
    for (typename Actions::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
        if (!seen.insert(it->name).second) return true;
    }
    return false;
}

bool waitForCount(const QAtomicInt& counter, int expected, int timeoutMs = 2000) {
    QElapsedTimer timer;
    timer.start();
    while (counter.loadAcquire() != expected && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::yieldCurrentThread();
    }
    return counter.loadAcquire() == expected;
}

} // namespace

class LinkageEngineTest : public QObject {
    Q_OBJECT

public:
    explicit LinkageEngineTest(QObject* parent = NULL);
    ~LinkageEngineTest();

private slots:
    void init();
    void cleanup();
    void returnsDefaultsBeforeEventActionsAndDeduplicates();
    void keepsActiveAndClearActionsIndependent();
    void isolatesDisableStateByEventActionAndPhase();
    void suppressesInfoActionsAndFallback();
    void invokesFallbackWithoutHoldingConfigurationLock();
    void supportsConcurrentConfigurationQueryAndExecution();

private:
    LinkageEngine engine_;
};

LinkageEngineTest::LinkageEngineTest(QObject* parent)
    : QObject(parent), engine_() {}

LinkageEngineTest::~LinkageEngineTest() {}

void LinkageEngineTest::init() {
    engine_.clearAll();
    engine_.setFallback(LinkageEngine::FallbackCallback());
}

void LinkageEngineTest::cleanup() {
    engine_.setFallback(LinkageEngine::FallbackCallback());
    engine_.clearAll();
}

void LinkageEngineTest::returnsDefaultsBeforeEventActionsAndDeduplicates() {
    engine_.registerAction("default_active", "\u9ed8\u8ba4\u4ea7\u751f", []() {});
    engine_.registerAction("shared", "\u5171\u4eab\u52a8\u4f5c", []() {});
    engine_.registerAction("event_active", "\u4e8b\u4ef6\u4ea7\u751f", []() {});
    engine_.registerAction("default_clear", "\u9ed8\u8ba4\u6d88\u9664", []() {});
    engine_.registerAction("event_clear", "\u4e8b\u4ef6\u6d88\u9664", []() {});

    engine_.setLevelDefault(EventLevel::Emergency,
                            {"default_active", "shared"},
                            {"default_clear", "shared"});
    engine_.configureEvent("E-1-A",
                           {"shared", "event_active"},
                           {"shared", "event_clear"});

    const auto groups =
        engine_.getEventActionGroups("E-1-A", EventLevel::Emergency);

    QCOMPARE(names(groups.activeActions),
             QStringList() << "default_active" << "shared" << "event_active");
    QCOMPARE(names(groups.clearActions),
             QStringList() << "default_clear" << "shared" << "event_clear");
    QCOMPARE(displayNames(groups.activeActions),
             QStringList() << QString::fromUtf8("\u9ed8\u8ba4\u4ea7\u751f")
                           << QString::fromUtf8("\u5171\u4eab\u52a8\u4f5c")
                           << QString::fromUtf8("\u4e8b\u4ef6\u4ea7\u751f"));
    QCOMPARE(displayNames(groups.clearActions),
             QStringList() << QString::fromUtf8("\u9ed8\u8ba4\u6d88\u9664")
                           << QString::fromUtf8("\u5171\u4eab\u52a8\u4f5c")
                           << QString::fromUtf8("\u4e8b\u4ef6\u6d88\u9664"));
    QCOMPARE(enabledStates(groups.activeActions), QList<bool>() << true << true << true);
    QCOMPARE(enabledStates(groups.clearActions), QList<bool>() << true << true << true);
}

void LinkageEngineTest::keepsActiveAndClearActionsIndependent() {
    engine_.registerAction("default_active_only", "default active only", []() {});
    engine_.registerAction("default_clear_only", "default clear only", []() {});
    engine_.registerAction("event_active_only", "event active only", []() {});
    engine_.registerAction("event_clear_only", "event clear only", []() {});
    engine_.setLevelDefault(EventLevel::Emergency,
                            {"default_active_only"},
                            {"default_clear_only"});
    engine_.configureEvent("E-1-A",
                           {"event_active_only"},
                           {"event_clear_only"});

    const LinkageEngine::EventActionGroups groups =
        engine_.getEventActionGroups("E-1-A", EventLevel::Emergency);

    QCOMPARE(names(groups.activeActions),
             QStringList() << "default_active_only" << "event_active_only");
    QCOMPARE(names(groups.clearActions),
             QStringList() << "default_clear_only" << "event_clear_only");
    QVERIFY(!names(groups.activeActions).contains("default_clear_only"));
    QVERIFY(!names(groups.activeActions).contains("event_clear_only"));
    QVERIFY(!names(groups.clearActions).contains("default_active_only"));
    QVERIFY(!names(groups.clearActions).contains("event_active_only"));
}

void LinkageEngineTest::isolatesDisableStateByEventActionAndPhase() {
    engine_.registerAction("shared", "shared", []() {});
    engine_.registerAction("other", "other", []() {});
    engine_.configureEvent("E-1-A", {"shared", "other"}, {"shared", "other"});
    engine_.configureEvent("E-2-A", {"shared", "other"}, {"shared", "other"});
    engine_.disableAction("E-1-A", "shared", true);
    engine_.disableAction("E-1-A", "other", false);

    const LinkageEngine::EventActionGroups first =
        engine_.getEventActionGroups("E-1-A", EventLevel::Emergency);
    const LinkageEngine::EventActionGroups second =
        engine_.getEventActionGroups("E-2-A", EventLevel::Emergency);

    QVERIFY(names(first.activeActions).contains("shared"));
    QVERIFY(names(first.clearActions).contains("shared"));
    QVERIFY(names(second.activeActions).contains("shared"));
    QVERIFY(names(second.clearActions).contains("shared"));
    QVERIFY(!enabledFor(first.activeActions, "shared"));
    QVERIFY(enabledFor(first.clearActions, "shared"));
    QVERIFY(enabledFor(first.activeActions, "other"));
    QVERIFY(!enabledFor(first.clearActions, "other"));
    QVERIFY(enabledFor(second.activeActions, "shared"));
    QVERIFY(enabledFor(second.clearActions, "shared"));
    QVERIFY(enabledFor(second.activeActions, "other"));
    QVERIFY(enabledFor(second.clearActions, "other"));

    engine_.enableAction("E-1-A", "shared", true);
    const LinkageEngine::EventActionGroups reenabled =
        engine_.getEventActionGroups("E-1-A", EventLevel::Emergency);
    QVERIFY(enabledFor(reenabled.activeActions, "shared"));
    QVERIFY(!enabledFor(reenabled.clearActions, "other"));
}

void LinkageEngineTest::suppressesInfoActionsAndFallback() {
    QAtomicInt actionCount(0);
    QAtomicInt fallbackCount(0);
    engine_.registerAction("info_action", "info action", [&actionCount]() {
        actionCount.ref();
    });
    engine_.setLevelDefault(EventLevel::Info,
                            {"info_action"},
                            {"info_action"});
    engine_.configureEvent("E-1-I", {"info_action"}, {"info_action"});
    engine_.setFallback([&fallbackCount](const std::string&, bool) {
        fallbackCount.ref();
    });

    const LinkageEngine::EventActionGroups groups =
        engine_.getEventActionGroups("E-1-I", EventLevel::Info);
    QVERIFY(groups.activeActions.empty());
    QVERIFY(groups.clearActions.empty());

    const Event info = makeEvent("E-1-I", EventLevel::Info);
    engine_.executeActive(info);
    engine_.executeCleared(info);
    engine_.clearAll(); // Wait for any incorrectly submitted asynchronous work.

    QCOMPARE(actionCount.loadAcquire(), 0);
    QCOMPARE(fallbackCount.loadAcquire(), 0);
    engine_.setFallback(LinkageEngine::FallbackCallback());
}

void LinkageEngineTest::invokesFallbackWithoutHoldingConfigurationLock() {
    QAtomicInt fallbackCount(0);
    QAtomicInt registeredDuringFallback(0);
    QSemaphore registrationFinished;
    std::thread registrationThread;

    engine_.setFallback([this, &fallbackCount, &registeredDuringFallback,
                         &registrationFinished, &registrationThread]
                        (const std::string&, bool) {
        registrationThread = std::thread([this, &registrationFinished]() {
            engine_.registerAction("from_fallback", "\u56de\u8c03\u5185\u6ce8\u518c", []() {});
            registrationFinished.release();
        });

        // Registration must complete before fallback returns, proving config is unlocked.
        if (registrationFinished.tryAcquire(1, 2000)) {
            registeredDuringFallback.storeRelease(1);
        }
        fallbackCount.ref();
    });

    engine_.executeActive(makeEvent("E-1-A", EventLevel::Emergency));
    if (registrationThread.joinable()) registrationThread.join();

    QCOMPARE(fallbackCount.loadAcquire(), 1);
    QVERIFY2(registeredDuringFallback.loadAcquire() == 1,
             "registerAction did not finish while fallback was running; configuration lock may be held");
    engine_.setFallback(LinkageEngine::FallbackCallback());
}

void LinkageEngineTest::supportsConcurrentConfigurationQueryAndExecution() {
    const int iterationCount = 500;
    const int workerCount = 3;
    QAtomicInt duplicateDetected(0);
    QAtomicInt completedWorkers(0);
    QSemaphore startGate;
    QSemaphore completionGate;
    std::mutex diagnosticMutex;
    std::string diagnostic;

    engine_.registerAction("shared", "shared", []() {});
    engine_.registerAction("writer_a", "writer a", []() {});
    engine_.registerAction("writer_b", "writer b", []() {});

    const Event value = makeEvent("E-1-A", EventLevel::Emergency);
    const auto recordDuplicate = [&duplicateDetected, &diagnosticMutex, &diagnostic]
                                 (const char* phase, int iteration) {
        if (duplicateDetected.testAndSetOrdered(0, 1)) {
            std::lock_guard<std::mutex> locker(diagnosticMutex);
            diagnostic = std::string("duplicate action name in ") + phase
                       + " group at iteration " + std::to_string(iteration);
        }
    };

    std::thread writer([this, &startGate, &completionGate, &completedWorkers,
                        iterationCount]() {
        startGate.acquire();
        for (int i = 0; i < iterationCount; ++i) {
            if ((i % 2) == 0) {
                engine_.configureEvent("E-1-A",
                                       {"shared", "writer_a", "shared"},
                                       {"writer_b", "shared", "writer_b"});
                engine_.setLevelDefault(EventLevel::Emergency,
                                        {"writer_b", "shared", "writer_b"},
                                        {"shared", "writer_a", "shared"});
                engine_.disableAction("E-1-A", "shared", true);
                engine_.enableAction("E-1-A", "shared", false);
            } else {
                engine_.configureEvent("E-1-A",
                                       {"writer_b", "shared", "writer_b"},
                                       {"shared", "writer_a", "shared"});
                engine_.setLevelDefault(EventLevel::Emergency,
                                        {"shared", "writer_a", "shared"},
                                        {"writer_b", "shared", "writer_b"});
                engine_.enableAction("E-1-A", "shared", true);
                engine_.disableAction("E-1-A", "shared", false);
            }
        }
        completedWorkers.ref();
        completionGate.release();
    });

    const auto readerBody = [this, &startGate, &completionGate, &completedWorkers,
                             &recordDuplicate, &value,
                             iterationCount]() {
        startGate.acquire();
        for (int i = 0; i < iterationCount; ++i) {
            const LinkageEngine::EventActionGroups groups =
                engine_.getEventActionGroups("E-1-A", EventLevel::Emergency);
            if (hasDuplicateNames(groups.activeActions)) recordDuplicate("active", i);
            if (hasDuplicateNames(groups.clearActions)) recordDuplicate("clear", i);
            engine_.executeActive(value);
            engine_.executeCleared(value);
        }
        completedWorkers.ref();
        completionGate.release();
    };

    std::thread firstReader(readerBody);
    std::thread secondReader(readerBody);
    startGate.release(workerCount);

    const bool completedInTime = completionGate.tryAcquire(workerCount, 15000);
    writer.join();
    firstReader.join();
    secondReader.join();

    QVERIFY2(completedInTime,
             "concurrent linkage workers did not complete within 15 seconds; possible deadlock");
    QVERIFY(waitForCount(completedWorkers, workerCount));
    QVERIFY2(duplicateDetected.loadAcquire() == 0, diagnostic.c_str());
}

QTEST_GUILESS_MAIN(LinkageEngineTest)

#include "test_linkage_engine.moc"
