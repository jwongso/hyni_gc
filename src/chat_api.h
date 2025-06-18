#pragma once

#include <memory>
#include "http_client.h"
#include "general_context.h"

namespace hyni
{

/**
 * @brief Main class for interacting with LLM chat APIs
 *
 * This class provides methods for sending messages to LLM APIs,
 * with support for synchronous, asynchronous, and streaming interactions.
 */
class chat_api {
public:
    /**
     * @brief Constructs a chat API with the given context
     * @param context The general context to use for API interactions
     */
    explicit chat_api(std::unique_ptr<general_context> context);

    /**
     * @brief Sends a message and waits for a response
     * @param message The message to send
     * @param cancel_check Optional callback to check if the operation should be cancelled
     * @return The response text
     * @throws std::runtime_error If the request fails or the response cannot be parsed
     */
    [[nodiscard]] std::string send_message(const std::string& message, progress_callback cancel_check = nullptr);

    /**
     * @brief Sends a message and streams the response
     * @param message The message to send
     * @param on_chunk Callback function to handle each chunk of the response
     * @param on_complete Optional callback function to handle completion
     * @param cancel_check Optional callback to check if the operation should be cancelled
     * @throws std::runtime_error If streaming is not supported or the request fails
     */
    void send_message_stream(const std::string& message,
                             stream_callback on_chunk,
                             completion_callback on_complete = nullptr,
                             progress_callback cancel_check = nullptr);

    /**
     * @brief Sends a message asynchronously
     * @param message The message to send
     * @return A future containing the response text
     */
    [[nodiscard]] std::future<std::string> send_message_async(const std::string& message);

    /**
     * @brief Sends the current context as a message and waits for a response
     * @param cancel_check Optional callback to check if the operation should be cancelled
     * @return The response text
     * @throws std::runtime_error If the request fails or the response cannot be parsed
     */
    [[nodiscard]] std::string send_message(progress_callback cancel_check = nullptr);

    /**
     * @brief Sends the current context as a message and streams the response
     * @param on_chunk Callback function to handle each chunk of the response
     * @param on_complete Optional callback function to handle completion
     * @param cancel_check Optional callback to check if the operation should be cancelled
     * @throws std::runtime_error If streaming is not supported or the request fails
     */
    void send_message_stream(stream_callback on_chunk,
                             completion_callback on_complete = nullptr,
                             progress_callback cancel_check = nullptr);

    /**
     * @brief Sends the current context as a message asynchronously
     * @return A future containing the response text
     */
    [[nodiscard]] std::future<std::string> send_message_async();

    /**
     * @brief Gets the underlying context for advanced usage
     * @return Reference to the general context
     */
    [[nodiscard]] general_context& get_context() noexcept { return *m_context; }

private:
    std::unique_ptr<general_context> m_context;
    std::unique_ptr<http_client> m_http_client;

    /**
     * @brief Ensures that the HTTP client is initialized
     */
    void ensure_http_client();

    /**
     * @brief Sends a request to the API
     * @param request The request to send
     * @param cancel_check Optional callback to check if the operation should be cancelled
     * @return The HTTP response
     */
    [[nodiscard]] http_response send_request(const nlohmann::json& request, progress_callback cancel_check = nullptr);
};

struct needs_schema {};
struct has_schema {};

template<typename SchemaState = needs_schema>
class chat_api_builder {
private:
    std::string m_schema_path;
    context_config m_config;
    std::string m_api_key;

public:
    // Only available when schema is not set
    template<typename T = SchemaState>
    std::enable_if_t<std::is_same_v<T, needs_schema>,
                     chat_api_builder<has_schema>>
    with_schema(const std::string& path) {
        chat_api_builder<has_schema> next;
        next.m_schema_path = path;
        next.m_config = m_config;
        next.m_api_key = m_api_key;
        return next;
    }

    // Available in any state
    chat_api_builder& with_config(const context_config& config) {
        m_config = config;
        return *this;
    }

    chat_api_builder& with_api_key(const std::string& key) {
        m_api_key = key;
        return *this;
    }

    // Only available when schema is set
    template<typename T = SchemaState>
    std::enable_if_t<std::is_same_v<T, has_schema>, std::unique_ptr<chat_api>>
    build() {
        auto context = std::make_unique<general_context>(m_schema_path, m_config);
        if (!m_api_key.empty()) {
            context->set_api_key(m_api_key);
        }
        return std::make_unique<chat_api>(std::move(context));
    }
};

/*
Usage:
auto api = chat_api_builder<>()
               .with_schema("schemas/claude.json")  // Required first!
               .with_api_key(key)                   // Optional, any order
               .with_config(config)                 // Optional, any order
               .build();                            // Only compiles if schema was set!

This won't compile:
auto api = chat_api_builder<>()
            .with_api_key(key)
            .build();  // Error: build() not available without schema!
*/

} // hyni
