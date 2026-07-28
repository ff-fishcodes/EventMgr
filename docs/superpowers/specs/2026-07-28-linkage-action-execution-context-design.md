# 联动动作执行上下文设计

## 1. 背景

外部会注册大量命名联动动作。当前所有动作都由 `LinkageEngine` 提交到固定大小为 4 的线程池，调用方无法像 Qt `connect()` 一样指定动作在哪个 QObject 所属线程执行。

目标是在保留默认线程池行为的同时，允许调用方为单个动作注册一个外部维护的 QObject 执行上下文。模块隐藏调度细节，只暴露两个注册重载。

## 2. 外部接口

`ExternalAPI` 与 `LinkageEngine` 提供对应接口：

```cpp
bool registerAction(const std::string& name,
                    const std::string& displayName,
                    ActionCallback callback);

bool registerAction(const std::string& name,
                    const std::string& displayName,
                    QObject* executionContext,
                    ActionCallback callback);

bool isActionAvailable(const std::string& actionName) const;
```

线程池重载保持现有调用源码兼容，调用方可以忽略新增的 `bool` 返回值。context 为空时不回退到线程池。

注册规则：

- action 名必须非空。
- callback 必须非空。
- context 重载要求 context 非空，且不能是 `QThread` 对象。
- `displayName` 为空时使用 action 名。
- action 名全局唯一；同名已注册时拒绝，不替换、不恢复、不注销。
- 注册失败返回 `false`、记录 `qWarning()`，并保持原注册不变。
- 注册全过程由 `LinkageEngine` 内部锁串行化，防止并发同名注册。

模块不提供 action 注销接口，也不提供生产环境清空联动接口。

## 3. 执行模式

### 3.1 线程池动作

未传 context 的 action 继续提交到 `linkagePool_`，最大并发保持 4。任务开始时获取 registration 强引用，执行期间保持 registration 存活。多次触发逐次提交，不合并、不去重；不同任务无完成顺序保证。

### 3.2 绑定执行上下文的动作

每个 context action 创建一个私有 `ActionDispatcher`：

```cpp
class ActionDispatcher : public QObject {
    Q_OBJECT
public:
    explicit ActionDispatcher(QObject* parent = nullptr);
    ~ActionDispatcher() override;
    void dispatch();

signals:
    void requested();
};
```

dispatcher 创建后移除线程亲和性，不设置 parent，只负责从任意触发线程发射 signal。注册时，将 `requested()` 以 `Qt::QueuedConnection` 连接到 executionContext，并由 receiver context 管理连接生命周期。每次触发只 emit 对应动作的 signal，不向其他 action 广播。

executionContext 由调用方维护：模块不拥有、不启动、不停止、不检查其线程或 event loop。调用方在注册前完成线程设置；动作最终在哪个线程执行由调用方提供的 context 决定。模块不读取或管理目标线程状态。

同一发送线程内保持投递顺序；多个触发线程并发投递时不保证全局顺序。callback 在 context 线程中串行执行，除非调用方主动使用嵌套 event loop 造成重入。

## 4. 注册数据模型

callback、显示名、执行方式、context 与 dispatcher 合并到单个不可变 registration，删除独立 callback map 和 displayName map：

```cpp
struct ActionRegistration {
    enum class ExecutionKind {
        ThreadPool,
        Context
    };

    std::string displayName;
    std::shared_ptr<const ActionCallback> callback;
    ExecutionKind executionKind;
    QPointer<QObject> executionContext;
    std::shared_ptr<ActionDispatcher> dispatcher;
    QMetaObject::Connection connection;
};
```

注册表：

```cpp
std::unordered_map<std::string,
    std::shared_ptr<const ActionRegistration>> actionTable_;
```

同名注册在持锁状态下直接拒绝，因此无需 generation 或动态替换机制。

## 5. 生命周期与安全

执行 context 只以 `QPointer<QObject>` 非拥有方式保存：

- 投递前发现 context 已销毁：本次跳过，并记录包含 action 名称的 `qWarning()`。
- queued callback 真正执行前再次检查 context；失效则跳过并记录日志。
- 检查后才发生销毁时，由 Qt 自动断开 connection 并安全丢弃调用；此极窄竞态允许没有日志。
- context 销毁后 registration 和事件绑定继续保留，action 名仍被占用，不能同名重新注册。
- 模块不额外连接 `destroyed` 信号，不维护销毁原子状态。

