// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "testing/gtest/include/gtest/gtest.h"

// identifiers for known autocomplete providers
#define HISTORY_IDENTIFIER L"Chrome:History"
#define SEARCH_IDENTIFIER L"google.com/websearch/en"

namespace {

const int num_results_per_provider = 3;

// Autocomplete provider that provides known results. Note that this is
// refcounted so that it can also be a task on the message loop.
class TestProvider : public AutocompleteProvider {
 public:
  TestProvider(int relevance, const std::wstring& prefix)
      : AutocompleteProvider(NULL, NULL, ""),
        relevance_(relevance),
        prefix_(prefix) {
  }

  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes,
                     bool synchronous_only);

  void set_listener(ACProviderListener* listener) {
    listener_ = listener;
  }

 private:
  void Run();

  void AddResults(int start_at, int num);

  int relevance_;
  const std::wstring prefix_;
};

void TestProvider::Start(const AutocompleteInput& input,
                         bool minimal_changes,
                         bool synchronous_only) {
  if (minimal_changes)
    return;

  matches_.clear();

  // Generate one result synchronously, the rest later.
  AddResults(0, 1);

  if (!synchronous_only) {
    done_ = false;
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &TestProvider::Run));
  }
}

void TestProvider::Run() {
  DCHECK(num_results_per_provider > 0);
  AddResults(1, num_results_per_provider);
  done_ = true;
  DCHECK(listener_);
  listener_->OnProviderUpdate(true);
}

void TestProvider::AddResults(int start_at, int num) {
  for (int i = start_at; i < num; i++) {
    AutocompleteMatch match(this, relevance_ - i, false);

    wchar_t str[16];
    swprintf_s(str, L"%d", i);
    match.fill_into_edit = prefix_ + str;
    match.destination_url = match.fill_into_edit;

    match.contents = match.destination_url;
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.description = match.destination_url;
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));

    matches_.push_back(match);
  }
}

class AutocompleteProviderTest : public testing::Test,
                                 public ACControllerListener {
 public:
  // ACControllerListener
  virtual void OnAutocompleteUpdate(bool updated_result, bool query_complete);

 protected:
  // testing::Test
  virtual void SetUp();

  void ResetController(bool same_destinations);

  // Runs a query on the input "a", and makes sure both providers' input is
  // properly collected.
  void RunTest();

  // These providers are owned by the controller once it's created.
  ACProviders providers_;

  AutocompleteResult result_;

 private:
  MessageLoopForUI message_loop_;
  scoped_ptr<AutocompleteController> controller_;
};

void AutocompleteProviderTest::SetUp() {
  ResetController(false);
}

void AutocompleteProviderTest::ResetController(bool same_destinations) {
  // Forget about any existing providers.  The controller owns them and will
  // Release() them below, when we delete it during the call to reset().
  providers_.clear();

  // Construct two new providers, with either the same or different prefixes.
  TestProvider* providerA = new TestProvider(num_results_per_provider, L"a");
  providerA->AddRef();
  providers_.push_back(providerA);

  TestProvider* providerB = new TestProvider(num_results_per_provider * 2,
                                             same_destinations ? L"a" : L"b");
  providerB->AddRef();
  providers_.push_back(providerB);

  // Reset the controller to contain our new providers.
  AutocompleteController* controller =
      new AutocompleteController(this, providers_);
  controller_.reset(controller);
  providerA->set_listener(controller);
  providerB->set_listener(controller);
}

void AutocompleteProviderTest::OnAutocompleteUpdate(bool updated_result,
                                                    bool query_complete) {
  controller_->GetResult(&result_);
  if (query_complete)
    MessageLoop::current()->Quit();
}

void AutocompleteProviderTest::RunTest() {
  result_.Reset();
  const AutocompleteInput input(L"a", std::wstring(), true, false);
  EXPECT_FALSE(controller_->Start(input, false, false));

  // The message loop will terminate when all autocomplete input has been
  // collected.
  MessageLoop::current()->Run();
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         const AutocompleteResult::const_iterator& iter) {
  return os << static_cast<const AutocompleteMatch*>(&(*iter));
}

