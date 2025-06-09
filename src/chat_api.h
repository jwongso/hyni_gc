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

} // hyni
