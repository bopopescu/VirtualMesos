// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Authors: vladl@google.com (Vlad Losev), wan@google.com (Zhanyong Wan)
//
// This file tests the internal cross-platform support utilities.

#include <gtest/internal/gtest-port.h>

#if GTEST_OS_MAC
#include <pthread.h>
#include <time.h>
#endif  // GTEST_OS_MAC

#if GTEST_HAS_PTHREAD
#include <unistd.h>
#endif  // GTEST_HAS_PTHREAD

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

// Indicates that this translation unit is part of Google Test's
// implementation.  It must come before gtest-internal-inl.h is
// included, or there will be a compiler error.  This trick is to
// prevent a user from accidentally including gtest-internal-inl.h in
// his code.
#define GTEST_IMPLEMENTATION_ 1
#include "src/gtest-internal-inl.h"
#undef GTEST_IMPLEMENTATION_

namespace testing {
namespace internal {

TEST(GtestCheckSyntaxTest, BehavesLikeASingleStatement) {
  if (AlwaysFalse())
    GTEST_CHECK_(false) << "This should never be executed; "
                           "It's a compilation test only.";

  if (AlwaysTrue())
    GTEST_CHECK_(true);
  else
    ;  // NOLINT

  if (AlwaysFalse())
    ;  // NOLINT
  else
    GTEST_CHECK_(true) << "";
}

TEST(GtestCheckSyntaxTest, WorksWithSwitch) {
  switch (0) {
    case 1:
      break;
    default:
      GTEST_CHECK_(true);
  }

  switch(0)
    case 0:
      GTEST_CHECK_(true) << "Check failed in switch case";
}

#if GTEST_OS_MAC
void* ThreadFunc(void* data) {
  pthread_mutex_t* mutex = reinterpret_cast<pthread_mutex_t*>(data);
  pthread_mutex_lock(mutex);
  pthread_mutex_unlock(mutex);
  return NULL;
}

TEST(GetThreadCountTest, ReturnsCorrectValue) {
  EXPECT_EQ(1U, GetThreadCount());
  pthread_mutex_t mutex;
  pthread_attr_t  attr;
  pthread_t       thread_id;

  // TODO(vladl@google.com): turn mutex into internal::Mutex for automatic
  // destruction.
  pthread_mutex_init(&mutex, NULL);
  pthread_mutex_lock(&mutex);
  ASSERT_EQ(0, pthread_attr_init(&attr));
  ASSERT_EQ(0, pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE));

  const int status = pthread_create(&thread_id, &attr, &ThreadFunc, &mutex);
  ASSERT_EQ(0, pthread_attr_destroy(&attr));
  ASSERT_EQ(0, status);
  EXPECT_EQ(2U, GetThreadCount());
  pthread_mutex_unlock(&mutex);

  void* dummy;
  ASSERT_EQ(0, pthread_join(thread_id, &dummy));

