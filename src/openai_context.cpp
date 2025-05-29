#include "openai_context.h"
#include "logger.h"
#include <algorithm>
#include <sstream>

namespace hyni {

constexpr size_t MAX_CONTENT_LENGTH = 20;

openai_context::openai_context()
    : m_api_url(GPT_API_URL)
    , m_max_context_length(MAX_CONTENT_LENGTH)
{
    LOG_DEBUG("OpenAI context created");
    LOG_INFO("API URL: " + m_api_url);
    LOG_INFO("Max context length: " + std::to_string(m_max_context_length));
}

void openai_context::add_user_message(const prompt &prompt) {
    logger::instance().log_section("OPENAI USER MESSAGE", {
        "Multi-turn: " + std::string(prompt.is_multi_turn ? "YES" : "NO"),
        "Question type: " + std::to_string(static_cast<int>(prompt.question_type)),
        "Has image: " + std::string(prompt.has_image() ? "YES" : "NO")
    });

    if (!prompt.is_multi_turn) {
        LOG_INFO("Clearing history (non-multi-turn)");
        m_history.clear();
    }

    nlohmann::json msg;
    msg["role"] = "user";
    msg["content"] = nlohmann::json::array();

    // Handle text content
    if (!prompt.user_message.empty()) {
        std::string content = m_history.empty() ? prompt.get_combined_prompt() : prompt.user_message;
        msg["content"].push_back({
            {"type", "text"},
            {"text", content}
        });
        LOG_DEBUG("Text content: " + logger::instance().truncate_text(content));
    }

    // Handle image content
    if (prompt.has_image()) {
        nlohmann::json image_content = {
            {"type", "image_url"},
            {"image_url", {
                {"url", "data:" + prompt.mime_type + ";base64," + prompt.image_base64}
            }}
        };
        msg["content"].push_back(image_content);
        LOG_DEBUG("Added image content (type: " + prompt.mime_type +
                  ", size: " + std::to_string(prompt.image_base64.size()) + " bytes)");
    }

    // Validate we have at least one content item
    if (msg["content"].empty()) {
        LOG_WARNING("Empty message content - adding placeholder");
        msg["content"].push_back({
            {"type", "text"},
            {"text", "[empty message]"}
        });
    }

    m_history.push_back(msg);
    trim_history();
    LOG_INFO("Message added. History now has " + std::to_string(m_history.size()) + " messages");
}


void openai_context::add_assistant_message(const std::string& message) {
    LOG_DEBUG("Adding assistant message: " + logger::instance().truncate_text(message));

    nlohmann::json msg;
    msg["role"] = "assistant";
    msg["content"] = nlohmann::json::array({
        {{"type", "text"}, {"text", message}}
    });

    m_history.push_back(msg);
    trim_history();
    LOG_INFO("History now contains " + std::to_string(m_history.size()) + " messages");
}

nlohmann::json openai_context::generate_payload(QUESTION_TYPE type) const {
    nlohmann::json payload;
    payload["model"] = GPT_MODEL_TYPE;
    payload["top_p"] = 1.0;  // Recommended default

    switch(type) {
    case QUESTION_TYPE::Behavioral:
        payload["max_tokens"] = 2048;  // STAR responses are structured
        payload["temperature"] = 0.8;   // Balanced creativity for stories
        break;

    case QUESTION_TYPE::SystemDesign:
        payload["max_tokens"] = 3072;   // More detailed explanations
        payload["temperature"] = 0.4;   // Less creative, more factual
        break;

    case QUESTION_TYPE::Coding:
        payload["max_tokens"] = 2048;   // Code + explanation needs space
        payload["temperature"] = 0.5;   // Low for accurate code
        break;

    case QUESTION_TYPE::General:  // General
        payload["max_tokens"] = 1024;
        payload["temperature"] = 0.7;
        break;

    default:
        throw std::invalid_argument("Unknown question type");
    }

    if (type == QUESTION_TYPE::Behavioral) {
        // For behavioral questions, ensure system message is first
        payload["messages"] = nlohmann::json::array();
        if (!m_history.empty() && m_history[0]["role"] == "system") {
            payload["messages"].push_back(m_history[0]);
        } else {
            payload["messages"].push_back({
                {"role", "system"},
                {"content", BEHAVIORAL_SYSPROMPT}
            });
        }
        // Add all subsequent messages
        size_t start_idx = (!m_history.empty() && m_history[0]["role"] == "system") ? 1 : 0;
        for (size_t i = start_idx; i < m_history.size(); ++i) {
            payload["messages"].push_back(m_history[i]);
        }
    }
    else if (type == QUESTION_TYPE::SystemDesign) {
        payload["messages"] = nlohmann::json::array();
        payload["messages"].push_back({
            {"role", "system"},
            {"content", SYSTEM_DESIGN_SYSPROMPT}
        });
        for (const auto& msg : m_history) {
            if (msg["role"] != "system") {
                payload["messages"].push_back(msg);
            }
        }
    }
    else if (type == QUESTION_TYPE::SystemDesign) {
        payload["messages"] = nlohmann::json::array();
        payload["messages"].push_back({
            {"role", "system"},
            {"content", GENERAL_SYSPROMPT}
        });
        for (const auto& msg : m_history) {
            if (msg["role"] != "system") {
                payload["messages"].push_back(msg);
            }
        }
    }
    else {
        // Standard conversation flow
        payload["messages"] = m_history;
    }

    LOG_DEBUG("Complete payload:\n" + payload.dump(2));

    return payload;
}

void openai_context::process_response(const nlohmann::json& response) {
    logger::instance().log_section("OPENAI RESPONSE", {
        "Processing API response",
        "Response keys: " + logger::instance().get_json_keys(response)
    });

    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& choice = response["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            std::string content;

            // Handle both formats:
            // 1. New format: {"content": [{"type": "text", "text": "..."}]}
            // 2. Old format: {"content": "..."}
            if (choice["message"]["content"].is_array()) {
                for (const auto& item : choice["message"]["content"]) {
                    if (item["type"] == "text" && item.contains("text")) {
                        content += item["text"].get<std::string>();
                    }
                }
            } else {
                content = choice["message"]["content"].get<std::string>();
            }

            if (!content.empty()) {
                LOG_INFO("Extracted assistant response (" +
                         std::to_string(content.size()) + " characters)");
                add_assistant_message(content);
            } else {
                LOG_WARNING("Empty content in response");
            }
        } else {
            LOG_WARNING("Response missing message/content in choice");
            LOG_DEBUG("Choice object dump:\n" + choice.dump(2));
        }
    } else {
        LOG_ERROR("Invalid response format - missing choices");
        LOG_DEBUG("Full response dump:\n" + response.dump(2));
    }
}

