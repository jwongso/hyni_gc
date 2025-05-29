#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/schema_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <filesystem>
#include <curl/curl.h>

using namespace hyni;
using json = nlohmann::json;

// Simple write callback for storing response in a string
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    if (!s) return 0;
    size_t new_length = size * nmemb;
    try {
        s->append(static_cast<char*>(contents), new_length);
    } catch (...) {
        return 0;
    }
    return new_length;
}

// Minimal API call function for testing purposes
std::string make_api_call(const std::string& url,
                          const std::string& api_key,
                          const std::string& payload,
                          bool is_anthropic = false) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to init curl");

    std::string response;
    struct curl_slist* headers = nullptr;

    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (is_anthropic) {
        headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
        headers = curl_slist_append(headers, ("x-api-key: " + api_key).c_str());
    } else {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("CURL failed: ") + curl_easy_strerror(res));
    }

    return response;
}

class GeneralContextFunctionalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get API key from environment
        const char* api_key = std::getenv("CL_API_KEY");
        if (!api_key) {
            GTEST_SKIP() << "CL_API_KEY environment variable not set";
        }
        m_api_key = api_key;

        // Create test schema directory and copy claude.json
        m_test_schema_dir = "../schemas";

        // Setup schema manager
        auto& manager = schema_manager::get_instance();
        manager.set_schema_directory(m_test_schema_dir);

        // Create context with validation enabled
        context_config config;
        config.enable_validation = true;
        config.default_max_tokens = 100;
        config.default_temperature = 0.3;

        m_context = manager.create_context("claude", config);

        // Replace API key placeholder in headers
        auto request = m_context->build_request();
        // We'll need to handle API key injection at request time
    }

    void TearDown() override {
    }

    void create_test_image() {
        // Create a small test image (1x1 pixel PNG)
        const unsigned char png_data[] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
            0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
            0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
            0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
            0x54, 0x08, 0x99, 0x01, 0x01, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01,
            0xE2, 0x21, 0xBC, 0x33, 0x00, 0x00, 0x00, 0x00,
            0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
        };

        std::ofstream file("test_image.png", std::ios::binary);
        file.write(reinterpret_cast<const char*>(png_data), sizeof(png_data));
        file.close();
    }

    std::string m_api_key;
    std::string m_test_schema_dir;
    std::unique_ptr<general_context> m_context;
};

// Test basic schema loading and context creation
TEST_F(GeneralContextFunctionalTest, SchemaManagerBasicFunctionality) {
    auto& manager = schema_manager::get_instance();

    // Test provider availability
    EXPECT_TRUE(manager.is_provider_available("claude"));

    // Test available providers list
    auto providers = manager.get_available_providers();
    EXPECT_FALSE(providers.empty());
    EXPECT_NE(std::find(providers.begin(), providers.end(), "claude"), providers.end());

    // Test context creation
    EXPECT_NE(m_context, nullptr);
    EXPECT_TRUE(m_context->supports_multimodal());
    EXPECT_TRUE(m_context->supports_system_messages());
    EXPECT_TRUE(m_context->supports_streaming());
}

// Test basic single message conversation
TEST_F(GeneralContextFunctionalTest, BasicSingleMessage) {
    // Set up a simple conversation
    m_context->add_user_message("Hello, please respond with exactly 'Hi there!'");

    // Validate request structure
    EXPECT_TRUE(m_context->is_valid_request());

    auto request = m_context->build_request();

    // Verify request structure
    EXPECT_TRUE(request.contains("model"));
    EXPECT_TRUE(request.contains("max_tokens"));
    EXPECT_TRUE(request.contains("messages"));
    EXPECT_EQ(request["messages"].size(), 1);
    EXPECT_EQ(request["messages"][0]["role"], "user");

    std::string payload = request.dump();

    //Perform actual API call
    std::string api_key = m_api_key;
    std::string api_url = m_context->get_endpoint();
    bool is_anthropic = m_context->get_provider_name() == "claude";

    std::string response_str;
    json response_json;
    try {
        response_str = make_api_call(api_url, api_key, payload, is_anthropic);
        std::cout << response_json << std::endl;
        response_json = json::parse(response_str);
    } catch (const std::exception& ex) {
        FAIL() << "API call failed: " << ex.what();
    }

    // Step 6: Extract and validate response
    std::string text = m_context->extract_text_response(response_json);
    EXPECT_FALSE(text.empty());
    EXPECT_EQ(text, "Hi there!");
}

// Test multi-turn conversation
TEST_F(GeneralContextFunctionalTest, MultiTurnConversation) {
    // First exchange
    m_context->add_user_message("What's 2+2?");

    auto request1 = m_context->build_request();
    EXPECT_EQ(request1["messages"].size(), 1);

    // Simulate assistant response
    m_context->add_assistant_message("2+2 equals 4.");

    // Second user message
    m_context->add_user_message("What about 3+3?");

    auto request2 = m_context->build_request();
    EXPECT_EQ(request2["messages"].size(), 3);

    // Verify message order and roles
    EXPECT_EQ(request2["messages"][0]["role"], "user");
    EXPECT_EQ(request2["messages"][1]["role"], "assistant");
    EXPECT_EQ(request2["messages"][2]["role"], "user");

    EXPECT_TRUE(m_context->is_valid_request());
}