  // MacOS X may not immediately report the updated thread count after
  // joining a thread, causing flakiness in this test. To counter that, we
  // wait for up to .5 seconds for the OS to report the correct value.
  for (int i = 0; i < 5; ++i) {
    if (GetThreadCount() == 1)
      break;

    timespec time;
    time.tv_sec = 0;
    time.tv_nsec = 100L * 1000 * 1000;  // .1 seconds.
    nanosleep(&time, NULL);
  }
  EXPECT_EQ(1U, GetThreadCount());
  pthread_mutex_destroy(&mutex);
}
#else
TEST(GetThreadCountTest, ReturnsZeroWhenUnableToCountThreads) {
  EXPECT_EQ(0U, GetThreadCount());
}
#endif  // GTEST_OS_MAC

TEST(GtestCheckDeathTest, DiesWithCorrectOutputOnFailure) {
  const bool a_false_condition = false;
  const char regex[] =
#ifdef _MSC_VER
     "gtest-port_test\\.cc\\(\\d+\\):"
#else
     "gtest-port_test\\.cc:[0-9]+"
#endif  // _MSC_VER
     ".*a_false_condition.*Extra info.*";

  EXPECT_DEATH_IF_SUPPORTED(GTEST_CHECK_(a_false_condition) << "Extra info",
                            regex);
}

#if GTEST_HAS_DEATH_TEST

TEST(GtestCheckDeathTest, LivesSilentlyOnSuccess) {
  EXPECT_EXIT({
      GTEST_CHECK_(true) << "Extra info";
      ::std::cerr << "Success\n";
      exit(0); },
      ::testing::ExitedWithCode(0), "Success");
}

#endif  // GTEST_HAS_DEATH_TEST

#if GTEST_USES_POSIX_RE

template <typename Str>
class RETest : public ::testing::Test {};

// Defines StringTypes as the list of all string types that class RE
// supports.
typedef testing::Types<
#if GTEST_HAS_STD_STRING
    ::std::string,
#endif  // GTEST_HAS_STD_STRING
#if GTEST_HAS_GLOBAL_STRING
    ::string,
#endif  // GTEST_HAS_GLOBAL_STRING
    const char*> StringTypes;

TYPED_TEST_CASE(RETest, StringTypes);

// Tests RE's implicit constructors.
TYPED_TEST(RETest, ImplicitConstructorWorks) {
  const RE empty(TypeParam(""));
  EXPECT_STREQ("", empty.pattern());

  const RE simple(TypeParam("hello"));
  EXPECT_STREQ("hello", simple.pattern());

  const RE normal(TypeParam(".*(\\w+)"));
  EXPECT_STREQ(".*(\\w+)", normal.pattern());
}

// Tests that RE's constructors reject invalid regular expressions.
TYPED_TEST(RETest, RejectsInvalidRegex) {
  EXPECT_NONFATAL_FAILURE({
    const RE invalid(TypeParam("?"));
  }, "\"?\" is not a valid POSIX Extended regular expression.");
}

// Tests RE::FullMatch().
TYPED_TEST(RETest, FullMatchWorks) {
  const RE empty(TypeParam(""));
  EXPECT_TRUE(RE::FullMatch(TypeParam(""), empty));
  EXPECT_FALSE(RE::FullMatch(TypeParam("a"), empty));

  const RE re(TypeParam("a.*z"));
  EXPECT_TRUE(RE::FullMatch(TypeParam("az"), re));
  EXPECT_TRUE(RE::FullMatch(TypeParam("axyz"), re));
  EXPECT_FALSE(RE::FullMatch(TypeParam("baz"), re));
  EXPECT_FALSE(RE::FullMatch(TypeParam("azy"), re));
}

// Tests RE::PartialMatch().
TYPED_TEST(RETest, PartialMatchWorks) {
  const RE empty(TypeParam(""));
  EXPECT_TRUE(RE::PartialMatch(TypeParam(""), empty));
  EXPECT_TRUE(RE::PartialMatch(TypeParam("a"), empty));

  const RE re(TypeParam("a.*z"));
  EXPECT_TRUE(RE::PartialMatch(TypeParam("az"), re));
  EXPECT_TRUE(RE::PartialMatch(TypeParam("axyz"), re));
  EXPECT_TRUE(RE::PartialMatch(TypeParam("baz"), re));
  EXPECT_TRUE(RE::PartialMatch(TypeParam("azy"), re));
  EXPECT_FALSE(RE::PartialMatch(TypeParam("zza"), re));
}

#elif GTEST_USES_SIMPLE_RE

TEST(IsInSetTest, NulCharIsNotInAnySet) {
  EXPECT_FALSE(IsInSet('\0', ""));
  EXPECT_FALSE(IsInSet('\0', "\0"));
  EXPECT_FALSE(IsInSet('\0', "a"));
}

TEST(IsInSetTest, WorksForNonNulChars) {
  EXPECT_FALSE(IsInSet('a', "Ab"));
  EXPECT_FALSE(IsInSet('c', ""));

  EXPECT_TRUE(IsInSet('b', "bcd"));
  EXPECT_TRUE(IsInSet('b', "ab"));
}

TEST(IsDigitTest, IsFalseForNonDigit) {
  EXPECT_FALSE(IsDigit('\0'));
  EXPECT_FALSE(IsDigit(' '));
  EXPECT_FALSE(IsDigit('+'));
  EXPECT_FALSE(IsDigit('-'));
  EXPECT_FALSE(IsDigit('.'));
  EXPECT_FALSE(IsDigit('a'));
}

TEST(IsDigitTest, IsTrueForDigit) {
  EXPECT_TRUE(IsDigit('0'));
  EXPECT_TRUE(IsDigit('1'));
  EXPECT_TRUE(IsDigit('5'));
  EXPECT_TRUE(IsDigit('9'));
}

TEST(IsPunctTest, IsFalseForNonPunct) {
  EXPECT_FALSE(IsPunct('\0'));
  EXPECT_FALSE(IsPunct(' '));
  EXPECT_FALSE(IsPunct('\n'));
  EXPECT_FALSE(IsPunct('a'));
  EXPECT_FALSE(IsPunct('0'));
}

TEST(IsPunctTest, IsTrueForPunct) {
  for (const char* p = "^-!\"#$%&'()*+,./:;<=>?@[\\]_`{|}~"; *p; p++) {
    EXPECT_PRED1(IsPunct, *p);
  }
}

TEST(IsRepeatTest, IsFalseForNonRepeatChar) {
  EXPECT_FALSE(IsRepeat('\0'));
  EXPECT_FALSE(IsRepeat(' '));
  EXPECT_FALSE(IsRepeat('a'));
  EXPECT_FALSE(IsRepeat('1'));
  EXPECT_FALSE(IsRepeat('-'));
}

TEST(IsRepeatTest, IsTrueForRepeatChar) {
  EXPECT_TRUE(IsRepeat('?'));
  EXPECT_TRUE(IsRepeat('*'));
  EXPECT_TRUE(IsRepeat('+'));
}

TEST(IsWhiteSpaceTest, IsFalseForNonWhiteSpace) {
  EXPECT_FALSE(IsWhiteSpace('\0'));
  EXPECT_FALSE(IsWhiteSpace('a'));
  EXPECT_FALSE(IsWhiteSpace('1'));
  EXPECT_FALSE(IsWhiteSpace('+'));
  EXPECT_FALSE(IsWhiteSpace('_'));
}

TEST(IsWhiteSpaceTest, IsTrueForWhiteSpace) {
  EXPECT_TRUE(IsWhiteSpace(' '));
  EXPECT_TRUE(IsWhiteSpace('\n'));
  EXPECT_TRUE(IsWhiteSpace('\r'));
  EXPECT_TRUE(IsWhiteSpace('\t'));
  EXPECT_TRUE(IsWhiteSpace('\v'));
  EXPECT_TRUE(IsWhiteSpace('\f'));
}

TEST(IsWordCharTest, IsFalseForNonWordChar) {
  EXPECT_FALSE(IsWordChar('\0'));
  EXPECT_FALSE(IsWordChar('+'));
  EXPECT_FALSE(IsWordChar('.'));
  EXPECT_FALSE(IsWordChar(' '));
  EXPECT_FALSE(IsWordChar('\n'));
}

TEST(IsWordCharTest, IsTrueForLetter) {
  EXPECT_TRUE(IsWordChar('a'));
  EXPECT_TRUE(IsWordChar('b'));
  EXPECT_TRUE(IsWordChar('A'));
  EXPECT_TRUE(IsWordChar('Z'));
}

TEST(IsWordCharTest, IsTrueForDigit) {
  EXPECT_TRUE(IsWordChar('0'));
  EXPECT_TRUE(IsWordChar('1'));
  EXPECT_TRUE(IsWordChar('7'));
  EXPECT_TRUE(IsWordChar('9'));
}

TEST(IsWordCharTest, IsTrueForUnderscore) {
  EXPECT_TRUE(IsWordChar('_'));
}

TEST(IsValidEscapeTest, IsFalseForNonPrintable) {
  EXPECT_FALSE(IsValidEscape('\0'));
  EXPECT_FALSE(IsValidEscape('\007'));
}

TEST(IsValidEscapeTest, IsFalseForDigit) {
  EXPECT_FALSE(IsValidEscape('0'));
  EXPECT_FALSE(IsValidEscape('9'));
}

TEST(IsValidEscapeTest, IsFalseForWhiteSpace) {
  EXPECT_FALSE(IsValidEscape(' '));
  EXPECT_FALSE(IsValidEscape('\n'));
}

TEST(IsValidEscapeTest, IsFalseForSomeLetter) {
  EXPECT_FALSE(IsValidEscape('a'));
  EXPECT_FALSE(IsValidEscape('Z'));
}

TEST(IsValidEscapeTest, IsTrueForPunct) {
  EXPECT_TRUE(IsValidEscape('.'));
  EXPECT_TRUE(IsValidEscape('-'));
  EXPECT_TRUE(IsValidEscape('^'));
  EXPECT_TRUE(IsValidEscape('$'));
  EXPECT_TRUE(IsValidEscape('('));
  EXPECT_TRUE(IsValidEscape(']'));
  EXPECT_TRUE(IsValidEscape('{'));
  EXPECT_TRUE(IsValidEscape('|'));
}

TEST(IsValidEscapeTest, IsTrueForSomeLetter) {
  EXPECT_TRUE(IsValidEscape('d'));
  EXPECT_TRUE(IsValidEscape('D'));
  EXPECT_TRUE(IsValidEscape('s'));
  EXPECT_TRUE(IsValidEscape('S'));
  EXPECT_TRUE(IsValidEscape('w'));
  EXPECT_TRUE(IsValidEscape('W'));
}

TEST(AtomMatchesCharTest, EscapedPunct) {
  EXPECT_FALSE(AtomMatchesChar(true, '\\', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, '\\', ' '));
  EXPECT_FALSE(AtomMatchesChar(true, '_', '.'));
  EXPECT_FALSE(AtomMatchesChar(true, '.', 'a'));

  EXPECT_TRUE(AtomMatchesChar(true, '\\', '\\'));
  EXPECT_TRUE(AtomMatchesChar(true, '_', '_'));
  EXPECT_TRUE(AtomMatchesChar(true, '+', '+'));
  EXPECT_TRUE(AtomMatchesChar(true, '.', '.'));
}

TEST(AtomMatchesCharTest, Escaped_d) {
  EXPECT_FALSE(AtomMatchesChar(true, 'd', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 'd', 'a'));
  EXPECT_FALSE(AtomMatchesChar(true, 'd', '.'));

  EXPECT_TRUE(AtomMatchesChar(true, 'd', '0'));
  EXPECT_TRUE(AtomMatchesChar(true, 'd', '9'));
}

TEST(AtomMatchesCharTest, Escaped_D) {
  EXPECT_FALSE(AtomMatchesChar(true, 'D', '0'));
  EXPECT_FALSE(AtomMatchesChar(true, 'D', '9'));

  EXPECT_TRUE(AtomMatchesChar(true, 'D', '\0'));
  EXPECT_TRUE(AtomMatchesChar(true, 'D', 'a'));
  EXPECT_TRUE(AtomMatchesChar(true, 'D', '-'));
}

TEST(AtomMatchesCharTest, Escaped_s) {
  EXPECT_FALSE(AtomMatchesChar(true, 's', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 's', 'a'));
  EXPECT_FALSE(AtomMatchesChar(true, 's', '.'));
  EXPECT_FALSE(AtomMatchesChar(true, 's', '9'));

  EXPECT_TRUE(AtomMatchesChar(true, 's', ' '));
  EXPECT_TRUE(AtomMatchesChar(true, 's', '\n'));
  EXPECT_TRUE(AtomMatchesChar(true, 's', '\t'));
}

TEST(AtomMatchesCharTest, Escaped_S) {
  EXPECT_FALSE(AtomMatchesChar(true, 'S', ' '));
  EXPECT_FALSE(AtomMatchesChar(true, 'S', '\r'));

  EXPECT_TRUE(AtomMatchesChar(true, 'S', '\0'));
  EXPECT_TRUE(AtomMatchesChar(true, 'S', 'a'));
  EXPECT_TRUE(AtomMatchesChar(true, 'S', '9'));
}

TEST(AtomMatchesCharTest, Escaped_w) {
  EXPECT_FALSE(AtomMatchesChar(true, 'w', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 'w', '+'));
  EXPECT_FALSE(AtomMatchesChar(true, 'w', ' '));
  EXPECT_FALSE(AtomMatchesChar(true, 'w', '\n'));

  EXPECT_TRUE(AtomMatchesChar(true, 'w', '0'));
  EXPECT_TRUE(AtomMatchesChar(true, 'w', 'b'));
  EXPECT_TRUE(AtomMatchesChar(true, 'w', 'C'));
  EXPECT_TRUE(AtomMatchesChar(true, 'w', '_'));
}

TEST(AtomMatchesCharTest, Escaped_W) {
  EXPECT_FALSE(AtomMatchesChar(true, 'W', 'A'));
  EXPECT_FALSE(AtomMatchesChar(true, 'W', 'b'));
  EXPECT_FALSE(AtomMatchesChar(true, 'W', '9'));
  EXPECT_FALSE(AtomMatchesChar(true, 'W', '_'));

  EXPECT_TRUE(AtomMatchesChar(true, 'W', '\0'));
  EXPECT_TRUE(AtomMatchesChar(true, 'W', '*'));
  EXPECT_TRUE(AtomMatchesChar(true, 'W', '\n'));
}

TEST(AtomMatchesCharTest, EscapedWhiteSpace) {
  EXPECT_FALSE(AtomMatchesChar(true, 'f', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 'f', '\n'));
  EXPECT_FALSE(AtomMatchesChar(true, 'n', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 'n', '\r'));
  EXPECT_FALSE(AtomMatchesChar(true, 'r', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 'r', 'a'));
  EXPECT_FALSE(AtomMatchesChar(true, 't', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 't', 't'));
  EXPECT_FALSE(AtomMatchesChar(true, 'v', '\0'));
  EXPECT_FALSE(AtomMatchesChar(true, 'v', '\f'));

  EXPECT_TRUE(AtomMatchesChar(true, 'f', '\f'));
  EXPECT_TRUE(AtomMatchesChar(true, 'n', '\n'));
  EXPECT_TRUE(AtomMatchesChar(true, 'r', '\r'));
  EXPECT_TRUE(AtomMatchesChar(true, 't', '\t'));
  EXPECT_TRUE(AtomMatchesChar(true, 'v', '\v'));
}

TEST(AtomMatchesCharTest, UnescapedDot) {
  EXPECT_FALSE(AtomMatchesChar(false, '.', '\n'));

  EXPECT_TRUE(AtomMatchesChar(false, '.', '\0'));
  EXPECT_TRUE(AtomMatchesChar(false, '.', '.'));
  EXPECT_TRUE(AtomMatchesChar(false, '.', 'a'));
  EXPECT_TRUE(AtomMatchesChar(false, '.', ' '));
}

TEST(AtomMatchesCharTest, UnescapedChar) {
  EXPECT_FALSE(AtomMatchesChar(false, 'a', '\0'));
  EXPECT_FALSE(AtomMatchesChar(false, 'a', 'b'));
  EXPECT_FALSE(AtomMatchesChar(false, '$', 'a'));

  EXPECT_TRUE(AtomMatchesChar(false, '$', '$'));
  EXPECT_TRUE(AtomMatchesChar(false, '5', '5'));
  EXPECT_TRUE(AtomMatchesChar(false, 'Z', 'Z'));
}

TEST(ValidateRegexTest, GeneratesFailureAndReturnsFalseForInvalid) {
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex(NULL)),
                          "NULL is not a valid simple regular expression");
  EXPECT_NONFATAL_FAILURE(
      ASSERT_FALSE(ValidateRegex("a\\")),
      "Syntax error at index 1 in simple regular expression \"a\\\": ");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("a\\")),
                          "'\\' cannot appear at the end");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("\\n\\")),
                          "'\\' cannot appear at the end");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("\\s\\hb")),
                          "invalid escape sequence \"\\h\"");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("^^")),
                          "'^' can only appear at the beginning");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex(".*^b")),
                          "'^' can only appear at the beginning");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("$$")),
                          "'$' can only appear at the end");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("^$a")),
                          "'$' can only appear at the end");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("a(b")),
                          "'(' is unsupported");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("ab)")),
                          "')' is unsupported");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("[ab")),
                          "'[' is unsupported");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("a{2")),
                          "'{' is unsupported");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("?")),
                          "'?' can only follow a repeatable token");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("^*")),
                          "'*' can only follow a repeatable token");
  EXPECT_NONFATAL_FAILURE(ASSERT_FALSE(ValidateRegex("5*+")),
                          "'+' can only follow a repeatable token");
}

