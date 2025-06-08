#pragma once

#include <memory>
#include "http_client.h"
#include "general_context.h"

namespace hyni
{

class chat_api {
public:
    explicit chat_api(std::unique_ptr<general_context> context);

    // Synchronous chat
    std::string send_message(const std::string& message, progress_callback cancel_check = nullptr);

    // Streaming chat
    void send_message_stream(const std::string& message,
                             stream_callback on_chunk,
                             completion_callback on_complete = nullptr,
                             progress_callback cancel_check = nullptr);

    // Async chat
    std::future<std::string> send_message_async(const std::string& message);

    std::string send_message(progress_callback cancel_check = nullptr);
    void send_message_stream(stream_callback on_chunk,
                             completion_callback on_complete = nullptr,
                             progress_callback cancel_check = nullptr);
    std::future<std::string> send_message_async();

    // Access to underlying context for advanced usage
    general_context& get_context() { return *m_context; }

private:
    std::unique_ptr<general_context> m_context;
    std::unique_ptr<http_client> m_http_client;

    void ensure_http_client();
    http_response send_request(const nlohmann::json& request, progress_callback cancel_check = nullptr);
};

} // hyni