// Tests that the default selection is set properly when updating results.
TEST_F(AutocompleteProviderTest, Query) {
  RunTest();

  // Make sure the default match gets set to the highest relevance match.  The
  // highest relevance matches should come from the second provider.
  EXPECT_EQ(num_results_per_provider * 2, result_.size());  // two providers
  ASSERT_NE(result_.end(), result_.default_match());
  EXPECT_EQ(providers_[1], result_.default_match()->provider);
}

TEST_F(AutocompleteProviderTest, RemoveDuplicates) {
  // Set up the providers to provide duplicate results.
  ResetController(true);

  RunTest();

  // Make sure all the first provider's results were eliminated by the second
  // provider's.
  EXPECT_EQ(num_results_per_provider, result_.size());
  for (AutocompleteResult::const_iterator i(result_.begin());
       i != result_.end(); ++i)
    EXPECT_EQ(providers_[1], i->provider);

  // Set things back to the default for the benefit of any tests that run after
  // us.
  ResetController(false);
}

TEST(AutocompleteTest, InputType) {
  struct test_data {
    const wchar_t* input;
    const AutocompleteInput::Type type;
  } input_cases[] = {
    { L"", AutocompleteInput::INVALID },
    { L"?", AutocompleteInput::FORCED_QUERY },
    { L"?foo", AutocompleteInput::FORCED_QUERY },
    { L"?foo bar", AutocompleteInput::FORCED_QUERY },
    { L"?http://foo.com/bar", AutocompleteInput::FORCED_QUERY },
    { L"foo", AutocompleteInput::UNKNOWN },
    { L"foo.com", AutocompleteInput::URL },
    { L"foo/bar", AutocompleteInput::URL },
    { L"foo/bar baz", AutocompleteInput::UNKNOWN },
    { L"http://foo/bar baz", AutocompleteInput::URL },
    { L"foo bar", AutocompleteInput::QUERY },
    { L"link:foo.com", AutocompleteInput::UNKNOWN },
    { L"www.foo.com:81", AutocompleteInput::URL },
    { L"localhost:8080", AutocompleteInput::URL },
    { L"en.wikipedia.org/wiki/James Bond", AutocompleteInput::URL },
    // In Chrome itself, mailto: will get handled by ShellExecute, but in
    // unittest mode, we don't have the data loaded in the external protocol
    // handler to know this.
    // { L"mailto:abuse@foo.com", AutocompleteInput::URL },
    { L"view-source:http://www.foo.com/", AutocompleteInput::URL },
    { L"javascript:alert(\"Hey there!\");", AutocompleteInput::URL },
    { L"C:\\Program Files", AutocompleteInput::URL },
    { L"\\\\Server\\Folder\\File", AutocompleteInput::URL },
    { L"http://foo.com/", AutocompleteInput::URL },
    { L"127.0.0.1", AutocompleteInput::URL },
    { L"browser.tabs.closeButtons", AutocompleteInput::UNKNOWN },
  };

  for (int i = 0; i < arraysize(input_cases); ++i) {
    AutocompleteInput input(input_cases[i].input, std::wstring(), true, false);
    EXPECT_EQ(input_cases[i].type, input.type()) << "Input: " <<
        input_cases[i].input;
  }
}

// Test that we can properly compare matches' relevance when at least one is
// negative.
TEST(AutocompleteMatch, MoreRelevant) {
  struct RelevantCases {
    int r1;
    int r2;
    bool expected_result;
  } cases[] = {
    {  10,   0, true  },
    {  10,  -5, true  },
    {  -5,  10, false },
    {   0,  10, false },
    { -10,  -5, true  },
    {  -5, -10, false },
  };

  AutocompleteMatch m1;
  AutocompleteMatch m2;

  for (int i = 0; i < arraysize(cases); ++i) {
    m1.relevance = cases[i].r1;
    m2.relevance = cases[i].r2;
    EXPECT_EQ(cases[i].expected_result,
              AutocompleteMatch::MoreRelevant(m1, m2));
  }
}