TEST(ValidateRegexTest, ReturnsTrueForValid) {
  EXPECT_TRUE(ValidateRegex(""));
  EXPECT_TRUE(ValidateRegex("a"));
  EXPECT_TRUE(ValidateRegex(".*"));
  EXPECT_TRUE(ValidateRegex("^a_+"));
  EXPECT_TRUE(ValidateRegex("^a\\t\\&?"));
  EXPECT_TRUE(ValidateRegex("09*$"));
  EXPECT_TRUE(ValidateRegex("^Z$"));
  EXPECT_TRUE(ValidateRegex("a\\^Z\\$\\(\\)\\|\\[\\]\\{\\}"));
}

TEST(MatchRepetitionAndRegexAtHeadTest, WorksForZeroOrOne) {
  EXPECT_FALSE(MatchRepetitionAndRegexAtHead(false, 'a', '?', "a", "ba"));
  // Repeating more than once.
  EXPECT_FALSE(MatchRepetitionAndRegexAtHead(false, 'a', '?', "b", "aab"));

  // Repeating zero times.
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(false, 'a', '?', "b", "ba"));
  // Repeating once.
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(false, 'a', '?', "b", "ab"));
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(false, '#', '?', ".", "##"));
}

TEST(MatchRepetitionAndRegexAtHeadTest, WorksForZeroOrMany) {
  EXPECT_FALSE(MatchRepetitionAndRegexAtHead(false, '.', '*', "a$", "baab"));

  // Repeating zero times.
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(false, '.', '*', "b", "bc"));
  // Repeating once.
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(false, '.', '*', "b", "abc"));
  // Repeating more than once.
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(true, 'w', '*', "-", "ab_1-g"));
}