// Test system message functionality
TEST_F(GeneralContextFunctionalTest, SystemMessage) {
    std::string system_prompt = "You are a helpful assistant that responds concisely.";
    m_context->set_system_message(system_prompt);

    m_context->add_user_message("Hello");

    auto request = m_context->build_request();

    // Check how system message is handled based on schema configuration
    // Claude API uses separate "system" field, not a system message in messages array
    if (request.contains("system")) {
        // Anthropic Claude style - separate system field
        EXPECT_EQ(request["system"], system_prompt);
        EXPECT_EQ(request["messages"].size(), 1);
        EXPECT_EQ(request["messages"][0]["role"], "user");
    } else {
        // OpenAI style - system message as first message
        EXPECT_EQ(request["messages"].size(), 2);
        EXPECT_EQ(request["messages"][0]["role"], "system");
        EXPECT_EQ(request["messages"][0]["content"], system_prompt);
        EXPECT_EQ(request["messages"][1]["role"], "user");
    }

    EXPECT_TRUE(m_context->is_valid_request());
}

// Test parameter setting and validation
TEST_F(GeneralContextFunctionalTest, ParameterHandling) {
    // Test valid parameters
    m_context->set_parameter("temperature", 0.7);
    m_context->set_parameter("max_tokens", 150);
    m_context->set_parameter("top_p", 0.9);

    m_context->add_user_message("Test message");

    auto request = m_context->build_request();

    EXPECT_EQ(request["temperature"], 0.7);
    EXPECT_EQ(request["max_tokens"], 150);
    EXPECT_EQ(request["top_p"], 0.9);

    // Test parameter validation (with validation enabled)
    EXPECT_THROW(m_context->set_parameter("temperature", 2.0), validation_exception);
    EXPECT_THROW(m_context->set_parameter("max_tokens", -1), validation_exception);
    EXPECT_THROW(m_context->set_parameter("top_p", 1.5), validation_exception);
}

// Test model selection
TEST_F(GeneralContextFunctionalTest, ModelSelection) {
    // Test valid model
    m_context->set_model("claude-3-5-haiku-20241022");
    m_context->add_user_message("Hello");

    auto request = m_context->build_request();
    EXPECT_EQ(request["model"], "claude-3-5-haiku-20241022");

    // Test invalid model (should throw with validation enabled)
    EXPECT_THROW(m_context->set_model("invalid-model"), validation_exception);

    // Test supported models list
    auto models = m_context->get_supported_models();
    EXPECT_FALSE(models.empty());
    EXPECT_NE(std::find(models.begin(), models.end(), "claude-3-5-sonnet-20241022"), models.end());
}

// Test image handling (multimodal)
TEST_F(GeneralContextFunctionalTest, MultimodalImageHandling) {
    create_test_image();

    // Test with image file path
    m_context->add_user_message("What do you see in this image?", "image/png", "test_image.png");

    auto request = m_context->build_request();
    EXPECT_EQ(request["messages"].size(), 1);

    auto content = request["messages"][0]["content"];
    EXPECT_EQ(content.size(), 2); // text + image

    // Verify text content
    EXPECT_EQ(content[0]["type"], "text");
    EXPECT_EQ(content[0]["text"], "What do you see in this image?");

    // Verify image content
    EXPECT_EQ(content[1]["type"], "image");
    EXPECT_EQ(content[1]["source"]["media_type"], "image/png");
    EXPECT_TRUE(content[1]["source"].contains("data"));

    // Clean up
    std::filesystem::remove("test_image.png");
}

// Test validation errors
TEST_F(GeneralContextFunctionalTest, ValidationErrors) {
    // Test empty context (no messages)
    auto errors = m_context->get_validation_errors();
    EXPECT_FALSE(errors.empty());
    EXPECT_FALSE(m_context->is_valid_request());

    // Add message and test again
    m_context->add_user_message("Hello");
    errors = m_context->get_validation_errors();
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(m_context->is_valid_request());

    // Test invalid message role (if we could inject it)
    // This would require modifying the internal state, which is protected
}

// Test context reset functionality
TEST_F(GeneralContextFunctionalTest, ContextReset) {
    // Set up context with data
    m_context->set_system_message("Test system");
    m_context->set_parameter("temperature", 0.8);
    m_context->add_user_message("Hello");
    m_context->add_assistant_message("Hi");

    // Verify data is present
    auto request_before = m_context->build_request();
    EXPECT_EQ(request_before["messages"].size(), 2);
    EXPECT_EQ(request_before["temperature"], 0.8);
    EXPECT_TRUE(request_before.contains("system"));

    // Reset context
    m_context->reset();

    // Verify data is cleared
    auto errors = m_context->get_validation_errors();
    EXPECT_FALSE(errors.empty()); // Should have validation errors due to no messages

    auto request_after = m_context->build_request();
    EXPECT_EQ(request_after["messages"].size(), 0);
    EXPECT_FALSE(request_after.contains("temperature") && request_after["temperature"] == 0.8);
}

