#include "chat_api.h"
#include "config.h"
#include "openai_context.h"
#include "deepseek_context.h"
#include "claudeai_context.h"
#include <fstream>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace hyni;

std::unique_ptr<model_context> chat_api::create_context(API_PROVIDER provider) {
    switch(provider) {
    case API_PROVIDER::OpenAI: return std::make_unique<openai_context>();
    case API_PROVIDER::DeepSeek: return std::make_unique<deepseek_context>();
    case API_PROVIDER::ClaudeAI: return std::make_unique<claudeai_context>();
    default: throw std::runtime_error("Unsupported provider");
    }
}

std::string get_home_dir() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home ? std::string(home) : "";
}

const std::string& hyni::get_commit_hash() {
    static const std::string hash = HYNI_COMMIT_HASH;
    return hash;
}

std::unordered_map<std::string, std::string> parse_hynirc(const std::string& file_path) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(file_path);
    std::string line;

    while (std::getline(file, line)) {
        size_t delimiter_pos = line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            config[key] = value;
        }
    }

    return config;
}

API_PROVIDER chat_api::detect_api_provider(const std::string& url) {
    if (url.find("openai.com") != std::string::npos) {
        return API_PROVIDER::OpenAI;
    } else if (url.find("deepseek.com") != std::string::npos) {
        return API_PROVIDER::DeepSeek;
    } else if (url.find("anthropic.com") != std::string::npos) {
        return API_PROVIDER::ClaudeAI;
    } else {
        return API_PROVIDER::Unknown;
    }
}

std::string get_api_key(API_PROVIDER provider) {
    if (provider == API_PROVIDER::OpenAI) {
        if (const char* api_key_cstr = std::getenv("OA_API_KEY")) {
            return std::string(api_key_cstr);
        }
    } else if (provider == API_PROVIDER::DeepSeek) {
        if (const char* api_key_cstr = std::getenv("DS_API_KEY")) {
            return std::string(api_key_cstr);
        }
    } else if (provider == API_PROVIDER::ClaudeAI) {
        if (const char* api_key_cstr = std::getenv("CL_API_KEY")) {
            return std::string(api_key_cstr);
        }
    }

    std::string home_dir = get_home_dir();
    if (!home_dir.empty()) {
        fs::path rc_path = fs::path(home_dir) / ".hynirc";

        if (fs::exists(rc_path)) {
            auto config = parse_hynirc(rc_path.string());

            if (provider == API_PROVIDER::OpenAI) {
                auto it = config.find("OA_API_KEY");
                if (it != config.end()) {
                    return it->second;
                }
            }
            else if (provider == API_PROVIDER::DeepSeek) {
                auto it = config.find("DS_API_KEY");
                if (it != config.end()) {
                    return it->second;
                }
            }
            else if (provider == API_PROVIDER::ClaudeAI) {
                auto it = config.find("CL_API_KEY");
                if (it != config.end()) {
                    return it->second;
                }
            }
        }
    }

    return {};
}

chat_api::chat_api(const std::string& url)
    : m_curl_handle(curl_easy_init())
    , m_context(create_context(detect_api_provider(url)))
{
    if (!m_curl_handle) {
        throw std::runtime_error("Failed to initialize CURL handle");
    }

    context().configure(get_api_key(context().get_llm_provider()));
}

chat_api::chat_api(API_PROVIDER provider)
    : m_curl_handle(curl_easy_init())
    , m_context(create_context(provider))
{
    if (!m_curl_handle) {
        throw std::runtime_error("Failed to initialize CURL handle");
    }

    context().configure(get_api_key(context().get_llm_provider()));
}

chat_api::~chat_api() {
    cancel();
    if (m_curl_handle) {
        curl_easy_cleanup(m_curl_handle);
        m_curl_handle = nullptr;
    }
}

void chat_api::cancel() {
    m_cancel_flag.store(true);
    if (m_curl_handle) {
        curl_easy_pause(m_curl_handle, CURLPAUSE_ALL);
    }
}

bool chat_api::has_api_key() const {
    return !context().get_api_key().empty();
}

void chat_api::set_api_key(const std::string& api_key) {
    context().configure(api_key, context().get_api_url(), context().get_model());
}