TEST(MatchRepetitionAndRegexAtHeadTest, WorksForOneOrMany) {
  EXPECT_FALSE(MatchRepetitionAndRegexAtHead(false, '.', '+', "a$", "baab"));
  // Repeating zero times.
  EXPECT_FALSE(MatchRepetitionAndRegexAtHead(false, '.', '+', "b", "bc"));

  // Repeating once.
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(false, '.', '+', "b", "abc"));
  // Repeating more than once.
  EXPECT_TRUE(MatchRepetitionAndRegexAtHead(true, 'w', '+', "-", "ab_1-g"));
}

TEST(MatchRegexAtHeadTest, ReturnsTrueForEmptyRegex) {
  EXPECT_TRUE(MatchRegexAtHead("", ""));
  EXPECT_TRUE(MatchRegexAtHead("", "ab"));
}

TEST(MatchRegexAtHeadTest, WorksWhenDollarIsInRegex) {
  EXPECT_FALSE(MatchRegexAtHead("$", "a"));

  EXPECT_TRUE(MatchRegexAtHead("$", ""));
  EXPECT_TRUE(MatchRegexAtHead("a$", "a"));
}

TEST(MatchRegexAtHeadTest, WorksWhenRegexStartsWithEscapeSequence) {
  EXPECT_FALSE(MatchRegexAtHead("\\w", "+"));
  EXPECT_FALSE(MatchRegexAtHead("\\W", "ab"));

  EXPECT_TRUE(MatchRegexAtHead("\\sa", "\nab"));
  EXPECT_TRUE(MatchRegexAtHead("\\d", "1a"));
}

