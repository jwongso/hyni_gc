#include "chat_api.h"
#include "http_client.h"
#include "http_client_factory.h"

namespace hyni {

chat_api::chat_api(std::unique_ptr<general_context> context)
    : m_context(std::move(context)) {
    ensure_http_client();
}

std::string chat_api::send_message(const std::string& message, progress_callback cancel_check) {
    // Clear previous messages if needed (depending on your use case)
    m_context->clear_user_messages();
    m_context->add_user_message(message);

    auto request = m_context->build_request();
    auto response = m_http_client->post(m_context->get_endpoint(), request, cancel_check);

    if (!response.success) {
        throw std::runtime_error("API request failed: " + response.error_message);
    }

    try {
        auto json_response = nlohmann::json::parse(response.body);
        return m_context->extract_text_response(json_response);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse API response: " + std::string(e.what()));
    }
}

void chat_api::send_message_stream(const std::string& message,
                                 stream_callback on_chunk,
                                 completion_callback on_complete,
                                 progress_callback cancel_check) {
    if (!m_context->supports_streaming()) {
        throw std::runtime_error("Streaming is not supported by this provider");
    }

    m_context->clear_user_messages();
    m_context->add_user_message(message);

    // Enable streaming in the request
    auto request = m_context->build_request();
    request["stream"] = true;

    m_http_client->post_stream(
        m_context->get_endpoint(),
        request,
        [on_chunk, this](const std::string& chunk) {
            try {
                // Handle streaming chunks (implementation depends on provider)
                if (!chunk.empty() && chunk != "data: [DONE]") {
                    size_t pos = chunk.find("data: ");
                    if (pos != std::string::npos) {
                        std::string json_str = chunk.substr(pos + 6);
                        if (!json_str.empty()) {
                            auto json_chunk = nlohmann::json::parse(json_str);
                            std::string content = m_context->extract_text_response(json_chunk);
                            on_chunk(content);
                        }
                    }
                }
            } catch (...) {
                // Handle parse errors silently in streaming
            }
        },
        [on_complete](const http_response& response) {
            if (on_complete) {
                on_complete(response);
            }
        },
        cancel_check
    );
}

std::future<std::string> chat_api::send_message_async(const std::string& message) {
    ensure_http_client();

    return std::async(std::launch::async, [this, message]() {
        return send_message(message);
    });
}

std::string chat_api::send_message(progress_callback cancel_check) {
    ensure_http_client();

    // Validate we have at least one user message
    bool has_user_message = false;
    for (const auto& msg : m_context->get_messages()) {
        if (msg["role"] == "user") {
            has_user_message = true;
            break;
        }
    }

    if (!has_user_message) {
        throw std::runtime_error("No user message found in context");
    }

    auto request = m_context->build_request();
    m_http_client->set_headers(m_context->get_headers());
    auto response = m_http_client->post(m_context->get_endpoint(), request, cancel_check);

    if (!response.success) {
        throw std::runtime_error("API request failed: " + response.error_message);
    }

    try {
        auto json_response = nlohmann::json::parse(response.body);
        return m_context->extract_text_response(json_response);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse API response: " + std::string(e.what()));
    }
}

void chat_api::send_message_stream(stream_callback on_chunk,
                                   completion_callback on_complete,
                                   progress_callback cancel_check) {
    if (!m_context->supports_streaming()) {
        throw std::runtime_error("Streaming is not supported by this provider");
    }

    // Validate we have at least one user message
    bool has_user_message = false;
    for (const auto& msg : m_context->get_messages()) {
        if (msg["role"] == "user") {
            has_user_message = true;
            break;
        }
    }

    if (!has_user_message) {
        throw std::runtime_error("No user message found in context");
    }

    // Build request with streaming enabled
    auto request = m_context->build_request(true);
    m_http_client->set_headers(m_context->get_headers());

    m_http_client->post_stream(
        m_context->get_endpoint(),
        request,

        // on_chunk: handle each streamed response chunk
        [on_chunk, this](const std::string& chunk) {
            try {
                std::istringstream stream(chunk);
                std::string line;
                while (std::getline(stream, line)) {
                    if (line.rfind("data: ", 0) == 0) {  // line starts with "data: "
                        std::string json_str = line.substr(6);
                        if (json_str == "[DONE]") {
                            break; // Optional: stop processing after [DONE]
                        }

                        if (!json_str.empty()) {
                            auto json_chunk = nlohmann::json::parse(json_str, nullptr, false);
                            if (!json_chunk.is_discarded()) {
                                std::string content = m_context->extract_text_response(json_chunk);
                                if (!content.empty()) {
                                    on_chunk(content);
                                }
                            }
                        }
                    }
                }
            } catch (...) {
                // Silent fail: you may log this if needed
            }
        },

        // on_complete: mark streaming as complete
        [on_complete](const http_response& response) {
            if (on_complete) {
                on_complete(response);
            }
        },

        // cancel check
        cancel_check
        );
}

std::future<std::string> chat_api::send_message_async() {
    return std::async(std::launch::async, [this]() {
        return send_message();
    });
}

void chat_api::ensure_http_client() {
    if (!m_http_client) {
        m_http_client = http_client_factory::create_http_client(*m_context);
    }
}

http_response chat_api::send_request(const nlohmann::json& request, progress_callback cancel_check) {
    return m_http_client->post(m_context->get_endpoint(), request, cancel_check);
}

} // namespace hyni
