# pi-agent-c 移植状态文档 (Porting Status)

本项目是将 TypeScript 包 `@mariozechner/pi-agent-core` 移植到纯 C 语言的实现。

## 已实现特性 (Implemented Features)
- **自主迭代循环 (Autonomous Loop)**：实现了模型响应、工具执行、结果回填、再次请求的完整 Agent 闭环逻辑。
- **并行/串行执行控制**：支持全局切换工具执行模式。
- **多线程并行支持**：基于 `pthreads` 实现了多个工具调用的并发执行。
- **工具注册系统**：支持通过 C 语言函数指针将原生逻辑绑定为 Agent 可用的工具。
- **前置/后置钩子 (Hooks)**：支持 `before_tool_call` 和 `after_tool_call` 拦截器。
- **错误重试机制 (Retry)**：支持配置 LLM 请求失败时的自动重试次数及延迟。
- **异步指令注入 (Steering)**：允许在运行时异步注入新指令。支持：
    - **迭代间注入**：每一轮对话开始前合并新指令。
    - **执行间中断**：在串行工具执行期间发现新指令可立即中断。
    - **流式中断**：模型正在输出时注入指令可立即强行关闭当前网络连接。
- **后续任务编排 (Follow-up)**：支持预排队指令，在当前任务彻底完成后自动续接新任务。
- **动态链接**：作为一个独立的库，动态链接 `pi-ai-c` 库。

## 异步交互架构设计 (Async Architecture)
为了支持 Steering 和 Follow-up，`pi-agent-c` 引入了以下设计：

### 1. 线程安全 (Thread Safety)
`pi_agent_t` 结构体包含一个互斥锁 (`pthread_mutex_t`)。所有对 `steering_queue` 和 `follow_up_queue` 的操作都受该锁保护。这允许外部线程（如用户界面或网络监听器）在 Agent 运行期间安全地注入指令。

### 2. 双队列机制 (Dual Queues)
- **Steering Queue**：高优先级，用于“中途转向”。Agent 在每个关键决策点都会检查此队列。
- **Follow-up Queue**：正常优先级，用于“链式任务”。Agent 只有在空闲时才会检查此队列。

### 3. 中断检查点 (Checkpoints)
主循环在以下三个位置进行检查：
1.  **Request Start**：发起 LLM 请求前。
2.  **Tool Execution**：执行工具之间。
3.  **Stream Write**：底层网络回调中（通过 `abort_signal` 指针实现毫秒级响应）。

## 相对原 JS 实现缺失的特性 (Missing Features vs JS Version)
- **上下文自动剪裁 (Pruning)**：原版支持在 LLM 调用前根据 Token 数自动修剪对话历史。
- **多模态附件管理**：暂未实现对复杂 Agent 附件（Attachments）的统一管理。
- **异步事件流 (EventStream)**：JS 版使用 AsyncIterator 提供优雅体验。
- **精确 Token 统计**：尚未集成 C 语言版的 BPE 分词统计。

## 运行要求
- 需要链接 `libpi_ai_c`。
- 系统需支持 `pthreads` (POSIX)。
