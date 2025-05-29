#ifndef MODEL_CONTEXT_H
#define MODEL_CONTEXT_H

#include <string>
#include <nlohmann/json.hpp>

namespace hyni {

enum class API_PROVIDER {
    OpenAI,
    DeepSeek,
    ClaudeAI,
    Unknown
};

enum class QUESTION_TYPE {
    General,
    Behavioral,
    SystemDesign,
    Coding
};

struct prompt {
    std::string user_message;
    std::string extended_message;
    std::string system_message;
    QUESTION_TYPE question_type;
    bool is_multi_turn = false;
    std::string image_base64;  // Base64 encoded image string, if any
    std::string mime_type = "image/png";  // Default mime type for image

    // Helper function to check if an image is included
    bool has_image() const {
        return !image_base64.empty() && !mime_type.empty();
    }

    // Combine user message with extended message if multi-turn is enabled
    std::string get_combined_prompt() const {
        return user_message + extended_message;
    }
};

class model_context {
public:
    virtual ~model_context() = default;

    // LLM configuration
    virtual void configure(const std::string& api_key,
                           const std::string& api_url = "",
                           const std::string& model = "") = 0;

    virtual const std::string& get_api_key() const = 0;
    virtual const std::string& get_api_url() const = 0;
    virtual const std::string& get_model() const = 0;
    virtual API_PROVIDER get_llm_provider() const = 0;

    // Context management
    virtual void add_user_message(const prompt& prompt) = 0;
    virtual void add_assistant_message(const std::string& message) = 0;

    // Payload generation
    virtual nlohmann::json generate_payload(QUESTION_TYPE type) const = 0;

    // Response processing
    virtual void process_response(const nlohmann::json& response) = 0;

    // Utility
    virtual size_t current_length() const = 0;
    virtual void set_max_context_length(size_t length) = 0;
    virtual void log_message_history() const = 0;
};

} // namespace hyni

#endif // MODEL_CONTEXT_H
