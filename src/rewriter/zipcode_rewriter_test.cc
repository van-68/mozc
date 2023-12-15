// Copyright 2010-2021, Google Inc.
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

#include "rewriter/zipcode_rewriter.h"

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

using ::mozc::dictionary::PosMatcher;

enum SegmentType {
  ZIPCODE = 1,
  NON_ZIPCODE = 2,
};

void AddSegment(const absl::string_view key, const absl::string_view value,
                SegmentType type, const PosMatcher &pos_matcher,
                Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->value = std::string(key);
  candidate->content_key = std::string(key);
  candidate->content_value = std::string(value);

  if (type == ZIPCODE) {
    candidate->lid = pos_matcher.GetZipcodeId();
    candidate->rid = pos_matcher.GetZipcodeId();
  }
}

bool HasZipcodeAndAddress(const Segments &segments,
                          const absl::string_view expected) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    if (candidate.description == "郵便番号と住所") {
      if (candidate.content_value == expected) {
        return true;
      }
    }
  }
  return false;
}

class ZipcodeRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    pos_matcher_.Set(mock_data_manager_.GetPosMatcherData());
  }

  ZipcodeRewriter CreateZipcodeRewriter() const {
    return ZipcodeRewriter(pos_matcher_);
  }

  dictionary::PosMatcher pos_matcher_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(ZipcodeRewriterTest, BasicTest) {
  ZipcodeRewriter zipcode_rewriter = CreateZipcodeRewriter();

  const std::string kZipcode = "107-0052";
  const std::string kAddress = "東京都港区赤坂";
  ConversionRequest request;
  config::Config config;
  request.set_config(&config);

  {
    Segments segments;
    AddSegment("test", "test", NON_ZIPCODE, pos_matcher_, &segments);
    EXPECT_FALSE(zipcode_rewriter.Rewrite(request, &segments));
  }

  {
    config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);

    Segments segments;
    AddSegment(kZipcode, kAddress, ZIPCODE, pos_matcher_, &segments);
    zipcode_rewriter.Rewrite(request, &segments);
    EXPECT_TRUE(HasZipcodeAndAddress(segments, kZipcode + " " + kAddress));
  }

  {
    config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);

    Segments segments;
    AddSegment(kZipcode, kAddress, ZIPCODE, pos_matcher_, &segments);
    zipcode_rewriter.Rewrite(request, &segments);
    EXPECT_TRUE(HasZipcodeAndAddress(segments,
                                     // full-width space
                                     kZipcode + "　" + kAddress));
  }
}

}  // namespace
}  // namespace mozc