API_PROVIDER chat_api::get_api_provider() const {
    return context().get_llm_provider();
}

size_t chat_api::write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    if (!s) return 0;
    size_t new_length = size * nmemb;
    try {
        s->append(static_cast<char*>(contents), new_length);
    } catch (...) {
        return 0;
    }
    return new_length;
}

int chat_api::progress_callback(void* clientp, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                                curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto api = static_cast<chat_api*>(clientp);
    return api->m_cancel_flag.load() ? 1 : 0;
}

curl_slist* chat_api::setup_curl_options(CURL* curl,
                                         const std::string& payload_str) {
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (m_context->get_llm_provider() == API_PROVIDER::ClaudeAI) {
        headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
        headers = curl_slist_append(headers, ("x-api-key: " + context().get_api_key()).c_str());
    }
    else {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + context().get_api_key()).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, context().get_api_url().c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 90L);

    return headers;
}

std::string chat_api::send(const hyni::prompt &prompt,
                           const std::function<bool()>& should_cancel) {

    context().add_user_message(prompt);
    json payload = context().generate_payload(prompt.question_type);

    std::string payload_str = payload.dump();
    struct curl_slist* headers = setup_curl_options(m_curl_handle,
                                                    payload_str);
    std::string read_buffer;
    curl_easy_setopt(m_curl_handle, CURLOPT_WRITEDATA, &read_buffer);

    CURLM* multi_handle = curl_multi_init();
    curl_multi_add_handle(multi_handle, m_curl_handle);

    int still_running = 1;
    while (still_running && !m_cancel_flag.load() && !should_cancel()) {
        CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
        if (mc) {
            break;
        }
        curl_multi_wait(multi_handle, nullptr, 0, 100, nullptr);
    }

    curl_slist_free_all(headers);
    curl_multi_remove_handle(multi_handle, m_curl_handle);
    curl_multi_cleanup(multi_handle);

    if (m_cancel_flag.load() || should_cancel()) {
        throw std::runtime_error("Image request cancelled");
    }

    return read_buffer;
}

chat_api::api_response chat_api::get_assistant_reply(const std::string& json_response) {
    try {
        json response_json = json::parse(json_response);

        // Check for error first
        if (response_json.contains("error")) {
            std::string error_msg = response_json["error"]["message"].get<std::string>();
            return api_response(false, "", error_msg);
        }

        // Process response in context first (maintains conversation history)
        context().process_response(response_json);

        // Extract content based on provider
        std::string content;
        bool success = false;

        switch (context().get_llm_provider()) {
        case API_PROVIDER::ClaudeAI: {
            // Claude format: {"content": [{"type": "text", "text": "..."}]}
            if (response_json.contains("content") && response_json["content"].is_array()) {
                for (const auto& item : response_json["content"]) {
                    if (item.is_object() &&
                        item["type"] == "text" &&
                        item.contains("text")) {
                        content += item["text"].get<std::string>();
                        success = true;
                    }
                }
            }
            break;
        }
        case API_PROVIDER::DeepSeek:
        case API_PROVIDER::OpenAI: {
            // OpenAI/DeepSeek format: {"choices": [{"message": {"content": ...}}]}
            if (response_json.contains("choices") &&
                !response_json["choices"].empty() &&
                response_json["choices"][0].contains("message")) {

                const auto& message = response_json["choices"][0]["message"];

                // Handle both string and array formats
                if (message["content"].is_string()) {
                    content = message["content"].get<std::string>();
                    success = true;
                }
                else if (message["content"].is_array()) {
                    for (const auto& item : message["content"]) {
                        if (item.is_object() &&
                            item["type"] == "text" &&
                            item.contains("text")) {
                            content += item["text"].get<std::string>();
                            success = true;
                        }
                    }
                }
            }
            break;
        }
        default:
            return api_response(false, "", "Unsupported API provider");
        }

        if (success && !content.empty()) {
            return api_response(true, content);
        }
        return api_response(false, "", "Malformed API response: missing expected content");

    } catch (const json::parse_error& e) {
        return api_response(false, "", std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return api_response(false, "", std::string("Error processing response: ") + e.what());
    }
}
