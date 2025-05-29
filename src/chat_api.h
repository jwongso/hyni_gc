#ifndef CHAT_API_H
#define CHAT_API_H

#include <string>
#include <atomic>
#include <functional>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "model_context.h"

namespace hyni {

class chat_api {
public:
    struct api_response {
        bool success;
        std::string content;
        std::string error;

        // Convenience constructor
        api_response(bool s, const std::string& c = "", const std::string& e = "")
            : success(s), content(c), error(e) {}
    };

    chat_api(const std::string& url);
    chat_api(API_PROVIDER provider);
    ~chat_api();
    chat_api(const chat_api&) = delete;
    chat_api& operator=(const chat_api&) = delete;
    chat_api(chat_api&&) = delete;
    chat_api& operator=(chat_api&&) = delete;

    std::string send(const prompt& prompt,
                     const std::function<bool()>& should_cancel = []{ return false; });

    api_response get_assistant_reply(const std::string& json_response);

    model_context& context() { return *m_context; }
    const model_context& context() const { return *m_context; }

    void cancel();
    bool has_api_key() const;
    void set_api_key(const std::string& api_key);
    API_PROVIDER get_api_provider() const;
    void set_api_provider(API_PROVIDER provider);

    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s);

private:
    static std::unique_ptr<model_context> create_context(API_PROVIDER provider);
    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                 curl_off_t ultotal, curl_off_t ulnow);
    API_PROVIDER detect_api_provider(const std::string& url);
    curl_slist *setup_curl_options(CURL* curl, const std::string& payload_str);

private:
    CURL* m_curl_handle;
    std::unique_ptr<model_context> m_context;
    std::atomic<bool> m_cancel_flag{false};
};

} // namespace hyni

#endif // CHAT_API_H
