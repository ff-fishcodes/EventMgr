#include <QtTest>

#include "backend/linkage_engine.h"

#include <QAtomicInt>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSemaphore>
#include <QStringList>
#include <QUuid>

#include <memory>
#include <mutex>
#include <stdexcept>
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

struct ChildProcessResult {
    bool started;
    bool completedWithinTimeout;
    bool reaped;
    QProcess::ExitStatus exitStatus;
    int exitCode;
    QByteArray output;
    QString error;

    ChildProcessResult()
        : started(false),
          completedWithinTimeout(false),
          reaped(false),
          exitStatus(QProcess::NormalExit),
          exitCode(-1),
          output(),
          error() {}
    ~ChildProcessResult() {}
};

bool isIsolatedChild(const char* markerName, const char* testFunction) {
    const QByteArray expectedPrefix = QByteArray(testFunction) + ':';
    return qgetenv(markerName).startsWith(expectedPrefix);
}

ChildProcessResult runIsolatedTest(const char* markerName,
                                   const char* testFunction,
                                   int timeoutMs) {
    ChildProcessResult result;
    QProcess child;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QByteArray markerValue = QByteArray(testFunction) + ':'
        + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    environment.insert(QString::fromLatin1(markerName),
                       QString::fromLatin1(markerValue));
    child.setProcessEnvironment(environment);
    child.setProcessChannelMode(QProcess::MergedChannels);
    child.start(QCoreApplication::applicationFilePath(),
                QStringList() << QString::fromLatin1(testFunction));

    result.started = child.waitForStarted(2000);
    result.error = child.errorString();
    if (!result.started) return result;

    result.completedWithinTimeout = child.waitForFinished(timeoutMs);
    if (result.completedWithinTimeout) {
        result.reaped = true;
    } else {
        child.kill();
        result.reaped = child.waitForFinished(2000);
    }
    result.output = child.readAll();
    result.exitStatus = child.exitStatus();
    result.exitCode = child.exitCode();
    return result;
}

struct RegisterActionOnDestruction {
    LinkageEngine* engine;

