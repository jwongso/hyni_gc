#include "../src/chat_api.h"
#include <gtest/gtest.h>
#include <cstdlib>

using namespace hyni;
using namespace testing;

// Fixture class for chat_api tests
class ChatApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment variables
        setenv("OA_API_KEY", "test_openai_key", 1);
        setenv("DS_API_KEY", "test_deepseek_key", 1);
        setenv("CL_API_KEY", "test_claude_key", 1);
    }

    void TearDown() override {
        // Clean up
        unsetenv("OA_API_KEY");
        unsetenv("DS_API_KEY");
        unsetenv("CL_API_KEY");
    }
};

TEST_F(ChatApiTest, ConstructorSetsCorrectProvider) {
    chat_api openai_api("https://api.openai.com/v1");
    EXPECT_EQ(openai_api.get_api_provider(), API_PROVIDER::OpenAI);

    chat_api deepseek_api("https://api.deepseek.com/v1");
    EXPECT_EQ(deepseek_api.get_api_provider(), API_PROVIDER::DeepSeek);

    chat_api claude_api("https://api.anthropic.com/v1/messages");
    EXPECT_EQ(claude_api.get_api_provider(), API_PROVIDER::ClaudeAI);

    EXPECT_ANY_THROW(chat_api unknown_api("https://unknown.api.com"));
}

TEST_F(ChatApiTest, HasApiKeyReturnsCorrectValue) {
    chat_api api("https://api.openai.com/v1");
    EXPECT_TRUE(api.has_api_key());

    unsetenv("OA_API_KEY");
    chat_api no_key_api("https://api.openai.com/v1");
    EXPECT_TRUE(no_key_api.has_api_key());
}

TEST_F(ChatApiTest, SetApiKeyWorksCorrectly) {
    chat_api api("https://api.openai.com/v1");
    api.set_api_key("custom_key");
    EXPECT_TRUE(api.has_api_key());
}

TEST_F(ChatApiTest, GetAssistantReplyParsesValidJson) {
    chat_api api("https://api.openai.com/v1");
    std::string valid_json = R"({
        "choices": [{
            "message": {
                "content": "This is a test response"
            }
        }]
    })";

    chat_api::api_response response = api.get_assistant_reply(valid_json);
    EXPECT_EQ(response.content, "This is a test response");
}

TEST_F(ChatApiTest, GetAssistantReplyHandlesErrorResponse) {
    chat_api api("https://api.openai.com/v1");
    std::string error_json = R"({
        "error": {
            "message": "Invalid API key"
        }
    })";

    chat_api::api_response response = api.get_assistant_reply(error_json);
    EXPECT_TRUE(response.content.empty());
}

TEST_F(ChatApiTest, GetAssistantReplyHandlesInvalidJson) {
    chat_api api("https://api.openai.com/v1");
    std::string invalid_json = "not a json string";

    chat_api::api_response response = api.get_assistant_reply(invalid_json);
    EXPECT_TRUE(response.content.empty());
}

TEST_F(ChatApiTest, WriteCallbackWorksCorrectly) {
    std::string output;
    const char* test_data = "test data";
    size_t result = chat_api::write_callback((void*)test_data, 1, strlen(test_data), &output);

    EXPECT_EQ(result, strlen(test_data));
    EXPECT_EQ(output, "test data");
}

TEST_F(ChatApiTest, WriteCallbackHandlesException) {
    std::string* output = nullptr;
    const char* test_data = "test data";
    size_t result = chat_api::write_callback((void*)test_data, 1, strlen(test_data), output);

    EXPECT_EQ(result, 0);
}

TEST_F(ChatApiTest, HandlesDifferentQuestionTypes) {
    chat_api openai_api("https://api.openai.com/v1");
    chat_api deepseek_api("https://api.deepseek.com/v1");
    chat_api claude_api("https://api.anthropic.com/v1/messages");

    // Verify these calls don't crash
    hyni::prompt p{"test", "", ""};
    EXPECT_NO_THROW(openai_api.send(p));
    p.question_type = QUESTION_TYPE::SystemDesign;
    EXPECT_NO_THROW(openai_api.send(p));
    p.question_type = QUESTION_TYPE::Coding;
    EXPECT_NO_THROW(openai_api.send(p));

    hyni::prompt r{"test", "", ""};
    EXPECT_NO_THROW(deepseek_api.send(r));
    r.question_type = QUESTION_TYPE::SystemDesign;
    EXPECT_NO_THROW(deepseek_api.send(r));
    r.question_type = QUESTION_TYPE::Coding;
    EXPECT_NO_THROW(deepseek_api.send(r));

    hyni::prompt m{"test", "", ""};
    EXPECT_NO_THROW(claude_api.send(m));
    m.question_type = QUESTION_TYPE::SystemDesign;
    EXPECT_NO_THROW(claude_api.send(m));
    m.question_type = QUESTION_TYPE::Coding;
    EXPECT_NO_THROW(claude_api.send(m));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