`LinkageEngine` 是进程级单例，析构发生于软件关闭。析构前调用方必须停止注册、查询和事件触发。生产析构等待线程池已提交任务完成，但不保证外部 context 队列全部执行。测试隔离使用仅测试构建可见的 reset seam，生产构建不暴露清空能力；具体测试接口由实现决定。

## 6. 回调契约

- callback 保持 `std::function<void()>`，不传事件参数。
- 调用 callback 前不持有 `LinkageEngine` 或 `ConfigManager` 锁，因此 callback 可以再次调用 `ExternalAPI` 或触发新事件。
- callback 抛出 `std::exception` 或未知异常时捕获并记录 `qWarning()`；不重试、不传播、不自动禁用。
- callback 可以触发自身再次入队；模块不增加重入锁。
- callback 只能捕获非拥有引用、裸指针或 `QPointer`；不允许捕获可能成为最后持有者的 `shared_ptr`、`unique_ptr`，或要求特定线程析构的拥有对象。
- callback 函数体不得释放捕获对象；捕获对象生命周期由调用方维护。
- callback 耗时由调用方负责。GUI context callback 不得长时间阻塞 GUI；重任务应使用工作 context 或线程池重载。
- 模块只保证 execution context，不检查 callback 捕获的其他对象。

## 7. 查询与前端

`ActionInfo` 和 Qt 侧 `ActionEntry` 增加：

```cpp
bool enabled;
bool available;
```

定义：

```text
available = registration 存在
         && callback 有效
         &&（线程池 action 或 context 未销毁）

enabled = available
       && ConfigManager 中该 event/action/phase 的用户开关开启
```

模块不检查 context 线程是否正在运行。未注册 action 的 `displayName` 回退为 `actionName`，`available=false`、`enabled=false`。

`ExternalAPI::isActionAvailable()` 查询动作本身可用性。`ExternalAPI::isEventActionEnabled()` 返回有效 enabled 值，由 `LinkageEngine` 合并 availability 与 `ConfigManager` 用户开关；`ConfigManager` 仍只保存原始用户配置。设置开关仍为 `void`，配置写入与运行时可用性分离。

前端：

```cpp
checkBox->setChecked(action.enabled);
checkBox->setEnabled(action.available);
```

不可用 action 保留在列表与联动计数中，不显示状态文字或 tooltip。context 销毁不会主动向 UI 推送；页面下次打开、刷新或应用后重载时更新。

## 8. 执行语义

- 每次事件触发对应一次动作调用，不合并、不去重触发次数。
- 同一事件阶段内，等级默认和事件专属配置中的同名动作仍按既有规则去重，只执行一次。
- 事件产生侧与消除侧引用同一 action 时，共用同一注册执行方式；需要不同线程时应注册不同 action 名。
- 线程池 action 与 context action 均为 fire-and-forget；方法投递后立即返回。
- 不保证两种动作谁先完成，也不提供 join、future、超时、重试或结果聚合。
- action 排队后用户关闭开关，不撤销已经成功投递的本次调用；开关只影响后续触发。
- fallback 不支持执行上下文，本次不改变其语义。

## 9. 日志

至少记录：

```text
Linkage action '<name>' skipped: execution context destroyed
Linkage action '<name>' threw: <exception>
Linkage action '<name>' registration rejected: duplicate name
Linkage action '<name>' registration rejected: empty callback
```

每次执行尝试发现 context 已销毁都记录日志；单纯 availability 查询不记录。检查后才销毁导致 Qt 自动丢弃的极窄竞态允许没有日志。

## 10. 不包含

- fallback 的线程亲和性。
- callback 事件参数。
- action 注销、生产清空、动态替换。
- 全局执行顺序、执行结果聚合、join、超时、重试。
- context 线程或 event loop 管理与检查。
- 主动向前端推送 availability 变化。
- 成员函数注册模板语法糖。
- action 执行方式、context 或线程的外部查询。
- 注册数量硬上限。

## 11. 测试范围

1. 线程池 action 确实在线程池线程执行。
2. context action 在 context 所属线程执行。
3. context 销毁后不执行，查询 unavailable，有效 enabled=false。
4. context 失效时前端复选框取消且禁用。
5. 重复名及空名称、空 callback、空 context、QThread context 注册失败，原 registration 不变。
6. 未注册 action 显示内部名且不可用。
7. callback 异常不会越过线程池或 Qt event loop。
8. 多次触发逐次执行，不合并。
9. 测试 reset 能取消旧 queued callback，且生产接口不暴露清空能力。
10. `ExternalAPI` 两个重载、availability 和有效 enabled 查询一致。
11. 使用 `QSemaphore` 或条件等待确定性控制队列，不使用固定 sleep。