void openai_context::set_max_context_length(size_t length) {
    size_t new_length = std::max(size_t(1), length);
    LOG_INFO("Setting max context length: " + std::to_string(new_length) +
             " (was " + std::to_string(m_max_context_length) + ")");
    m_max_context_length = new_length;
    trim_history();
}

void openai_context::log_message_history() const {
    if (!logger::instance().is_enabled()) return;

    std::vector<std::string> messages;
    messages.push_back("OPENAI CONVERSATION HISTORY (" +
                       std::to_string(m_history.size()) + " messages)");

    for (size_t i = 0; i < m_history.size(); ++i) {
        const auto& msg = m_history[i];
        std::string role = msg["role"].get<std::string>();

        std::stringstream entry;
        entry << "Message " << i << " - Role: " << role;

        if (msg["content"].is_array()) {
            entry << "\n  Content items: " << msg["content"].size();
            for (const auto& content_item : msg["content"]) {
                std::string type = content_item["type"].get<std::string>();

                if (type == "text") {
                    std::string text = content_item["text"].get<std::string>();
                    entry << "\n  - Text: " << logger::instance().truncate_text(text);
                }
                else if (type == "image_url") {
                    std::string url = content_item["image_url"]["url"].get<std::string>();
                    // Extract mime type from data URL
                    std::string mime_type = "unknown";
                    size_t colon_pos = url.find(':');
                    size_t semicolon_pos = url.find(';');
                    if (colon_pos != std::string::npos && semicolon_pos != std::string::npos) {
                        mime_type = url.substr(colon_pos + 1, semicolon_pos - colon_pos - 1);
                    }
                    entry << "\n  - Image: " << mime_type << " (data URL truncated)";
                }
                else {
                    entry << "\n  - Unknown content type: " << type;
                }
            }
        } else {
            // Legacy format fallback (shouldn't happen with current implementation)
            entry << "\n  [Legacy Content Format]: "
                  << logger::instance().truncate_text(msg["content"].get<std::string>());
        }

        messages.push_back(entry.str());
    }

    logger::instance().log_section("OPENAI MESSAGE HISTORY", messages);
}

void openai_context::trim_history() {
    if (m_history.size() <= m_max_context_length) return;

    std::vector<std::string> log_messages = {
        "Trimming history from " + std::to_string(m_history.size()) +
        " to " + std::to_string(m_max_context_length) + " messages"
    };

    // Preserve system message if exists
    bool has_system = !m_history.empty() && m_history[0]["role"] == "system";
    size_t preserve = has_system ? 1 : 0;

    // Calculate how many messages to keep (preserving system message)
    size_t keep = std::min(m_max_context_length + preserve, m_history.size());
    size_t remove = m_history.size() - keep;

    if (remove > 0) {
        log_messages.push_back("Removing " + std::to_string(remove) +
            " messages" + (has_system ? " (preserving system)" : ""));
        m_history.erase(m_history.begin() + preserve,
                       m_history.begin() + preserve + remove);
    }
    logger::instance().log_section("HISTORY TRIMMING", log_messages);
}

} // namespace hyni