// Test response parsing (with mock responses)
TEST_F(GeneralContextFunctionalTest, ResponseParsing) {
    // Create mock successful response
    json mock_response = {
        {"id", "msg_123"},
        {"type", "message"},
        {"role", "assistant"},
        {"content", json::array({{{"type", "text"}, {"text", "Hello! How can I help you?"}}})},
        {"model", "claude-3-5-sonnet-20241022"},
        {"stop_reason", "end_turn"},
        {"usage", {{"input_tokens", 15}, {"output_tokens", 8}}}
    };

    // Test text extraction
    std::string text = m_context->extract_text_response(mock_response);
    EXPECT_EQ(text, "Hello! How can I help you?");

    // Test full response extraction
    auto content = m_context->extract_full_response(mock_response);
    EXPECT_TRUE(content.is_array());
    EXPECT_EQ(content.size(), 1);

    // Test error response
    json error_response = {
        {"type", "error"},
        {"error", {
            {"type", "invalid_request_error"},
            {"message", "Missing required field: max_tokens"}
        }}
    };

    std::string error_msg = m_context->extract_error(error_response);
    EXPECT_EQ(error_msg, "Missing required field: max_tokens");
}

// Test edge cases and error conditions
TEST_F(GeneralContextFunctionalTest, EdgeCasesAndErrors) {
    // Test very long message
    std::string long_message(10000, 'a');
    EXPECT_NO_THROW(m_context->add_user_message(long_message));

    // Test special characters
    m_context->clear_messages();
    m_context->add_user_message("Hello 世界! 🌍 Special chars: @#$%^&*()");
    EXPECT_TRUE(m_context->is_valid_request());

    // Test empty message
    m_context->clear_messages();
    EXPECT_NO_THROW(m_context->add_user_message(""));

    // Test null/empty parameter values
    EXPECT_THROW(m_context->set_parameter("top_k", nullptr), validation_exception);

    // Test clearing individual components
    m_context->add_user_message("Test");
    m_context->set_parameter("temperature", 0.5);

    m_context->clear_messages();
    auto request = m_context->build_request();
    EXPECT_EQ(request["messages"].size(), 0);
    EXPECT_EQ(request["temperature"], 0.5); // Parameters should remain

    m_context->clear_parameters();
    request = m_context->build_request();
    EXPECT_FALSE(request.contains("temperature") && request["temperature"] == 0.5);
}

// Test rate limiting awareness (mock test)
TEST_F(GeneralContextFunctionalTest, RateLimitingHandling) {
    // This would test rate limiting in a real scenario
    // For now, just verify we can make multiple requests in sequence

    for (int i = 0; i < 3; ++i) {
        m_context->clear_messages();
        m_context->add_user_message("Test message " + std::to_string(i));

        auto request = m_context->build_request();
        EXPECT_TRUE(m_context->is_valid_request());

        // In real implementation, you'd add delays and handle 429 responses
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Performance test for request building
TEST_F(GeneralContextFunctionalTest, PerformanceTest) {
    auto start = std::chrono::high_resolution_clock::now();

    // Build many requests
    for (int i = 0; i < 1000; ++i) {
        if (i % 100 == 0) {
            m_context->clear_messages();
        }
        m_context->add_user_message("Message " + std::to_string(i));
        auto request = m_context->build_request();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should be able to build requests quickly
    EXPECT_LT(duration.count(), 1000); // Less than 1 second for 1000 requests
}

// Integration test (requires actual API key and network)
TEST_F(GeneralContextFunctionalTest, ActualAPIIntegration) {
    m_context->set_system_message("Respond with exactly 'Integration test successful'");
    m_context->add_user_message("Please confirm this integration test is working.");

    auto request = m_context->build_request();

    std::string api_key = m_api_key;
    std::string api_url = m_context->get_endpoint();
    bool is_anthropic = m_context->get_provider_name() == "claude";
    EXPECT_TRUE(is_anthropic);

    std::string payload = request.dump();

    std::string response_str;
    json response_json;
    try {
        response_str = make_api_call(api_url, api_key, payload, is_anthropic);
        std::cout << response_json << std::endl;
        response_json = json::parse(response_str);
    } catch (const std::exception& ex) {
        FAIL() << "API call failed: " << ex.what();
    }

    // Step 6: Extract and validate response
    std::string text = m_context->extract_text_response(response_json);
    EXPECT_FALSE(text.empty());
    EXPECT_EQ(text, "Integration test successful");
}
