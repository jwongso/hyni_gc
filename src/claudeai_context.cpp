#include "claudeai_context.h"
#include "logger.h"
#include <algorithm>
#include <sstream>

namespace hyni {

constexpr size_t MAX_CONTENT_LENGTH = 20;

void append_to_message(nlohmann::json& msg, const std::string& text) {
    if (msg["content"].is_array()) {
        for (auto& content_item : msg["content"]) {
            if (content_item["type"] == "text") {
                content_item["text"] = content_item["text"].get<std::string>() + text;
                break;
            }
        }
    } else if (msg["content"].is_string()) {
        msg["content"] = msg["content"].get<std::string>() + text;
    }
}

claudeai_context::claudeai_context()
    : m_max_context_length(MAX_CONTENT_LENGTH) {}

void claudeai_context::add_user_message(const prompt &prompt) {
    if (logger::instance().is_enabled()) {
        std::vector<std::string> messages;
        messages.push_back("ADDING USER MESSAGE");
        messages.push_back("Multi-turn: " + std::string(prompt.is_multi_turn ? "YES" : "NO"));
        messages.push_back("Question type: " + std::to_string(static_cast<int>(prompt.question_type)));
        messages.push_back("Has image: " + std::string(prompt.has_image() ? "YES" : "NO"));

        logger::instance().log_section("ADDING USER MESSAGE", messages);
    }

    if (!prompt.is_multi_turn) {
        LOG_INFO("Clearing history (non-multi-turn)");
        m_history.clear();
    }

    nlohmann::json msg;
    msg["role"] = "user";
    msg["content"] = nlohmann::json::array();

    // Add text part if message is not empty
    if (!prompt.user_message.empty()) {
        bool using_combined = m_history.empty();
        std::string message_text = using_combined ? prompt.get_combined_prompt() : prompt.user_message;
        LOG_INFO(std::string("Using ") + (using_combined ? "combined prompt" : "user message only"));
        LOG_INFO("Text: " + logger::instance().truncate_text(message_text));
        // If this is a first message (no history) then use combined prompt otherwise only the user message.
        msg["content"].push_back({
            {"type", "text"},
            {"text", message_text}
        });
    }

    // Add image part if there is an image
    if (prompt.has_image()) {
        LOG_INFO("Adding image of type: " + prompt.mime_type);
        LOG_INFO("Image data size: " + std::to_string(prompt.image_base64.size()) + " bytes");
        msg["content"].push_back({
            {"type", "image"},
            {"source", {
                           {"type", "base64"},
                           {"media_type", prompt.mime_type},
                           {"data", prompt.image_base64}
                       }}
        });
    }

    m_history.push_back(msg);
    trim_history();
    LOG_INFO("Message added. History now has " + std::to_string(m_history.size()) + " messages");
}

void claudeai_context::add_assistant_message(const std::string& message) {
    LOG_DEBUG("Adding assistant message: " + logger::instance().truncate_text(message));
    nlohmann::json msg;
    msg["role"] = "assistant";
    msg["content"] = nlohmann::json::array();
    msg["content"].push_back({{"type", "text"}, {"text", message}});
    m_history.push_back(msg);
    trim_history();
    LOG_INFO("History now contains " + std::to_string(m_history.size()) + " messages");
}

nlohmann::json claudeai_context::generate_payload(QUESTION_TYPE type) const {
    nlohmann::json payload;
    payload["model"] = CL_MODEL_TYPE;
    payload["messages"] = nlohmann::json::array();

    // Default parameters (for General/Coding)
    payload["max_tokens"] = 2048;
    payload["temperature"] = 0.7;

    // Question-type specific tuning
    switch(type) {
    case QUESTION_TYPE::Behavioral:
        payload["max_tokens"] = 2048;
        payload["temperature"] = 0.8;  // More creative responses
        payload["system"] = BEHAVIORAL_SYSPROMPT;
        break;

    case QUESTION_TYPE::SystemDesign:
        payload["max_tokens"] = 3072;  // More space for architecture details
        payload["temperature"] = 0.5;
        payload["system"] = SYSTEM_DESIGN_SYSPROMPT;
        break;

    case QUESTION_TYPE::Coding:
        payload["temperature"] = 0.5;  // Lower for precise code
        break;

    case QUESTION_TYPE::General:  // General
        payload["max_tokens"] = 1024;
        payload["temperature"] = 0.7;
        payload["system"] = GENERAL_SYSPROMPT;
        break;

    default:
        LOG_ERROR("Unknown question type: " + std::to_string(static_cast<int>(type)));
        throw std::invalid_argument("Unknown question type");
    }

    // Convert and add message history
    for (const auto& msg : m_history) {
        payload["messages"].push_back(convert_to_claude_format(msg));
    }

    LOG_DEBUG("Generated payload:\n" + payload.dump(2));

    return payload;
}

void claudeai_context::process_response(const nlohmann::json& response) {
    LOG_DEBUG("Processing API response");
    if (response.contains("content")) {
        std::string full_content;
        size_t content_items = 0;
        for (const auto& content_item : response["content"]) {
            if (content_item["type"] == "text" && content_item.contains("text")) {
                std::string text = content_item["text"].get<std::string>();
                full_content += text;
                content_items++;
                LOG_DEBUG("Processing text content item (" +
                          std::to_string(text.size()) + " bytes)");
            }
        }
        if (!full_content.empty()) {
            LOG_INFO("Successfully processed " + std::to_string(content_items) +
                     " content items (" + std::to_string(full_content.size()) +
                     " total characters)");
            // Store in standard format, conversion happens during send
            nlohmann::json msg;
            msg["role"] = "assistant";
            msg["content"] = full_content;
            m_history.push_back(msg);
            trim_history();

            LOG_DEBUG("History now contains " + std::to_string(m_history.size()) +
                      " messages");
        }
    }
    else {
        LOG_ERROR("Response missing 'content' field");
        LOG_DEBUG("Full response dump:\n" + response.dump(2));
    }
}

void claudeai_context::set_max_context_length(size_t length) {
    m_max_context_length = std::min(static_cast<int>(std::max(size_t(1), length)), 50);
    trim_history();
}

void claudeai_context::trim_history() {
    if (m_history.size() <= m_max_context_length) return;

    std::vector<std::string> trim_messages = {
        "TRIMMING HISTORY",
        "Current history size: " + std::to_string(m_history.size()),
        "Max allowed: " + std::to_string(m_max_context_length)
    };

    // Special handling to preserve system message
    bool has_system = !m_history.empty() && m_history[0]["role"] == "system";
    size_t preserve = has_system ? 1 : 0;
    size_t keep = std::min(m_max_context_length + preserve, m_history.size());
    size_t remove = m_history.size() - keep;

    if (remove > 0) {
        trim_messages.push_back("Removing " + std::to_string(remove) +
                                " messages, preserving system message: " +
                                (has_system ? "YES" : "NO"));

        m_history.erase(m_history.begin() + preserve,
                        m_history.begin() + preserve + remove);

        trim_messages.push_back("After trimming, history size: " +
                                std::to_string(m_history.size()));
    }

    logger::instance().log_section("HISTORY TRIMMING", trim_messages);
}

void claudeai_context::log_message_history() const {
    if (!logger::instance().is_enabled()) return;

    std::vector<std::string> messages;
    messages.push_back("CLAUDE CONVERSATION HISTORY (" + std::to_string(m_history.size()) + " messages)");

    for (size_t i = 0; i < m_history.size(); ++i) {
        const auto& msg = m_history[i];
        std::string role = msg["role"].get<std::string>();

        std::stringstream entry;
        entry << "Message " << i << " - Role: " << role;

        if (msg["content"].is_array()) {
            entry << "\n  Content type: array with " << msg["content"].size() << " items";
            for (const auto& content_item : msg["content"]) {
                if (content_item["type"] == "text") {
                    std::string text = content_item["text"].get<std::string>();
                    entry << "\n  - Text: " << logger::instance().truncate_text(text);
                } else if (content_item["type"] == "image") {
                    entry << "\n  - Image: " << content_item["source"]["media_type"].get<std::string>()
                    << " (base64 data truncated)";
                }
            }
        } else if (msg["content"].is_string()) {
            entry << "\n  Content: " << logger::instance().truncate_text(msg["content"].get<std::string>());
        }

        messages.push_back(entry.str());
    }

    logger::instance().log_section("CLAUDE CONVERSATION HISTORY", messages);
}

nlohmann::json claudeai_context::convert_to_claude_format(const nlohmann::json& msg) const {
    nlohmann::json claude_msg;
    claude_msg["role"] = msg["role"];

    if (msg["role"] == "system") {
        // Convert system messages to user messages with instructions
        claude_msg["role"] = "user";
        claude_msg["content"] = nlohmann::json::array({
            {{"type", "text"}, {"text", msg["content"]}}
        });
    } else {
        // Regular messages (including images)
        claude_msg["content"] = msg.contains("content") && msg["content"].is_array()
                                    ? msg["content"]  // Already in Claude format (for images)
                                    : nlohmann::json::array({
                                          {{"type", "text"}, {"text", msg["content"]}}
                                      });
    }

    return claude_msg;
}

} // namespace hyni
