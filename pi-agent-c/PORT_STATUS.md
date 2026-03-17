# pi-agent-c 移植状态文档 (Porting Status)

本项目是将 TypeScript 包 `@mariozechner/pi-agent-core` 移植到纯 C 语言的实现。

## 已实现特性 (Implemented Features)
- **自主迭代循环 (Autonomous Loop)**：实现了模型响应、工具执行、结果回填、再次请求的完整 Agent 闭环逻辑。
- **并行/串行执行控制**：支持全局切换工具执行模式。
- **多线程并行支持**：基于 `pthreads` 实现了多个工具调用的并发执行，显著提升 I/O 密集型工具的效率。
- **工具注册系统**：支持通过 C 语言函数指针将原生逻辑绑定为 Agent 可用的工具。
- **动态链接**：作为一个独立的库，动态链接 `pi-ai-c` 库。

## 相对原 JS 实现缺失的特性 (Missing Features vs JS Version)
- **Steering & Follow-up 队列**：尚未实现“中途注入指令 (Steering)”和“后续自动执行 (Follow-up)”的异步队列管理。
- **高级钩子函数**：原版支持 `beforeToolCall` 和 `afterToolCall` 拦截器，允许修改工具输入输出或跳过执行，C 版目前需在工具函数内部处理类似逻辑。
- **上下文自动剪裁 (Pruning)**：原版支持在 LLM 调用前根据 Token 数自动修剪对话历史，C 版目前需要人工管理上下文长度。
- **多模态消息处理**：暂未实现对复杂 Agent 附件（Attachments）的统一管理。
- **异步事件流 (EventStream)**：JS 版使用 AsyncIterator 提供优雅的流处理体验，C 版使用传统的 Callback 机制，对于复杂的嵌套异步逻辑处理稍显繁琐。
- **错误重试策略**：原版支持细粒度的服务器错误重试延迟管理，C 版目前仅实现了基础的错误回调。

## 运行要求
- 需要链接 `libpi_ai_c`。
- 系统需支持 `pthreads` (POSIX)。
