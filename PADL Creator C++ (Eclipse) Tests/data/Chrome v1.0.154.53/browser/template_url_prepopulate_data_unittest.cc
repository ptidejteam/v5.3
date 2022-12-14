// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/template_url.h"
#include "chrome/browser/template_url_prepopulate_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test TemplateURLPrepopulateDataTest;

// Verifies the set of prepopulate data doesn't contain entries with duplicate
// ids.
TEST_F(TemplateURLPrepopulateDataTest, UniqueIDs) {
  // GEO ids.
  int ids[] = { 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC,
                0xE, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x20, 0x22, 0x23, 0x25, 0x26,
                0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x31, 0x32,
                0x33, 0x36, 0x37, 0x38, 0x39, 0x3B, 0x3D, 0x3E, 0x3F, 0x41,
                0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4B, 0x4D,
                0x4E, 0x50, 0x51, 0x54, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
                0x5D, 0x5E, 0x62, 0x63, 0x64, 0x65, 0x67, 0x68, 0x6A, 0x6C,
                0x6D, 0x6E, 0x6F, 0x71, 0x72, 0x74, 0x75, 0x76, 0x77, 0x79,
                0x7A, 0x7C, 0x7D, 0x7E, 0x7F, 0x81, 0x82, 0x83, 0x85, 0x86,
                0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x91, 0x92,
                0x93, 0x94, 0x95, 0x97, 0x98, 0x9A, 0x9C, 0x9D, 0x9E, 0x9F,
                0xA0, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xAD, 0xAE,
                0xAF, 0xB0, 0xB1, 0xB2, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,
                0xBB, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6,
                0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0,
                0xD1, 0xD2, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB,
                0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE3, 0xE4, 0xE7, 0xE8,
                0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2,
                0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFB, 0xFC, 0xFD, 0xFE,
                0x102, 0x103, 0x104, 0x105, 0x107, 0x108, 0x10D, 0x12C, 0x12D,
                0x12E, 0x12F, 0x130, 0x131, 0x132, 0x133, 0x134, 0x135, 0x136,
                0x137, 0x138, 0x139, 0x13A, 0x13B, 0x13D, 0x13E, 0x13F, 0x141,
                0x142, 0x143, 0x144, 0x145, 0x146, 0x147, 0x148, 0x149, 0x14A,
                0x14B, 0x14C, 0x14D, 0x14E, 0x14F, 0x150, 0x151, 0x152, 0x153,
                0x154, 0x155, 0x156, 0x157, 0x15A, 0x15B, 0x15C, 0x15D, 0x15F,
                0x160, 0x3B16, 0x4CA2, 0x52FA, 0x6F60E7, -1 };
  TestingProfile profile;
  for (size_t i = 0; i < arraysize(ids); ++i) {
    profile.GetPrefs()->SetInteger(prefs::kGeoIDAtInstall, ids[i]);
    ScopedVector<TemplateURL> urls;
    size_t url_count;
    TemplateURLPrepopulateData::GetPrepopulatedEngines(
        profile.GetPrefs(), &(urls.get()), &url_count);
    std::set<int> unique_ids;
    for (size_t turl_i = 0; turl_i < urls.size(); ++turl_i) {
      ASSERT_TRUE(unique_ids.find(urls[turl_i]->prepopulate_id()) ==
                  unique_ids.end());
      unique_ids.insert(urls[turl_i]->prepopulate_id());
    }
  }
}