TEST(MatchRegexAtHeadTest, WorksWhenRegexStartsWithRepetition) {
  EXPECT_FALSE(MatchRegexAtHead(".+a", "abc"));
  EXPECT_FALSE(MatchRegexAtHead("a?b", "aab"));

  EXPECT_TRUE(MatchRegexAtHead(".*a", "bc12-ab"));
  EXPECT_TRUE(MatchRegexAtHead("a?b", "b"));
  EXPECT_TRUE(MatchRegexAtHead("a?b", "ab"));
}

TEST(MatchRegexAtHeadTest,
     WorksWhenRegexStartsWithRepetionOfEscapeSequence) {
  EXPECT_FALSE(MatchRegexAtHead("\\.+a", "abc"));
  EXPECT_FALSE(MatchRegexAtHead("\\s?b", "  b"));

  EXPECT_TRUE(MatchRegexAtHead("\\(*a", "((((ab"));
  EXPECT_TRUE(MatchRegexAtHead("\\^?b", "^b"));
  EXPECT_TRUE(MatchRegexAtHead("\\\\?b", "b"));
  EXPECT_TRUE(MatchRegexAtHead("\\\\?b", "\\b"));
}

TEST(MatchRegexAtHeadTest, MatchesSequentially) {
  EXPECT_FALSE(MatchRegexAtHead("ab.*c", "acabc"));

  EXPECT_TRUE(MatchRegexAtHead("ab.*c", "ab-fsc"));
}

