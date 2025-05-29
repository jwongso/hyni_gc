#include "deepseek_context.h"
#include "config.h"
#include "logger.h"
#include <sstream>

constexpr int MAX_CONTENT_LENGTH = 8;

namespace hyni {

deepseek_context::deepseek_context()
    : m_max_context_length(MAX_CONTENT_LENGTH) {
    LOG_DEBUG("DeepSeek context created with max length: " + std::to_string(m_max_context_length));
} // Slightly smaller default for DeepSeek

void deepseek_context::add_user_message(const prompt& prompt) {
    logger::instance().log_section("DEEPSEEK USER MESSAGE", {
        "Multi-turn: " + std::string(prompt.is_multi_turn ? "YES" : "NO"),
        "Question type: " + std::to_string(static_cast<int>(prompt.question_type)),
        "Using: " + std::string(m_history.empty() ? "combined prompt" : "user message only")
    });
    if (!prompt.is_multi_turn) {
        LOG_INFO("Clearing history (non-multi-turn)");
        m_history.clear();
    }

    nlohmann::json msg;
    msg["role"] = "user";
    msg["content"] = m_history.empty() ?
                         prompt.get_combined_prompt() : prompt.user_message;
    LOG_DEBUG("Adding user message: " + logger::instance().truncate_text(msg["content"].get<std::string>()));
    m_history.push_back(msg);
    trim_history();
}

void deepseek_context::add_assistant_message(const std::string& message) {
    LOG_DEBUG("Adding assistant message: " + logger::instance().truncate_text(message));
    nlohmann::json msg;
    msg["role"] = "assistant";
    msg["content"] = message;
    m_history.push_back(msg);
    trim_history();
    LOG_INFO("History now contains " + std::to_string(m_history.size()) + " messages");
}

nlohmann::json deepseek_context::generate_payload(QUESTION_TYPE type) const {
    nlohmann::json payload;

    // Model selection based on question type
    payload["model"] = (type == QUESTION_TYPE::Behavioral || type == QUESTION_TYPE::General)
        ? DS_GENERAL_MODEL_TYPE
        : DS_CODING_MODEL_TYPE;
    payload["max_tokens"] = 2048;
    payload["temperature"] = 0.7;
    payload["stream"] = false;

    switch(type) {
    case QUESTION_TYPE::Behavioral:
        payload["max_tokens"] = 2048;  // Structured STAR responses
        payload["temperature"] = 0.8;   // More creative storytelling
        break;

    case QUESTION_TYPE::SystemDesign:
        payload["max_tokens"] = 3072;   // Detailed architecture needs
        payload["temperature"] = 0.6;   // Balanced accuracy/creativity
        break;

    case QUESTION_TYPE::Coding:
        payload["temperature"] = 0.4;   // Lower for precise code
        break;

    case QUESTION_TYPE::General:  // General
        payload["max_tokens"] = 1024;
        payload["temperature"] = 0.7;
        break;

    default:
        throw std::invalid_argument("Unknown question type");
    }

    if (type == QUESTION_TYPE::Behavioral) {
        std::ostringstream oss;
        oss << "[INSTRUCTIONS]\n" << BEHAVIORAL_SYSPROMPT
            << "\n\n[CONVERSATION HISTORY]\n";

        for (const auto& msg : m_history) {
            oss << msg["role"].get<std::string>() << ": "
                << msg["content"].get<std::string>() << "\n";
        }

        payload["messages"] = {{{"role", "user"}, {"content", oss.str()}}};
    } else {
        // Standard questions use direct history
        payload["messages"] = m_history;
    }

    LOG_DEBUG("Complete payload:\n" + payload.dump(2));

    return payload;
}

void deepseek_context::process_response(const nlohmann::json& response) {
    logger::instance().log_section("DEEPSEEK RESPONSE", {
        "Processing API response",
        "Response keys: " + logger::instance().get_json_keys(response)
    });
    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& choice = response["choices"][0];
        if (choice.contains("message") && choice["message"].contains("content")) {
            std::string content = choice["message"]["content"].get<std::string>();
            LOG_INFO("Extracted assistant response (" + std::to_string(content.size()) + " characters)");
            add_assistant_message(choice["message"]["content"].get<std::string>());
        } else {
            LOG_WARNING("Response missing message/content in choice");
        }
    }
    else {
        LOG_ERROR("Invalid response format - missing choices");
        LOG_DEBUG("Full response dump:\n" + response.dump(2));
    }
}

void deepseek_context::set_max_context_length(size_t length) {
    size_t new_length = std::max(size_t(1), length);
    LOG_INFO("Setting max context length: " + std::to_string(new_length) +
             " (was " + std::to_string(m_max_context_length) + ")");
    m_max_context_length = new_length;
    trim_history();
}

void deepseek_context::log_message_history() const {
    if (!logger::instance().is_enabled()) return;

    std::vector<std::string> messages;
    messages.push_back("DEEPSEEK CONVERSATION HISTORY (" +
                       std::to_string(m_history.size()) + " messages)");

    for (size_t i = 0; i < m_history.size(); ++i) {
        const auto& msg = m_history[i];
        std::string role = msg["role"].get<std::string>();

        std::stringstream entry;
        entry << "Message " << i << " - Role: " << role;

        if (msg["content"].is_array()) {
            // Handle multimodal content (if supported)
            entry << "\n  Content items: " << msg["content"].size();
            for (const auto& content_item : msg["content"]) {
                if (content_item.is_object() && content_item.contains("type")) {
                    std::string type = content_item["type"].get<std::string>();
                    if (type == "text" && content_item.contains("text")) {
                        entry << "\n  - Text: " << logger::instance().truncate_text(content_item["text"].get<std::string>());
                    } else if (type == "image" && content_item.contains("source")) {
                        entry << "\n  - Image: " << content_item["source"]["media_type"].get<std::string>()
                        << " (base64 data)";
                    }
                } else {
                    // Fallback for simple text in array
                    entry << "\n  - Text: " << logger::instance().truncate_text(content_item.get<std::string>());
                }
            }
        } else if (msg["content"].is_string()) {
            // Standard text message
            entry << "\n  Content: " << logger::instance().truncate_text(msg["content"].get<std::string>());
        } else {
            entry << "\n  [Unknown content format]";
        }

        // Special handling for behavioral system messages
        if (role == "system" && i == 0 &&
            msg["content"].get<std::string>().find(BEHAVIORAL_SYSPROMPT) != std::string::npos) {
            entry << "\n  [STAR Behavioral System Prompt]";
        }

        messages.push_back(entry.str());
    }

    logger::instance().log_section("DEEPSEEK MESSAGE HISTORY", messages);
}

void deepseek_context::trim_history() {
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
