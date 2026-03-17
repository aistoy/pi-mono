# pi-ai-c 移植状态文档 (Porting Status)

本项目是将 TypeScript 包 `@mariozechner/pi-ai` 移植到纯 C 语言的实现。

## 已实现特性 (Implemented Features)
- **统一数据模型**：实现了 `pi_ai_context_t`, `pi_ai_message_t` 等核心结构，支持文本、思考（Thinking）和工具调用内容。
- **OpenAI 兼容客户端**：基于 `libcurl` 实现了对 OpenAI 兼容 API 的流式（SSE）请求支持。
- **流式事件系统**：实现了基于回调（Callback）的事件分发，支持 `text_delta`, `thinking_delta`, `toolcall_delta` 等事件。
- **Partial JSON 解析**：实现了基础的 JSON “愈合”解析器，支持在流式过程中实时解析不完整的工具参数。
- **工具参数校验**：支持根据 JSON Schema (参数定义) 校验 `required` 字段以及基础数据类型（string, number, boolean, object, array）。
- **跨平台构建**：提供 CMake 配置，支持生成动态库。

## 相对原 JS 实现缺失的特性 (Missing Features vs JS Version)
- **多模型提供商支持**：目前仅支持 OpenAI 兼容格式。原版支持 Anthropic, Google Gemini, Bedrock, Mistral 等多种原生 API 及消息格式转换。
- **OAuth 认证流程**：未移植 Google Cloud, GitHub Copilot 等 OAuth 登录及 Token 自动刷新逻辑。
- **复杂成本计算**：目前未集成详细的模型 Token 成本元数据及计算逻辑。
- **多模态支持**：虽然结构体定义了图像类型，但目前 OpenAI 客户端尚未完全实现 Base64 图像的自动发送逻辑。
- **类型安全校验 (TypeBox)**：JS 版使用 TypeBox 提供编译时和运行时的深度类型安全，C 版仅实现了基础的运行期 JSON 校验。
- **AbortSignal 支持**：目前尚未实现通过信号中断正在进行的 HTTP 请求。
- **浏览器/前端适配**：C 版仅针对服务端/嵌入式环境，不支持原版中对 Web 端的适配逻辑。

## 未来计划
1. 增加对 Anthropic 格式的直接支持。
2. 实现 Token 成本的动态加载与计算。
3. 增加对本地 LLM (如 Ollama, llama.cpp) 的专门优化。
