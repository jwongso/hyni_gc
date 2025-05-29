#ifndef DEEPSEEK_CONTEXT_H
#define DEEPSEEK_CONTEXT_H

#include "config.h"
#include "model_context.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace hyni {

class deepseek_context : public model_context {
public:
    deepseek_context();

    void configure(const std::string& api_key,
                   const std::string& api_url = "",
                   const std::string& model = "") override {
        m_api_key = api_key;
        m_api_url = api_url.empty() ? DS_API_URL : api_url;
        m_model = model.empty() ? DS_CODING_MODEL_TYPE : model;
    }

    const std::string& get_api_key() const override { return m_api_key; }
    const std::string& get_api_url() const override { return m_api_url; }
    const std::string& get_model() const override { return m_model; }
    API_PROVIDER get_llm_provider() const override { return API_PROVIDER::DeepSeek; }

    // Context management
    void add_user_message(const prompt& prompt) override;
    void add_assistant_message(const std::string& message) override;

    // Payload generation (DeepSeek-specific format)
    nlohmann::json generate_payload(QUESTION_TYPE type) const override;

    // Response processing
    void process_response(const nlohmann::json& response) override;

    // Context control
    size_t current_length() const override { return m_history.size(); }
    void set_max_context_length(size_t length) override;
    void log_message_history() const override;

private:
    std::string m_api_key;
    std::string m_api_url;
    std::string m_model;
    std::vector<nlohmann::json> m_history;
    size_t m_max_context_length;

    void trim_history();
};

} // namespace hyni

#endif // DEEPSEEK_CONTEXT_H