TEST(MatchRegexAnywhereTest, ReturnsFalseWhenStringIsNull) {
  EXPECT_FALSE(MatchRegexAnywhere("", NULL));
}

TEST(MatchRegexAnywhereTest, WorksWhenRegexStartsWithCaret) {
  EXPECT_FALSE(MatchRegexAnywhere("^a", "ba"));
  EXPECT_FALSE(MatchRegexAnywhere("^$", "a"));

  EXPECT_TRUE(MatchRegexAnywhere("^a", "ab"));
  EXPECT_TRUE(MatchRegexAnywhere("^", "ab"));
  EXPECT_TRUE(MatchRegexAnywhere("^$", ""));
}

TEST(MatchRegexAnywhereTest, ReturnsFalseWhenNoMatch) {
  EXPECT_FALSE(MatchRegexAnywhere("a", "bcde123"));
  EXPECT_FALSE(MatchRegexAnywhere("a.+a", "--aa88888888"));
}

TEST(MatchRegexAnywhereTest, ReturnsTrueWhenMatchingPrefix) {
  EXPECT_TRUE(MatchRegexAnywhere("\\w+", "ab1_ - 5"));
  EXPECT_TRUE(MatchRegexAnywhere(".*=", "="));
  EXPECT_TRUE(MatchRegexAnywhere("x.*ab?.*bc", "xaaabc"));
}

TEST(MatchRegexAnywhereTest, ReturnsTrueWhenMatchingNonPrefix) {
  EXPECT_TRUE(MatchRegexAnywhere("\\w+", "$$$ ab1_ - 5"));
  EXPECT_TRUE(MatchRegexAnywhere("\\.+=", "=  ...="));
}

// Tests RE's implicit constructors.
TEST(RETest, ImplicitConstructorWorks) {
  const RE empty("");
  EXPECT_STREQ("", empty.pattern());

  const RE simple("hello");
  EXPECT_STREQ("hello", simple.pattern());
}

// Tests that RE's constructors reject invalid regular expressions.
TEST(RETest, RejectsInvalidRegex) {
  EXPECT_NONFATAL_FAILURE({
    const RE normal(NULL);
  }, "NULL is not a valid simple regular expression");

  EXPECT_NONFATAL_FAILURE({
    const RE normal(".*(\\w+");
  }, "'(' is unsupported");

  EXPECT_NONFATAL_FAILURE({
    const RE invalid("^?");
  }, "'?' can only follow a repeatable token");
}

// Tests RE::FullMatch().
TEST(RETest, FullMatchWorks) {
  const RE empty("");
  EXPECT_TRUE(RE::FullMatch("", empty));
  EXPECT_FALSE(RE::FullMatch("a", empty));

  const RE re1("a");
  EXPECT_TRUE(RE::FullMatch("a", re1));

  const RE re("a.*z");
  EXPECT_TRUE(RE::FullMatch("az", re));
  EXPECT_TRUE(RE::FullMatch("axyz", re));
  EXPECT_FALSE(RE::FullMatch("baz", re));
  EXPECT_FALSE(RE::FullMatch("azy", re));
}

// Tests RE::PartialMatch().
TEST(RETest, PartialMatchWorks) {
  const RE empty("");
  EXPECT_TRUE(RE::PartialMatch("", empty));
  EXPECT_TRUE(RE::PartialMatch("a", empty));

  const RE re("a.*z");
  EXPECT_TRUE(RE::PartialMatch("az", re));
  EXPECT_TRUE(RE::PartialMatch("axyz", re));
  EXPECT_TRUE(RE::PartialMatch("baz", re));
  EXPECT_TRUE(RE::PartialMatch("azy", re));
  EXPECT_FALSE(RE::PartialMatch("zza", re));
}

