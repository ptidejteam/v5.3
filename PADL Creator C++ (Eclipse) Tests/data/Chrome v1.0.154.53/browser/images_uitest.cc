// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "chrome/test/ui/ui_test.h"

class ImagesTest : public UITest {
 protected:
  ImagesTest() : UITest() {
    launch_arguments_ = test_data_directory_;
    file_util::AppendToPath(&launch_arguments_, L"animated-gifs.html");
  }
};

TEST_F(ImagesTest, AnimatedGIFs) {
  std::wstring page_title = L"animated gif test";

  // Let the GIFs fully animate.
  for (int i = 0; i < 10; ++i) {
    Sleep(kWaitForActionMaxMsec / 10);
    if (page_title == GetActiveTabTitle())
      break;
  }

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // Tau will check if this crashed.
}

