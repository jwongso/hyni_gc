# hyni_gc
Hyni - schema based context management chat API

---

## 🚀 Features

- 🎙️ **Live Audio Transcription** (e.g., via Whisper)
- 🧠 **General Context Management** with conversation history and system prompt support
- 🔁 **Multi-LLM Support** (OpenAI, Claude, DeepSeek, and more)
- 💬 **Streaming Chat API** for real-time feedback
- 🛑 **Cancellation-aware Requests** using `libcurl`
- 📦 Written in **Modern C++**
- ⚙️ Cross-language integration support (C++, Python, JavaScript, Swift, etc.)

---

## 📐 Architecture Overview

```text
[ Client (Qt / CLI / Web) ]
            |
            v
[ Audio Input / Whisper STT ]
            |
            v
[ GeneralContext → PromptBuilder → ChatAPI ]
            |
            v
[ LLM Provider (OpenAI, Claude, DeepSeek...) ]
```

## Core Components

- `general_context`
Handles prompt history, user/system messages, request schemas, and JSON generation.

- `chat_api`
Manages LLM API interactions, supports streaming and cancellation.

- `prompt`
Encapsulates user questions, context type (coding, design, behavioral), and system role.

## Example Usage

### Using Builder Pattern
```cpp
hyni::prompt prompt = hyni::prompt()
    .with_type(hyni::question_type::Coding)
    .with_user_text("Write a C++ function to reverse a string.")
    .with_system_message("You are an expert C++ developer.");

std::string response = m_chat_api.send(prompt, [] { return false; });
std::cout << "AI: " << response << std::endl;
```

### Direct Message Management
```cpp
m_context->add_user_message("How do I implement a binary search in C++?");
m_context->add_system_message("Answer like a senior engineer mentoring a junior.");
```

## JSON Request Generation

Hyni builds API payloads automatically and strips null fields for provider compliance:
```cpp
nlohmann::json request = general_context.build_request();
// All null fields are recursively removed before sending
```

Switching Providers

Switch easily between providers with a simple config:

m_context->set_llm_provider(API_PROVIDER::ClaudeAI);