#endif  // GTEST_USES_POSIX_RE

TEST(CaptureStderrTest, CapturesStdErr) {
  CaptureStderr();
  fprintf(stderr, "abc");
  ASSERT_STREQ("abc", GetCapturedStderr().c_str());
}

TEST(ThreadLocalTest, DefaultConstructor) {
  ThreadLocal<int> t1;
  EXPECT_EQ(0, t1.get());

  ThreadLocal<void*> t2;
  EXPECT_TRUE(t2.get() == NULL);
}

TEST(ThreadLocalTest, Init) {
  ThreadLocal<int> t1(123);
  EXPECT_EQ(123, t1.get());

  int i = 0;
  ThreadLocal<int*> t2(&i);
  EXPECT_EQ(&i, t2.get());
}

class NoCopyConstructor {
 public:
  NoCopyConstructor() {}
 private:
  GTEST_DISALLOW_COPY_AND_ASSIGN_(NoCopyConstructor);
};

TEST(ThreadLocalTest, CopyConstructorIsNotNeededForDefaultVersion) {
  ThreadLocal<NoCopyConstructor> bar;
  bar.get();
}

class NoDefaultContructor {
 public:
  explicit NoDefaultContructor(const char*) {}
  NoDefaultContructor(const NoDefaultContructor&) {}
};

TEST(ThreadLocalTest, NoDefaultContructorIsNeededForParamVersion) {
  ThreadLocal<NoDefaultContructor> bar(NoDefaultContructor("foo"));
  bar.pointer();
}

TEST(ThreadLocalTest, GetAndPointerShouldReturnTheSame) {
    ThreadLocal<String> threadLocal;
    ASSERT_EQ(threadLocal.pointer(), &(threadLocal.get()));
    const ThreadLocal<String>& constThreadLocal = threadLocal;
    ASSERT_EQ(threadLocal.pointer(), constThreadLocal.pointer());
}

#if GTEST_IS_THREADSAFE
TEST(MutexTestDeathTest, AssertHeldShouldAssertWhenNotLocked) {
    EXPECT_DEATH_IF_SUPPORTED({
        Mutex m;
        { MutexLock lock(&m); }
        m.AssertHeld();
      }
      , "Current thread is not holding mutex..+");
}

#if GTEST_HAS_PTHREAD
void Sleep(double time) {
  usleep(time*1000.0*1000.0);
}
#else  // GTEST_HAS_PTHREAD
#warning "Implement Sleep for your platform"
#endif  // GTEST_HAS_PTHREAD

void SleepABit(Random* random) {
  double microsecond = 1.0/(1000.0*1000.0);
  Sleep(random->Generate(40)*microsecond);
}

class AtomicCounterWithMutex {
 public:
  explicit AtomicCounterWithMutex(Mutex* mutex) :
    counter_(0), mutex_(mutex), random_(42) {}
  void increment() {
    MutexLock lock(mutex_);
    int temp_ = counter_;
    SleepABit(&random_);
    counter_ = temp_+1;
  }
  int counter() { return counter_; }
 private:
  volatile int counter_;
  Mutex*       mutex_;
  Random       random_;
};

const int NumberOfIncrementsInCounterThread = 20;

void CountWithMutexThread(AtomicCounterWithMutex* counter) {
  for (int i = 0; i < NumberOfIncrementsInCounterThread; ++i) {
      counter->increment();
  }
}

TEST(MutexTest, OnlyOneThreadCanBeInTheMutexAtATime) {
  Mutex mutex;
  AtomicCounterWithMutex lockedCounter(&mutex);

  const int NumCounterThreads = 7;
  typedef ThreadWithParam<AtomicCounterWithMutex*> ThreadType;
  scoped_ptr<ThreadType> counters[NumCounterThreads];
  for (int i = 0; i < NumCounterThreads; ++i) {
    counters[i].reset(new ThreadType(&CountWithMutexThread, &lockedCounter));
  }
  for (int i = 0; i < NumCounterThreads; ++i) {
      counters[i]->Start();
  }
  for (int i = 0; i < NumCounterThreads; ++i) {
      counters[i]->Join();
  }
  ASSERT_EQ(NumberOfIncrementsInCounterThread*NumCounterThreads,
    lockedCounter.counter());
}

TEST(MutexTest, StaticConstructionHappensLaterThanUse) {
  char storageBeforeStaticInit[sizeof(Mutex)] = {0};
  Mutex* mutexBeforeCtr = reinterpret_cast<Mutex*>(storageBeforeStaticInit);
  {
    MutexLock lock(mutexBeforeCtr);
    mutexBeforeCtr->AssertHeld();

    Mutex* mutex = new(storageBeforeStaticInit) Mutex(
      Mutex::NO_CONSTRUCTOR_NEEDED_FOR_STATIC_MUTEX);
    mutex->AssertHeld();
  }
  mutexBeforeCtr->~Mutex();
}

