/* Copyright (c) 2024 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/web_discovery/browser/payload_generator.h"

#include <memory>
#include <utility>

#include "base/test/values_test_util.h"
#include "brave/components/web_discovery/browser/content_scraper.h"
#include "brave/components/web_discovery/browser/patterns.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_discovery {

class WebDiscoveryPayloadGeneratorTest : public testing::Test {
 public:
  ~WebDiscoveryPayloadGeneratorTest() override = default;

  // testing::Test:
  void SetUp() override {
    server_config_ = std::make_unique<ServerConfig>();
    server_config_->location = "us";

    url_details_ = std::make_unique<PatternsURLDetails>();

    url_details_->payload_rule_groups = std::vector<PayloadRuleGroup>(2);

    auto& single_group = url_details_->payload_rule_groups[0];
    single_group.key = ".single-element";
    single_group.rule_type = PayloadRuleType::kSingle;
    single_group.result_type = PayloadResultType::kSingle;
    single_group.action = "single_action";

    auto& query_group = url_details_->payload_rule_groups[1];
    query_group.key = "query_group";
    query_group.rule_type = PayloadRuleType::kQuery;
    query_group.result_type = PayloadResultType::kClustered;
    query_group.action = "query";
    query_group.rules = std::vector<PayloadRule>(2);

    auto& join_rule = query_group.rules[0];
    join_rule.selector = "#results";
    join_rule.key = "r";
    join_rule.is_join = true;

    auto& qurl_rule = query_group.rules[1];
    qurl_rule.selector = "qurl";
    qurl_rule.key = "qurl";
    qurl_rule.is_join = false;
  }

 protected:
  std::vector<base::Value::Dict> GenerateQueryPayloadsHelper(
      std::unique_ptr<PageScrapeResult> scrape_result) {
    return GenerateQueryPayloads(*server_config_.get(), url_details_.get(),
                                 std::move(scrape_result));
  }

  std::unique_ptr<ServerConfig> server_config_;

 private:
  std::unique_ptr<PatternsURLDetails> url_details_;
};

TEST_F(WebDiscoveryPayloadGeneratorTest, GenerateQueryPayloads) {
  GURL test_url("https://example.com/test");
  auto scrape_result = std::make_unique<PageScrapeResult>(test_url, "test_id");

  std::vector<base::Value::Dict> single_dicts;
  single_dicts.push_back(
      base::Value::Dict().Set("ab", "value1").Set("cd", "value2"));
  single_dicts.push_back(
      base::Value::Dict().Set("ef", "value3").Set("gh", "value4"));
  scrape_result->fields[".single-element"] = std::move(single_dicts);

  std::vector<base::Value::Dict> result_dicts1;
  result_dicts1.push_back(base::Value::Dict().Set("njk", "joinvalue1"));
  result_dicts1.push_back(base::Value::Dict().Set("abc", "joinvalue2"));
  result_dicts1.push_back(base::Value::Dict().Set("njk", "joinvalue3"));
  result_dicts1.push_back(base::Value::Dict().Set("abc", "joinvalue4"));

  std::vector<base::Value::Dict> result_dicts2;
  result_dicts2.push_back(
      base::Value::Dict().Set("qurl", "https://example.com/test1"));
  scrape_result->fields["#results"] = std::move(result_dicts1);
  scrape_result->fields["qurl"] = std::move(result_dicts2);

  auto payloads = GenerateQueryPayloadsHelper(std::move(scrape_result));
  ASSERT_EQ(payloads.size(), 3u);

  const auto* payload = &payloads[0];
  const auto* inner_payload = payload->FindDict(kInnerPayloadKey);
  ASSERT_TRUE(inner_payload);
  base::ExpectDictStringValue("single_action", *payload, kActionKey);

  EXPECT_EQ(inner_payload->size(), 3u);

  base::ExpectDictStringValue("us", *inner_payload, "ctry");
  base::ExpectDictStringValue("value1", *inner_payload, "ab");
  base::ExpectDictStringValue("value2", *inner_payload, "cd");

  payload = &payloads[1];
  inner_payload = payload->FindDict(kInnerPayloadKey);
  ASSERT_TRUE(inner_payload);
  base::ExpectDictStringValue("single_action", *payload, kActionKey);

  EXPECT_EQ(inner_payload->size(), 3u);

  base::ExpectDictStringValue("us", *inner_payload, "ctry");
  base::ExpectDictStringValue("value3", *inner_payload, "ef");
  base::ExpectDictStringValue("value4", *inner_payload, "gh");

  payload = &payloads[2];
  inner_payload = payload->FindDict(kInnerPayloadKey);
  ASSERT_TRUE(inner_payload);
  base::ExpectDictStringValue("query", *payload, kActionKey);

  EXPECT_EQ(inner_payload->size(), 2u);

  const auto* r_dict = inner_payload->FindDict("r");
  ASSERT_TRUE(r_dict);
  EXPECT_EQ(r_dict->size(), 4u);
  base::ExpectDictStringValue("https://example.com/test1", *inner_payload,
                              "qurl");

  const auto* r0_dict = r_dict->FindDict("0");
  const auto* r1_dict = r_dict->FindDict("1");
  const auto* r2_dict = r_dict->FindDict("2");
  const auto* r3_dict = r_dict->FindDict("3");
  ASSERT_TRUE(r0_dict && r1_dict && r2_dict && r3_dict);
  EXPECT_EQ(r0_dict->size(), 1u);
  EXPECT_EQ(r1_dict->size(), 1u);
  EXPECT_EQ(r2_dict->size(), 1u);
  EXPECT_EQ(r3_dict->size(), 1u);
  base::ExpectDictStringValue("joinvalue1", *r0_dict, "njk");
  base::ExpectDictStringValue("joinvalue2", *r1_dict, "abc");
  base::ExpectDictStringValue("joinvalue3", *r2_dict, "njk");
  base::ExpectDictStringValue("joinvalue4", *r3_dict, "abc");
}

TEST_F(WebDiscoveryPayloadGeneratorTest, GenerateAlivePayload) {
  std::string date_hour = "2023051509";

  auto alive_payload = GenerateAlivePayload(*server_config_.get(), date_hour);

  base::ExpectDictStringValue("alive", alive_payload, "action");

  const auto* inner_payload = alive_payload.FindDict("payload");

  ASSERT_TRUE(inner_payload);

  base::ExpectDictStringValue("us", *inner_payload, "ctry");
  base::ExpectDictStringValue(date_hour, *inner_payload, "t");
  base::ExpectDictBooleanValue(true, *inner_payload, "status");
}

TEST_F(WebDiscoveryPayloadGeneratorTest, ExcludePrivateResult) {
  GURL test_url("https://example.com/search");
  auto scrape_result = std::make_unique<PageScrapeResult>(test_url, "test_id");

  std::vector<base::Value::Dict> result_dicts1;
  for (int i = 0; i < 5; i++) {
    base::Value::Dict result_dict;
    std::string url = "https://example.com/result";
    if (i == 1) {
      url = "https://423947892374892879.com/example";
    } else {
      url += base::NumberToString(i == 0 ? 0 : i - 1);
    }
    result_dict.Set("u", url);
    result_dicts1.push_back(std::move(result_dict));
  }
  scrape_result->fields["#results"] = std::move(result_dicts1);
  std::vector<base::Value::Dict> result_dicts2;
  base::Value::Dict qurl_dict;
  qurl_dict.Set("qurl", "https://example.com/test1");
  result_dicts2.push_back(std::move(qurl_dict));
  scrape_result->fields["qurl"] = std::move(result_dicts2);

  auto payloads = GenerateQueryPayloadsHelper(std::move(scrape_result));
  ASSERT_EQ(payloads.size(), 1u);

  const auto* payload = &payloads[0];
  const auto* inner_payload = payload->FindDict("payload");
  const auto* r_dict = inner_payload->FindDict("r");
  ASSERT_TRUE(inner_payload && r_dict);

  ASSERT_EQ(r_dict->size(), 4u);

  for (int i = 0; i < 4; i++) {
    const auto* ri_dict = r_dict->FindDict(base::NumberToString(i));
    ASSERT_TRUE(ri_dict);

    base::ExpectDictStringValue(
        "https://example.com/result" + base::NumberToString(i), *ri_dict, "u");
  }
}

TEST_F(WebDiscoveryPayloadGeneratorTest, ShouldDropSearchResult) {
  GURL test_url("https://example.com/search");
  auto scrape_result = std::make_unique<PageScrapeResult>(test_url, "test_id");

  std::vector<base::Value::Dict> result_dicts;
  for (int i = 0; i < 3; i++) {
    base::Value::Dict result_dict;
    result_dict.Set("u",
                    "https://example.com/result" + base::NumberToString(i));
    result_dicts.push_back(std::move(result_dict));
  }
  scrape_result->fields["#results"] = std::move(result_dicts);

  std::vector<base::Value::Dict> qurl_dicts;
  base::Value::Dict qurl_dict;
  qurl_dict.Set("qurl", "https://example.com/test1");
  qurl_dicts.push_back(std::move(qurl_dict));
  scrape_result->fields["qurl"] = std::move(qurl_dicts);

  auto payloads = GenerateQueryPayloadsHelper(std::move(scrape_result));
  ASSERT_EQ(payloads.size(), 0u);
}

TEST_F(WebDiscoveryPayloadGeneratorTest, ContentMissing) {
  GURL test_url("https://example.com/search");
  auto scrape_result = std::make_unique<PageScrapeResult>(test_url, "test_id");

  std::vector<base::Value::Dict> result_dicts;
  for (int i = 0; i < 9; i++) {
    base::Value::Dict result_dict;
    if (i < 5) {
      result_dict.Set("x", "");
    } else {
      result_dict.Set("x", base::Value());
    }
    result_dicts.push_back(std::move(result_dict));
  }
  scrape_result->fields["#results"] = std::move(result_dicts);

  std::vector<base::Value::Dict> qurl_dicts;
  base::Value::Dict qurl_dict;
  qurl_dict.Set("qurl", "https://example.com/test1");
  qurl_dicts.push_back(std::move(qurl_dict));
  scrape_result->fields["qurl"] = std::move(qurl_dicts);

  auto payloads = GenerateQueryPayloadsHelper(std::move(scrape_result));
  ASSERT_EQ(payloads.size(), 0u);
}

}  // namespace web_discovery
