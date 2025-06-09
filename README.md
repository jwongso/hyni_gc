# hyni_gc
**Hyni** - Dynamic, schema-based context management for chat APIs

*Write once, run anywhere. Switch between OpenAI, Claude, DeepSeek, and more with zero code changes.*

---

## ✨ Why Hyni?

```cpp
// One line to rule them all - works with ANY LLM provider
auto response = hyni::chat("Explain quantum computing")
    .provider("claude")           // or "openai", "deepseek", "mistral"
    .temperature(0.7)
    .max_tokens(500)
    .send();
```

**No more provider-specific code.** No more JSON wrestling. Just pure, elegant AI conversations.

---

## 🚀 Features

- 🎯 **Provider Agnostic** - Switch between OpenAI, Claude, DeepSeek with one line
- 🧠 **Smart Context Management** - Automatic conversation history and memory
- 🎙️ **Live Audio Transcription** - Built-in Whisper integration (TODO)
- 💬 **Streaming & Async** - Real-time responses with full async support
- 🛑 **Cancellation Control** - Stop requests mid-flight
- 📦 **Modern C++20** - Clean, expressive API design
- 🔧 **Schema-Driven** - JSON configs handle all provider differences

---

## 🎯 Quick Start

### The Simplest Way
```cpp
#include "general_context.h"
#include "chat_api.h"
using namespace hyni;

// Create a context and chat API
auto context = std::make_unique<general_context>("openai");
chat_api chat(std::move(context));
chat.get_context().set_api_key("XYZ");

// One-liner conversations
std::string answer = chat.send_message("What is recursion?");
```

### Builder Pattern with Context
```cpp
// Set up your context
auto context = std::make_unique<general_context>("claude");
context->set_system_message("You are a helpful coding assistant")
       ->set_parameter("temperature", 0.8)
       ->set_parameter("max_tokens", 1000)
       ->set_api_key("1234567890");

chat_api chat(std::move(context));

// Reuse the same chat for multiple questions
std::string response = chat.send_message("Write a C++ class for a stack");
chat.get_context().add_assistant_message(response);
response = chat.send_message("Now add error handling");
chat.get_context().add_assistant_message(response);
response = chat.send_message("Show me how to test it");
chat.get_context().add_assistant_message(response);
response = chat.send_message("And now, explain me the time and space complexities");
```

### Advanced Context Management
```cpp
auto context = std::make_unique<general_context>("claude");
context->set_system_message("You are an expert software architect");

// Build conversations naturally
context->add_user_message("I need to design a microservice");
context->add_assistant_message("I'd be happy to help! What's the service for?");
context->add_user_message("User authentication and JWT tokens");

chat_api chat(std::move(context));
auto response = chat.send_message(); // Send with existing context
```

---

## 🔄 Sync vs Async - Your Choice

### Synchronous (Simple)
```cpp
auto context = std::make_unique<general_context>("openai");
chat_api chat(std::move(context));

std::string result = chat.send_message("Explain async programming");
std::cout << result << std::endl;
```

### Asynchronous (Powerful)
```cpp
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

auto future = chat.send_message_async("Generate a story about robots");

// Do other work...
std::this_thread::sleep_for(std::chrono::seconds(1));

// Get result when ready
std::string story = future.get();
```

### Streaming (Real-time)
```cpp
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

chat.send_message_stream("Write a long technical article",
    [](const std::string& chunk) {
        std::cout << chunk << std::flush;  // Print as it arrives
        return true;  // Continue streaming
    },
    [](const std::string& final_response) {
        std::cout << "\n--- Complete! ---\n";
    });
```

### Cancellable Operations
```cpp
std::atomic<bool> should_cancel{false};

auto context = std::make_unique<general_context>("openai");
chat_api chat(std::move(context));

auto future = chat.send_message_async("This might take a while...");

// Cancel check function
auto cancel_check = [&should_cancel]() { 
    return should_cancel.load(); 
};

// Cancel after 5 seconds
std::this_thread::sleep_for(std::chrono::seconds(5));
should_cancel = true;
```

---

## 🎨 Multiple Ways to Build Conversations

### 1. Direct Message Sending
```cpp
auto context = std::make_unique<general_context>("claude");
context->set_system_message("You are a senior C++ developer");

chat_api chat(std::move(context));
auto response = chat.send_message("Implement quicksort in C++");
```

### 2. Context-First Approach (Recommended)
```cpp
auto context = std::make_unique<general_context>("openai");
context->set_system_message("You are a creative writer");
context->add_user_message("Write a haiku about programming");

chat_api chat(std::move(context));
std::string poem = chat.send_message(); // Uses existing context
```

### 3. Conversational Building
```cpp
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

// Build conversation step by step
chat.get_context().add_user_message("Hello!");
chat.get_context().add_assistant_message("Hi! How can I help you?");
chat.get_context().add_user_message("Tell me about C++ smart pointers");

std::string response = chat.send_message();
```

### 4. Parameter Configuration
```cpp
auto context = std::make_unique<general_context>("claude");
context->set_parameter("temperature", 0.3);
context->set_parameter("max_tokens", 500);
context->set_parameter("top_p", 0.9);

chat_api chat(std::move(context));
std::string response = chat.send_message("Explain machine learning");
```

---

## 🔧 Provider Configuration