    explicit RegisterActionOnDestruction(LinkageEngine* linkageEngine)
        : engine(linkageEngine) {}
    ~RegisterActionOnDestruction() {
        engine->registerAction("from_destructor", "registered from destructor", []() {});
    }
};

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
    void releasesReplacedCallbacksWithoutHoldingConfigurationLock();
    void containsThrowingActionCallbacks();
    void containsThrowingFallbackCallbacks();
    void keepsEmptyRegisteredActionsQueryableAndSkipsExecution();
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
    QAtomicInt defaultActiveCount(0);
    QAtomicInt sharedCount(0);
    QAtomicInt eventActiveCount(0);
    QAtomicInt defaultClearCount(0);
    QAtomicInt eventClearCount(0);
    const auto configure = [this, &defaultActiveCount, &sharedCount,
                            &eventActiveCount, &defaultClearCount,
                            &eventClearCount]() {
        engine_.registerAction("default_active", "\u9ed8\u8ba4\u4ea7\u751f",
                               [&defaultActiveCount]() { defaultActiveCount.ref(); });
        engine_.registerAction("shared", "\u5171\u4eab\u52a8\u4f5c",
                               [&sharedCount]() { sharedCount.ref(); });
        engine_.registerAction("event_active", "\u4e8b\u4ef6\u4ea7\u751f",
                               [&eventActiveCount]() { eventActiveCount.ref(); });
        engine_.registerAction("default_clear", "\u9ed8\u8ba4\u6d88\u9664",
                               [&defaultClearCount]() { defaultClearCount.ref(); });
        engine_.registerAction("event_clear", "\u4e8b\u4ef6\u6d88\u9664",
                               [&eventClearCount]() { eventClearCount.ref(); });
        engine_.setLevelDefault(EventLevel::Emergency,
                                {"default_active", "shared"},
                                {"default_clear", "shared"});
        engine_.configureEvent("E-1-A",
                               {"shared", "event_active"},
                               {"shared", "event_clear"});
    };
    configure();

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

    const Event event = makeEvent("E-1-A", EventLevel::Emergency);
    engine_.executeActive(event);
    engine_.clearAll();
    QCOMPARE(defaultActiveCount.loadAcquire(), 1);
    QCOMPARE(sharedCount.loadAcquire(), 1);
    QCOMPARE(eventActiveCount.loadAcquire(), 1);
    QCOMPARE(defaultClearCount.loadAcquire(), 0);
    QCOMPARE(eventClearCount.loadAcquire(), 0);

    configure();
    engine_.executeCleared(event);
    engine_.clearAll();
    QCOMPARE(defaultActiveCount.loadAcquire(), 1);
    QCOMPARE(sharedCount.loadAcquire(), 2);
    QCOMPARE(eventActiveCount.loadAcquire(), 1);
    QCOMPARE(defaultClearCount.loadAcquire(), 1);
    QCOMPARE(eventClearCount.loadAcquire(), 1);
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

    QCOMPARE(names(first.activeActions), QStringList() << "shared" << "other");
    QCOMPARE(names(first.clearActions), QStringList() << "shared" << "other");
    QCOMPARE(names(second.activeActions), QStringList() << "shared" << "other");
    QCOMPARE(names(second.clearActions), QStringList() << "shared" << "other");
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
    QCOMPARE(names(reenabled.activeActions), QStringList() << "shared" << "other");
    QCOMPARE(names(reenabled.clearActions), QStringList() << "shared" << "other");
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
    std::string fallbackEventId;
    bool fallbackWasActive = false;
    QSemaphore registrationFinished;
    std::thread registrationThread;

    engine_.setFallback([this, &fallbackCount, &registeredDuringFallback,
                         &fallbackEventId, &fallbackWasActive,
                         &registrationFinished, &registrationThread]
                        (const std::string& eventId, bool isActive) {
        fallbackEventId = eventId;
        fallbackWasActive = isActive;
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
    QCOMPARE(QString::fromStdString(fallbackEventId), QString("E-1-A"));
    QVERIFY(fallbackWasActive);
    QVERIFY2(registeredDuringFallback.loadAcquire() == 1,
             "registerAction did not finish while fallback was running; configuration lock may be held");
    engine_.setFallback(LinkageEngine::FallbackCallback());
}

void LinkageEngineTest::releasesReplacedCallbacksWithoutHoldingConfigurationLock() {
    const char marker[] = "EVENTMGR_REENTRANT_DESTRUCTOR_CHILD";
    const char testFunction[] =
        "releasesReplacedCallbacksWithoutHoldingConfigurationLock";
    if (!isIsolatedChild(marker, testFunction)) {
        const ChildProcessResult child = runIsolatedTest(marker, testFunction, 3000);
        QVERIFY2(child.started, qPrintable(child.error));
        QVERIFY2(child.reaped, "timed-out callback destructor child could not be reaped");
        QVERIFY2(child.completedWithinTimeout,
                 "callback capture destructor re-entry deadlocked configuration");
        QVERIFY2(child.exitStatus == QProcess::NormalExit, child.output.constData());
        QVERIFY2(child.exitCode == 0, child.output.constData());
        return;
    }

    std::shared_ptr<RegisterActionOnDestruction> captured(
        new RegisterActionOnDestruction(&engine_));
    engine_.registerAction("replace_me", "replace me", [captured]() {});
    captured.reset();

    engine_.registerAction("replace_me", "replacement", []() {});
    engine_.configureEvent("E-1-A", {"from_destructor"}, {});
    const LinkageEngine::EventActionGroups groups =
        engine_.getEventActionGroups("E-1-A", EventLevel::Emergency);
    QCOMPARE(displayNames(groups.activeActions),
             QStringList() << "registered from destructor");
}

void LinkageEngineTest::containsThrowingActionCallbacks() {
    const char marker[] = "EVENTMGR_THROWING_ACTION_CHILD";
    const char testFunction[] = "containsThrowingActionCallbacks";
    if (!isIsolatedChild(marker, testFunction)) {
        const ChildProcessResult child = runIsolatedTest(marker, testFunction, 3000);
        QVERIFY2(child.started, qPrintable(child.error));
        QVERIFY2(child.reaped, "throwing action child could not be reaped");
        QVERIFY2(child.completedWithinTimeout, "throwing action child timed out");
        QVERIFY2(child.exitStatus == QProcess::NormalExit, child.output.constData());
        QVERIFY2(child.exitCode == 0, child.output.constData());
        return;
    }

    QAtomicInt invocationCount(0);
    engine_.registerAction("throws", "throws", [&invocationCount]() {
        invocationCount.ref();
        throw std::runtime_error("action failure");
    });
    engine_.configureEvent("E-1-A", {"throws"}, {});
    engine_.executeActive(makeEvent("E-1-A", EventLevel::Emergency));
    engine_.clearAll();
    QCOMPARE(invocationCount.loadAcquire(), 1);
}

void LinkageEngineTest::containsThrowingFallbackCallbacks() {
    const char marker[] = "EVENTMGR_THROWING_FALLBACK_CHILD";
    const char testFunction[] = "containsThrowingFallbackCallbacks";
    if (!isIsolatedChild(marker, testFunction)) {
        const ChildProcessResult child = runIsolatedTest(marker, testFunction, 3000);
        QVERIFY2(child.started, qPrintable(child.error));
        QVERIFY2(child.reaped, "throwing fallback child could not be reaped");
        QVERIFY2(child.completedWithinTimeout, "throwing fallback child timed out");
        QVERIFY2(child.exitStatus == QProcess::NormalExit, child.output.constData());
        QVERIFY2(child.exitCode == 0, child.output.constData());
        return;
    }

    QAtomicInt invocationCount(0);
    engine_.setFallback([&invocationCount](const std::string&, bool) {
        invocationCount.ref();
        throw std::runtime_error("fallback failure");
    });
    engine_.executeActive(makeEvent("E-1-A", EventLevel::Emergency));
    QCOMPARE(invocationCount.loadAcquire(), 1);
}

void LinkageEngineTest::keepsEmptyRegisteredActionsQueryableAndSkipsExecution() {
    const char marker[] = "EVENTMGR_EMPTY_ACTION_CHILD";
    const char testFunction[] =
        "keepsEmptyRegisteredActionsQueryableAndSkipsExecution";
    if (!isIsolatedChild(marker, testFunction)) {
        const ChildProcessResult child = runIsolatedTest(marker, testFunction, 3000);
        QVERIFY2(child.started, qPrintable(child.error));
        QVERIFY2(child.reaped, "empty action child could not be reaped");
        QVERIFY2(child.completedWithinTimeout, "empty action child timed out");
        QVERIFY2(child.exitStatus == QProcess::NormalExit, child.output.constData());
        QVERIFY2(child.exitCode == 0, child.output.constData());
        return;
    }

    engine_.registerAction("empty", "empty display", LinkageEngine::ActionCallback());
    engine_.configureEvent("E-1-A", {"empty"}, {});
    const LinkageEngine::EventActionGroups groups =
        engine_.getEventActionGroups("E-1-A", EventLevel::Emergency);
    QCOMPARE(names(groups.activeActions), QStringList() << "empty");
    QCOMPARE(displayNames(groups.activeActions), QStringList() << "empty display");

    engine_.executeActive(makeEvent("E-1-A", EventLevel::Emergency));
    engine_.clearAll();
}

void LinkageEngineTest::supportsConcurrentConfigurationQueryAndExecution() {
    const char marker[] = "EVENTMGR_CONCURRENCY_PRESSURE_CHILD";
    const char testFunction[] = "supportsConcurrentConfigurationQueryAndExecution";
    if (!isIsolatedChild(marker, testFunction)) {
        const ChildProcessResult child = runIsolatedTest(marker, testFunction, 20000);
        QVERIFY2(child.started, qPrintable(child.error));
        QVERIFY2(child.reaped, "timed-out concurrency child could not be reaped");
        QVERIFY2(child.completedWithinTimeout,
                 "concurrent linkage child exceeded 20 seconds; possible deadlock");
        QVERIFY2(child.exitStatus == QProcess::NormalExit, child.output.constData());
        QVERIFY2(child.exitCode == 0, child.output.constData());
        return;
    }

    const int iterationCount = 500;
    const int workerCount = 3;
    QAtomicInt duplicateDetected(0);
    QAtomicInt actionInvocationCount(0);
    QAtomicInt fallbackInvocationCount(0);
    QSemaphore startGate;
    std::mutex diagnosticMutex;
    std::string diagnostic;

    engine_.registerAction("shared", "shared", [&actionInvocationCount]() {
        actionInvocationCount.ref();
    });
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

    std::thread writer([this, &startGate, &actionInvocationCount,
                        &fallbackInvocationCount, iterationCount]() {
        startGate.acquire();
        for (int i = 0; i < iterationCount; ++i) {
            engine_.registerAction("shared", "shared", [&actionInvocationCount]() {
                actionInvocationCount.ref();
            });
            engine_.setFallback([&fallbackInvocationCount](const std::string&, bool) {
                fallbackInvocationCount.ref();
            });
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
    });

    const auto readerBody = [this, &startGate, &recordDuplicate, &value,
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
    };

    std::thread firstReader(readerBody);
    std::thread secondReader(readerBody);
    startGate.release(workerCount);

    writer.join();
    firstReader.join();
    secondReader.join();
    engine_.clearAll();

    QVERIFY2(duplicateDetected.loadAcquire() == 0, diagnostic.c_str());
    QVERIFY(actionInvocationCount.loadAcquire() > 0);
    QVERIFY(fallbackInvocationCount.loadAcquire() > 0);
}

QTEST_GUILESS_MAIN(LinkageEngineTest)

#include "test_linkage_engine.moc"
