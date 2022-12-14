// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/navigation_controller.h"
#include "chrome/test/test_tab_contents.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class DownloadRequestManagerTest : public testing::Test,
    public DownloadRequestManager::Callback,
    public DownloadRequestManager::TestingDelegate {
 public:
  virtual void SetUp() {
    allow_download_ = true;
    ask_allow_count_ = cancel_count_ = continue_count_ = 0;
    factory_.reset(TestTabContentsFactory::CreateAndRegisterFactory());
    TestTabContents* contents = factory_->CreateInstanceImpl();
    contents->set_commit_on_navigate(true);
    controller_ = new NavigationController(contents, &profile_);
    download_request_manager_ = new DownloadRequestManager(NULL, NULL);
    DownloadRequestManager::SetTestingDelegate(this);
  }

  virtual void TearDown() {
    controller_->Destroy();
    DownloadRequestManager::SetTestingDelegate(NULL);
  }

  virtual void ContinueDownload() {
    continue_count_++;
  }
  virtual void CancelDownload() {
    cancel_count_++;
  }

  void CanDownload() {
    download_request_manager_->CanDownloadImpl(
        controller_->active_contents(), this);
  }

  virtual bool ShouldAllowDownload() {
    ask_allow_count_++;
    return allow_download_;
  }

 protected:
  TestingProfile profile_;
  scoped_ptr<TestTabContentsFactory> factory_;
  NavigationController* controller_;
  scoped_refptr<DownloadRequestManager> download_request_manager_;

  // Number of times ContinueDownload was invoked.
  int continue_count_;

  // Number of times CancelDownload was invoked.
  int cancel_count_;

  // Whether the download should be allowed.
  bool allow_download_;

  // Number of times ShouldAllowDownload was invoked.
  int ask_allow_count_;
};

TEST_F(DownloadRequestManagerTest, Allow) {
  // All tabs should initially start at ALLOW_ONE_DOWNLOAD.
  ASSERT_EQ(DownloadRequestManager::ALLOW_ONE_DOWNLOAD,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));

  // Ask if the tab can do a download. This moves to PROMPT_BEFORE_DOWNLOAD.
  CanDownload();
  ASSERT_EQ(DownloadRequestManager::PROMPT_BEFORE_DOWNLOAD,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));
  // We should have been told we can download.
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  ASSERT_EQ(0, ask_allow_count_);
  continue_count_ = 0;

  // Ask again. This triggers asking the delegate for allow/disallow.
  allow_download_ = true;
  CanDownload();
  // This should ask us if the download is allowed.
  ASSERT_EQ(1, ask_allow_count_);
  ask_allow_count_ = 0;
  ASSERT_EQ(DownloadRequestManager::ALLOW_ALL_DOWNLOADS,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));
  // We should have been told we can download.
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  continue_count_ = 0;

  // Ask again and make sure continue is invoked.
  CanDownload();
  // The state is at allow_all, which means the delegate shouldn't be asked.
  ASSERT_EQ(0, ask_allow_count_);
  ASSERT_EQ(DownloadRequestManager::ALLOW_ALL_DOWNLOADS,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));
  // We should have been told we can download.
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  continue_count_ = 0;
}

TEST_F(DownloadRequestManagerTest, ResetOnNavigation) {
  controller_->LoadURL(GURL(factory_->scheme() + "://foo.com/bar"), GURL(), 0);

  // Do two downloads, allowing the second so that we end up with allow all.
  CanDownload();
  allow_download_ = true;
  CanDownload();
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  ASSERT_EQ(DownloadRequestManager::ALLOW_ALL_DOWNLOADS,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));

  // Navigate to a new URL with the same host, which shouldn't reset the allow
  // all state.
  controller_->LoadURL(GURL(factory_->scheme() + "://foo.com/bar2"),
                       GURL(), 0);
  CanDownload();
  ASSERT_EQ(1, continue_count_);
  ASSERT_EQ(0, cancel_count_);
  ASSERT_EQ(0, ask_allow_count_);
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  ASSERT_EQ(DownloadRequestManager::ALLOW_ALL_DOWNLOADS,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));

  // Do a user gesture, because we're at allow all, this shouldn't change the
  // state.
  download_request_manager_->OnUserGesture(controller_->active_contents());
  ASSERT_EQ(DownloadRequestManager::ALLOW_ALL_DOWNLOADS,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));

  // Navigate to a completely different host, which should reset the state.
  controller_->LoadURL(GURL(factory_->scheme() + "://fooey.com"), GURL(), 0);
  ASSERT_EQ(DownloadRequestManager::ALLOW_ONE_DOWNLOAD,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));
}

TEST_F(DownloadRequestManagerTest, ResetOnUserGesture) {
  controller_->LoadURL(GURL(factory_->scheme() + "://foo.com/bar"), GURL(), 0);

  // Do one download, which should change to prompt before download.
  CanDownload();
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  ASSERT_EQ(DownloadRequestManager::PROMPT_BEFORE_DOWNLOAD,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));

  // Do a user gesture, which should reset back to allow one.
  download_request_manager_->OnUserGesture(controller_->active_contents());
  ASSERT_EQ(DownloadRequestManager::ALLOW_ONE_DOWNLOAD,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));

  // Ask twice, which triggers calling the delegate. Don't allow the download
  // so that we end up with not allowed.
  allow_download_ = false;
  CanDownload();
  CanDownload();
  ASSERT_EQ(DownloadRequestManager::DOWNLOADS_NOT_ALLOWED,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));

  // A user gesture now should NOT change the state.
  download_request_manager_->OnUserGesture(controller_->active_contents());
  ASSERT_EQ(DownloadRequestManager::DOWNLOADS_NOT_ALLOWED,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));
  // And make sure we really can't download.
  ask_allow_count_ = continue_count_ = cancel_count_ = 0;
  CanDownload();
  ASSERT_EQ(0, ask_allow_count_);
  ASSERT_EQ(0, continue_count_);
  ASSERT_EQ(1, cancel_count_);
  // And the state shouldn't have changed.
  ASSERT_EQ(DownloadRequestManager::DOWNLOADS_NOT_ALLOWED,
            download_request_manager_->GetDownloadStatus(
                controller_->active_contents()));
}