### Dynamic Provider Switching
```cpp
// Create contexts for different providers
auto openai_context = std::make_unique<general_context>("openai");
auto claude_context = std::make_unique<general_context>("claude");
auto deepseek_context = std::make_unique<general_context>("deepseek");

// Same question, different providers
std::string question = "Explain machine learning";

chat_api openai_chat(std::move(openai_context));
chat_api claude_chat(std::move(claude_context));
chat_api deepseek_chat(std::move(deepseek_context));

std::string openai_view = openai_chat.send_message(question);
std::string claude_view = claude_chat.send_message(question);
std::string deepseek_view = deepseek_chat.send_message(question);
```

### Context Access and Modification
```cpp
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

// Access context for advanced configuration
chat.get_context().set_parameter("temperature", 0.8);
chat.get_context().set_parameter("max_tokens", 2000);
chat.get_context().add_system_message("You are an expert in distributed systems");

std::string response = chat.send_message("How do I design a scalable API?");
```

---

## 🎙️ Audio Integration

```cpp
// Live transcription with Whisper
hyni::audio_transcriber transcriber;
transcriber.start_listening([](const std::string& text) {
    if (!text.empty()) {
        auto response = hyni::ask(text).provider("claude");
        speak(response);  // Text-to-speech
    }
});
```

---

## 📐 Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Your Code     │──▶│  Hyni Context    │──▶│  LLM Provider   │
│                 │    │  Management      │    │  (Any/All)      │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         │              ┌────────▼────────┐              │
         │              │ Schema Engine   │              │
         │              │ (JSON Config)   │              │
         │              └─────────────────┘              │
         │                                               │
         ▼                                               ▼
┌─────────────────┐                        ┌─────────────────┐
│ Audio/Whisper   │                        │   Streaming     │
│ Integration     │                        │   Response      │
└─────────────────┘                        └─────────────────┘
```

### Core Components

- **`general_context`** - Smart conversation management with automatic history
- **`chat_api`** - Provider-agnostic HTTP client with streaming support  
- **`prompt`** - Type-safe message construction with validation
- **`schema_engine`** - JSON-driven provider configuration

---

## 🔄 Advanced Features

### Conversation State Management
```cpp
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

// Build up conversation history
chat.send_message("What are the SOLID principles?");
chat.send_message("Can you give me an example of the Single Responsibility Principle?");
chat.send_message("How does this apply to C++ classes?");

// Context automatically maintains conversation history
// Each new message builds on the previous conversation
```

### Stream Processing with Completion Callbacks
```cpp
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

std::string accumulated_response;

chat.send_message_stream("Write a detailed explanation of RAII",
    // Chunk callback - called for each piece of response
    [&accumulated_response](const std::string& chunk) {
        std::cout << chunk << std::flush;
        accumulated_response += chunk;
        return true; // Continue streaming
    },
    // Completion callback - called when stream is complete
    [&accumulated_response](const std::string& final_response) {
        std::cout << "\n\n--- Stream Complete ---\n";
        std::cout << "Total length: " << final_response.length() << " characters\n";
        
        // Save to file, log, or process final response
        save_to_file("raii_explanation.txt", final_response);
    });
```

### Error Handling and Cancellation
```cpp
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

std::atomic<bool> user_cancelled{false};

try {
    auto future = chat.send_message_async("Generate a very long story...");
    
    // Simulate user cancellation after 3 seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));
    user_cancelled = true;
    
    std::string result = future.get();
    
} catch (const std::exception& e) {
    std::cerr << "Request failed or was cancelled: " << e.what() << std::endl;
}
```

### Context Persistence and Reuse
```cpp
// Create and use a context
auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

chat.send_message("Remember that I'm working on a chess engine project");
chat.send_message("What data structures should I use for the board?");

// Get reference to context for inspection
general_context& ctx = chat.get_context();
std::cout << "Conversation has " << ctx.get_message_count() << " messages\n";

// Context automatically maintains full conversation history
// No need for manual session management
```

---

## 🛠️ Error Handling

```cpp
#include <stdexcept>

auto context = std::make_unique<general_context>("claude");
chat_api chat(std::move(context));

try {
    std::string response = chat.send_message("Hello world");
    std::cout << response << std::endl;
    
} catch (const std::runtime_error& e) {
    std::cerr << "Chat API error: " << e.what() << std::endl;
} catch (const std::exception& e) {
    std::cerr << "General error: " << e.what() << std::endl;
}

// With streaming and cancellation
std::atomic<bool> should_stop{false};

chat.send_message_stream("Long generation task...",
    [](const std::string& chunk) {
        std::cout << chunk;
        return true; // Continue
    },
    [](const std::string& final) {
        std::cout << "\nGeneration complete!\n";
    },
    [&should_stop]() {
        return should_stop.load(); // Check for cancellation
    });
```

---

## 📋 Installation

```bash
# CMake
find_package(hyni REQUIRED)
target_link_libraries(your_target hyni::hyni)

# vcpkg
vcpkg install hyni

# Conan
conan install hyni/1.0@
```

---

## 🤝 Contributing

We love contributions! Whether it's new LLM providers, features, or bug fixes.

```bash
git clone https://github.com/your-org/hyni_gc.git
cd hyni_gc
mkdir build && cd build
cmake ..
make -j$(nproc)
```

---

## 📄 License

MIT License - Use freely in commercial and open-source projects.

---

**Ready to supercharge your AI conversations?** 

```cpp
#include "chat_api.h"
#include "general_context.h"

auto context = std::make_unique<hyni::general_context>("claude");
hyni::chat_api chat(std::move(context));

auto response = chat.send_message("How do I get started with Hyni?");
// "Just create a context, pass it to chat_api, and start chatting! 🚀"
```

Generated by Claude Sonnet 4