TEST(MutexTest, StaticMutexShouldWorkAfterDestruction) {
  char storage[sizeof(Mutex)] = {0};
  Mutex* mutex = new(storage) Mutex(
      Mutex::NO_CONSTRUCTOR_NEEDED_FOR_STATIC_MUTEX);
  mutex->~Mutex();

  MutexLock lock(mutex);
  mutex->AssertHeld();
}

void AddTwo(int* param) {
  *param += 2;
}

TEST(ThreadWithParamTest, ShouldRunAction) {
  int i = 40;
  ThreadWithParam<int*> thread(AddTwo, &i);
  thread.Start();
  thread.Join();
  ASSERT_EQ(42, i);
}

const int NumberOfIncrementsInCountWithMutexThread = 1;

void CountWithMutex(AtomicCounterWithMutex* counter) {
  for (int i = 0; i < NumberOfIncrementsInCountWithMutexThread; ++i) {
    counter->increment();
  }
}

void emulateRaceOnStaticMutexLazyInit(int randomizer) {
  char storageBeforeStaticInit[sizeof(Mutex)] = {0};
  Mutex* mutexBeforeCtr = reinterpret_cast<Mutex*> (storageBeforeStaticInit);
  AtomicCounterWithMutex counter(mutexBeforeCtr);
  const int MaxNumCountersThread = 17;
  const int NumCounterThreads = 2+randomizer % (MaxNumCountersThread-2);

  typedef ThreadWithParam<AtomicCounterWithMutex*> ThreadType;
  scoped_ptr<ThreadType> counters[MaxNumCountersThread];
  for (int i = 0; i < NumCounterThreads; ++i) {
    counters[i].reset(new ThreadType(&CountWithMutex, &counter));
  }
  for (int i = 0; i < NumCounterThreads; ++i) {
    counters[i]->Start();
  }
  Mutex* mutex = new(storageBeforeStaticInit) Mutex(
    Mutex::NO_CONSTRUCTOR_NEEDED_FOR_STATIC_MUTEX);
  for (int i = 0; i < NumCounterThreads; ++i) {
    counters[i]->Join();
  }
  ASSERT_EQ(NumberOfIncrementsInCountWithMutexThread*NumCounterThreads,
    counter.counter());
  mutex->~Mutex();
}

TEST(MutexTest, LazyInitOnStaticMutexShouldBeRaceFree) {
  for (int round = 0; round < 500; ++round) {
    emulateRaceOnStaticMutexLazyInit(round);
  }
}

template <typename T>
void RunFromThread(void (func)(T), T param) {
  ThreadWithParam<T> thread(func, param);
  thread.Start();
  thread.Join();
}

struct CheckThreadLocalDefaultParam {
  ThreadLocal<String>* threadLocal_;
  const char* default_;
};

void CheckThreadLocalDefault(CheckThreadLocalDefaultParam* param) {
  EXPECT_STREQ(param->default_, param->threadLocal_->get().c_str());
}

TEST(ThreadLocalTest, ParameterizedConstructorSetsDefault) {
  using std::pair;

  ThreadLocal<String> threadLocal("foo");
  ASSERT_STREQ("foo", threadLocal.get().c_str());

  threadLocal.set("bar");
  ASSERT_STREQ("bar", threadLocal.get().c_str());

  CheckThreadLocalDefaultParam params = {&threadLocal, "foo"};
  RunFromThread(CheckThreadLocalDefault, &params);
}

class CountedDestructor {
 public:
  ~CountedDestructor() {
    counter()++;
  }
  static int& counter() {
    static int result = 0;
    return result;
  }
};

template <typename T>
void CallThreadLocalGet(ThreadLocal<T>* threadLocal) {
    threadLocal->get();
}

TEST(ThreadLocalTest, ShouldDestroyManagedObjectsNoLaterThanTLManager) {
  CountedDestructor::counter() = 0;
  {
    ThreadLocal<CountedDestructor> threadLocal;
    typedef ThreadWithParam<ThreadLocal<CountedDestructor>*> ThreadType;
    ThreadType thread(&CallThreadLocalGet<CountedDestructor>, &threadLocal);
    thread.Start();
    thread.Join();
  }
  // There should be 2 desctuctor calls as ThreadLocal also contains a member
  // T - used as a prototype for copy ctr version.
  ASSERT_EQ(2, CountedDestructor::counter());
}

void CheckThreadLocalIsEmpty(ThreadLocal<String>* threadLocal) {
  EXPECT_STREQ(String().c_str(), threadLocal->get().c_str());
}

TEST(ThreadLocalTest, ThreadLocalsInDifferentThreadShouldNotAffectEachOther) {
  ThreadLocal<String> threadLocal;
  threadLocal.set("Foo");
  ASSERT_STREQ("Foo", threadLocal.get().c_str());
  RunFromThread(CheckThreadLocalIsEmpty, &threadLocal);
}
#endif  // GTEST_IS_THREADSAFE

}  // namespace internal
}  // namespace testing