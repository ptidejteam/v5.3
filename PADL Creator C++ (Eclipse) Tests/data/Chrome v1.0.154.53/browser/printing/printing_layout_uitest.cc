// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/gfx/gdi_util.h"
#include "base/gfx/platform_device_win.h"
#include "base/gfx/png_decoder.h"
#include "base/gfx/png_encoder.h"
#include "base/time.h"
#include "base/win_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/gfx/emf.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/browser/printing/printing_test.h"
#include "net/url_request/url_request_unittest.h"

namespace {

const wchar_t* const kGenerateSwitch = L"print-layout-generate";
const wchar_t kDocRoot[] = L"chrome/test/data";

// Lightweight raw-bitmap management. The image, once initialized, is immuable.
// It is mainly used for comparison.
class Image {
 public:
  // Creates the image from the given filename on disk.
  Image(const std::wstring& filename) : ignore_alpha_(true) {
    std::string data;
    file_util::ReadFileToString(filename, &data);
    EXPECT_TRUE(data.size());
    std::wstring ext = file_util::GetFileExtensionFromPath(filename);
    if (LowerCaseEqualsASCII(ext, "png")) {
      LoadPng(data);
    } else if (LowerCaseEqualsASCII(ext, "emf")) {
      LoadEmf(data);
    } else {
      EXPECT_TRUE(false);
    }
  }

  const gfx::Size& size() const {
    return size_;
  }

  // Used to create the initial test files.
  void SaveToPng(const std::wstring& filename) {
    ASSERT_FALSE(data_.empty());
    std::vector<unsigned char> compressed;
    ASSERT_TRUE(PNGEncoder::Encode(&*data_.begin(),
                                   PNGEncoder::FORMAT_BGRA,
                                   size_.width(),
                                   size_.height(),
                                   row_length_,
                                   true,
                                   &compressed));
    ASSERT_TRUE(compressed.size());
    FILE* f;
    ASSERT_EQ(_wfopen_s(&f, filename.c_str(), L"wbS"), 0);
    ASSERT_EQ(fwrite(&*compressed.begin(), 1, compressed.size(), f),
              compressed.size());
    fclose(f);
  }

  double PercentageDifferent(const Image& rhs) const {
    if (size_.width() == 0 || size_.height() == 0 ||
        rhs.size_.width() == 0 || rhs.size_.height() == 0)
      return 100.;

    int width = std::min(size_.width(), rhs.size_.width());
    int height = std::min(size_.height(), rhs.size_.height());
    // Compute pixels different in the overlap
    int pixels_different = 0;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        uint32 lhs_pixel = pixel_at(x, y);
        uint32 rhs_pixel = rhs.pixel_at(x, y);
        if (lhs_pixel != rhs_pixel)
          ++pixels_different;
      }

      // Look for extra right lhs pixels. They should be white.
      for (int x = width; x < size_.width(); ++x) {
        uint32 lhs_pixel = pixel_at(x, y);
        if (lhs_pixel != Color(SK_ColorWHITE))
          ++pixels_different;
      }

      // Look for extra right rhs pixels. They should be white.
      for (int x = width; x < rhs.size_.width(); ++x) {
        uint32 rhs_pixel = rhs.pixel_at(x, y);
        if (rhs_pixel != Color(SK_ColorWHITE))
          ++pixels_different;
      }
    }

    // Look for extra bottom lhs pixels. They should be white.
    for (int y = height; y < size_.height(); ++y) {
      for (int x = 0; x < size_.width(); ++x) {
        uint32 lhs_pixel = pixel_at(x, y);
        if (lhs_pixel != Color(SK_ColorWHITE))
          ++pixels_different;
      }
    }

    // Look for extra bottom rhs pixels. They should be white.
    for (int y = height; y < rhs.size_.height(); ++y) {
      for (int x = 0; x < rhs.size_.width(); ++x) {
        uint32 rhs_pixel = rhs.pixel_at(x, y);
        if (rhs_pixel != Color(SK_ColorWHITE))
          ++pixels_different;
      }
    }

    // Like the WebKit ImageDiff tool, we define percentage different in terms
    // of the size of the 'actual' bitmap.
    double total_pixels = static_cast<double>(size_.width()) *
                          static_cast<double>(height);
    return static_cast<double>(pixels_different) / total_pixels * 100.;
  }

  // Returns the 0x0RGB or 0xARGB value of the pixel at the given location,
  // depending on ignore_alpha_.
  uint32 Color(uint32 color) const {
    if (ignore_alpha_)
      return color & 0xFFFFFF;  // Strip out A.
    else
      return color;
  }

  uint32 pixel_at(int x, int y) const {
    EXPECT_TRUE(x >= 0 && x < size_.width());
    EXPECT_TRUE(y >= 0 && y < size_.height());
    const uint32* data = reinterpret_cast<const uint32*>(&*data_.begin());
    const uint32* data_row = data + y * row_length_ / sizeof(uint32);
    return Color(data_row[x]);
  }

 private:
  void LoadPng(const std::string& compressed) {
    int w;
    int h;
    EXPECT_TRUE(PNGDecoder::Decode(
        reinterpret_cast<const unsigned char*>(compressed.c_str()),
        compressed.size(), PNGDecoder::FORMAT_BGRA, &data_, &w, &h));
    size_.SetSize(w, h);
    row_length_ = size_.width() * sizeof(uint32);
  }

  void LoadEmf(const std::string& data) {
    ASSERT_FALSE(data.empty());
    gfx::Emf emf;
    emf.CreateFromData(data.data(), data.size());
    gfx::Rect rect(emf.GetBounds());
    // Create a temporary HDC and bitmap to retrieve the renderered data.
    HDC hdc = CreateCompatibleDC(NULL);
    BITMAPV4HEADER hdr;
    EXPECT_FALSE(rect.x());
    EXPECT_FALSE(rect.y());
    EXPECT_NE(rect.width(), 0);
    EXPECT_NE(rect.height(), 0);
    size_ = rect.size();
    gfx::CreateBitmapV4Header(rect.width(), rect.height(), &hdr);
    void* bits;
    HBITMAP bitmap = CreateDIBSection(hdc,
                                      reinterpret_cast<BITMAPINFO*>(&hdr), 0,
                                      &bits, NULL, 0);
    EXPECT_TRUE(bitmap);
    EXPECT_TRUE(SelectObject(hdc, bitmap));
    gfx::PlatformDeviceWin::InitializeDC(hdc);
    EXPECT_TRUE(emf.Playback(hdc, NULL));
    row_length_ = size_.width() * sizeof(uint32);
    size_t bytes = row_length_ * size_.height();
    ASSERT_TRUE(bytes);
    data_.resize(bytes);
    memcpy(&*data_.begin(), bits, bytes);
    DeleteDC(hdc);
    DeleteObject(bitmap);
  }

  // Pixel dimensions of the image.
  gfx::Size size_;

  // Length of a line in bytes.
  int row_length_;

  // Actual bitmap data in arrays of RGBAs (so when loaded as uint32, it's
  // 0xABGR).
  std::vector<unsigned char> data_;

  // Flag to signal if the comparison functions should ignore the alpha channel.
  const bool ignore_alpha_;

  DISALLOW_EVIL_CONSTRUCTORS(Image);
};

class PrintingLayoutTest : public PrintingTest<UITest> {
 public:
  PrintingLayoutTest() {
    emf_path_ = browser_directory_;
    file_util::AppendToPath(&emf_path_, L"emf_dumps");
    std::wstring arg(L" --debug-print=\"");
    arg += emf_path_;
    arg += L"\"";
    launch_arguments_.append(arg);
    show_window_ = true;
  }

  virtual void SetUp() {
    // Make sure there is no left overs.
    CleanupDumpDirectory();
    UITest::SetUp();
  }

  virtual void TearDown() {
    UITest::TearDown();
    file_util::Delete(emf_path_, true);
  }

 protected:
  void PrintNowTab() {
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    if (!tab_proxy.get())
      return;

    ASSERT_TRUE(tab_proxy->PrintNow());
  }

  // Finds the dump for the last print job and compares it to the data named
  // |verification_name|. Compares the saved printed job pixels with the test
  // data pixels and returns the percentage of different pixels; 0 for success,
  // ]0, 100] for failure.
  double CompareWithResult(const std::wstring& verification_name) {
    std::wstring test_result(ScanFiles(verification_name));
    if (test_result.empty()) {
      // 100% different, the print job buffer is not there.
      return 100.;
    }

    std::wstring verification_file(test_data_directory_);
    file_util::AppendToPath(&verification_file, L"printing");
    file_util::AppendToPath(&verification_file, verification_name);
    std::wstring emf(verification_file + L".emf");
    std::wstring png(verification_file + L".png");

    // Looks for Cleartype override.
    if (file_util::PathExists(verification_file + L"_cleartype.png") &&
        IsClearTypeEnabled()) {
      png = verification_file + L"_cleartype.png";
    }

    if (GenerateFiles()) {
      // Copy the .emf and generate an .png.
      file_util::CopyFile(test_result, emf);
      Image emf_content(emf);
      emf_content.SaveToPng(png);
      // Saving is always fine.
      return 0;
    } else {
      // File compare between test and result.
      Image emf_content(emf);
      Image test_content(test_result);
      Image png_content(png);
      double diff_emf = emf_content.PercentageDifferent(test_content);

      EXPECT_EQ(0., diff_emf) << verification_name <<
          L" original size:" << emf_content.size() <<
          L" result size:" << test_content.size();
      if (diff_emf) {
        // Backup the result emf file.
        file_util::CopyFile(test_result, verification_file + L"_failed.emf");
      }

      // This verification is only to know that the EMF rendering stays
      // immutable.
      double diff_png = emf_content.PercentageDifferent(png_content);
      EXPECT_EQ(0., diff_png) << verification_name <<
          L" original size:" << emf_content.size() <<
          L" result size:" << test_content.size();
      if (diff_png) {
        // Backup the rendered emf file to detect the rendering difference.
        emf_content.SaveToPng(verification_file + L"_rendering.png");
      }
      return std::max(diff_png, diff_emf);
    }
  }

  // Makes sure the directory exists and is empty.
  void CleanupDumpDirectory() {
    // Tries to delete the dumping directory for around 10 seconds.
    for (int i = 0; i < 100 && file_util::PathExists(emf_path()); ++i) {
      // It's fine fail sometimes because of opened left over .PRN file.
      // Explanation:
      // When calling PrintNowTab(), it makes sure the page is rendered and
      // sent to the spooler. It does *not* wait for the spooler to flush the
      // job. It is completely unnecessary to wait for that. So the printer
      // may write the file too late. Since the printer holds an exclusive
      // access to the file, it can't be deleted until the printer is done.
      if (file_util::Delete(emf_path(), true)) {
        break;
      }
      Sleep(100);
    }
    file_util::CreateDirectory(emf_path());
  }

  // Returns if Clear Type is currently enabled.
  static bool IsClearTypeEnabled() {
    BOOL ct_enabled = 0;
    if (SystemParametersInfo(SPI_GETCLEARTYPE, 0, &ct_enabled, 0) && ct_enabled)
      return true;
    UINT smoothing = 0;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothing, 0) &&
        smoothing == FE_FONTSMOOTHINGCLEARTYPE)
      return true;
    return false;
  }

 private:
  // Verifies that there is one .emf and one .prn file in the dump directory.
  // Returns the path of the .emf file and deletes the .prn file.
  std::wstring ScanFiles(const std::wstring& verification_name) {
    // Try to 10 seconds.
    std::wstring emf_file;
    std::wstring prn_file;
    bool found_emf = false;
    bool found_prn = false;
    for (int i = 0; i < 100; ++i) {
      file_util::FileEnumerator enumerator(emf_path(), false,
                                           file_util::FileEnumerator::FILES);
      emf_file.clear();
      prn_file.clear();
      found_emf = false;
      found_prn = false;
      std::wstring file;
      while (!(file = enumerator.Next()).empty()) {
        std::wstring ext = file_util::GetFileExtensionFromPath(file);
        if (!_wcsicmp(ext.c_str(), L"emf")) {
          EXPECT_FALSE(found_emf) << "Found a leftover .EMF file: \"" <<
              emf_file << "\" and \"" << file << "\" when looking for \"" <<
              verification_name << "\"";
          found_emf = true;
          emf_file = file;
          continue;
        }
        if (!_wcsicmp(ext.c_str(), L"prn")) {
          EXPECT_FALSE(found_prn) << "Found a leftover .PRN file: \"" <<
              prn_file << "\" and \"" << file << "\" when looking for \"" <<
              verification_name << "\"";
          prn_file = file;
          found_prn = true;
          file_util::Delete(file, false);
          continue;
        }
        EXPECT_TRUE(false);
      }
      if (found_emf && found_prn)
        break;
      Sleep(100);
    }
    EXPECT_TRUE(found_emf) << ".PRN file is: " << prn_file;
    EXPECT_TRUE(found_prn) << ".EMF file is: " << emf_file;
    return emf_file;
  }

  static bool GenerateFiles() {
    return CommandLine().HasSwitch(kGenerateSwitch);
  }

  const std::wstring& emf_path() const { return emf_path_; }

  std::wstring emf_path_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintingLayoutTest);
};

// Tests that don't need UI access.
class PrintingLayoutTestHidden : public PrintingLayoutTest {
 public:
  PrintingLayoutTestHidden() {
    show_window_ = false;
  }
};

class PrintingLayoutTextTest : public PrintingLayoutTest {
  typedef PrintingLayoutTest Parent;
 public:
  // Returns if the test is disabled.
  // TODO(maruel):  http://b/1157665 Until the issue is fixed, disable the test
  // if ClearType is enabled.
  static bool IsTestCaseDisabled() {
    return Parent::IsTestCaseDisabled() || IsClearTypeEnabled();
  }
};

// Dismiss the first dialog box child of owner_window by "executing" the
// default button.
class DismissTheWindow : public base::RefCountedThreadSafe<DismissTheWindow> {
 public:
  DismissTheWindow(DWORD owner_process)
      : owner_process_(owner_process),
        dialog_was_found_(false),
        dialog_window_(NULL),
        other_thread_(MessageLoop::current()),
        start_time_(Time::Now()) {
  }

  void Start() {
    timer_.Start(TimeDelta::FromMilliseconds(250), this,
                 &DismissTheWindow::DoTimeout);
  }

 private:
  void DoTimeout() {
    // A bit twisted code that runs in 2 passes or more. First it tries to find
    // a dialog box, if it finds it, it will execute the default action. If it
    // still works, it will loop again but then it will try to *not* find the
    // window. Once this is right, it will stop the timer and unlock the
    // other_thread_ message loop.
    if (!timer_.IsRunning())
      return;

    if (!dialog_window_) {
      HWND dialog_window = NULL;
      for (;;) {
        dialog_window = FindWindowEx(NULL,
                                     dialog_window,
                                     MAKEINTATOM(32770),
                                     NULL);
        if (!dialog_window)
          break;

        // In some corner case, the Print... dialog box may not have any owner.
        // Trap that case. Too bad if the user has a dialog opened.
        if (Time::Now() - start_time_ > TimeDelta::FromSeconds(3))
          break;

        DWORD process_id = 0;
        GetWindowThreadProcessId(dialog_window, &process_id);
        if (process_id == owner_process_)
          break;
      }
      if (dialog_window) {
        LRESULT res = SendMessage(dialog_window, DM_GETDEFID, 0, 0);
        if (!res)
          return;
        EXPECT_EQ(DC_HASDEFID, HIWORD(res));
        WORD print_button_id = LOWORD(res);
        res = SendMessage(
            dialog_window,
            WM_COMMAND,
            print_button_id,
            reinterpret_cast<LPARAM>(GetDlgItem(dialog_window, print_button_id)));
        // Try again.
        if (res)
          return;

        // Ok it succeeded.
        dialog_window_ = dialog_window;
        dialog_was_found_ = true;
        return;
      }
      if (!dialog_was_found_)
        return;
    }

    // Now verify that it indeed closed itself.
    if (!IsWindow(dialog_window_)) {
      timer_.Stop();
      // Unlock the other thread.
      other_thread_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    } else {
      // Maybe it's time to try to click it again. Restart from the begining.
      dialog_window_ = NULL;
    }
  }

  DWORD owner_process_;
  bool dialog_was_found_;
  HWND dialog_window_;
  MessageLoop* other_thread_;
  base::RepeatingTimer<DismissTheWindow> timer_;
  Time start_time_;
};

}  // namespace

// This test is disable because it fails. See bug 1353559.
TEST_F(PrintingLayoutTextTest, DISABLED_Complex) {
  if (IsTestCaseDisabled())
    return;

  // Print a document, check its output.
  TestServer server(kDocRoot);
  NavigateToURL(server.TestServerPage("files/printing/test1.html"));
  PrintNowTab();
  EXPECT_EQ(0., CompareWithResult(L"test1"));
}

struct TestPool {
  const wchar_t* source;
  const wchar_t* result;
};

const TestPool kTestPool[] = {
  // ImagesB&W
  L"files/printing/test2.html", L"test2",
  // ImagesTransparent
  L"files/printing/test3.html", L"test3",
  // ImageColor
  L"files/printing/test4.html", L"test4",
  // TODO(maruel):  http://b/1171450 Transparent overlays are drawn opaque
  // L"files/printing/test5.html", L"test5",
};

TEST_F(PrintingLayoutTestHidden, ManyTimes) {
  if (IsTestCaseDisabled())
    return;

  TestServer server(kDocRoot);
  ASSERT_GT(arraysize(kTestPool), 0u);
  for (int i = 0; i < arraysize(kTestPool); ++i) {
    if (i)
      CleanupDumpDirectory();
    const TestPool& test = kTestPool[i % arraysize(kTestPool)];
    NavigateToURL(server.TestServerPageW(test.source));
    PrintNowTab();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    PrintNowTab();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    PrintNowTab();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    PrintNowTab();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
  }
}

// Prints a popup and immediately closes it.
TEST_F(PrintingLayoutTest, DISABLED_Delayed) {
  if (IsTestCaseDisabled())
    return;
  // TODO(maruel):  This test is failing on Windows 2000. I haven't investigated
  // why.
  if (win_util::GetWinVersion() < win_util::WINVERSION_XP)
    return;

  TestServer server(kDocRoot);

  {
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    bool is_timeout = true;
    GURL url = server.TestServerPage("files/printing/popup_delayed_print.htm");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));


    scoped_ptr<base::Thread> worker(
        new base::Thread("PrintingLayoutTest_worker"));
    scoped_refptr<DismissTheWindow> dismiss_task =
        new DismissTheWindow(process_util::GetProcId(process()));
    // We need to start the thread to be able to set the timer.
    worker->Start();
    worker->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(dismiss_task.get(), &DismissTheWindow::Start));

    MessageLoop::current()->Run();

    worker->Stop();

    // Force a navigation elsewhere to verify that it's fine with it.
    url = server.TestServerPage("files/printing/test1.html");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));
  }
  CloseBrowserAndServer();

  EXPECT_EQ(0., CompareWithResult(L"popup_delayed_print"))
      << L"popup_delayed_print";
}

// Prints a popup and immediately closes it.
TEST_F(PrintingLayoutTest, DISABLED_IFrame) {
  if (IsTestCaseDisabled())
    return;

  TestServer server(kDocRoot);

  {
    scoped_ptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    GURL url = server.TestServerPage("files/printing/iframe.htm");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));

    scoped_ptr<base::Thread> worker(
        new base::Thread("PrintingLayoutTest_worker"));
    scoped_refptr<DismissTheWindow> dismiss_task =
        new DismissTheWindow(process_util::GetProcId(process()));
    // We need to start the thread to be able to set the timer.
    worker->Start();
    worker->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(dismiss_task.get(), &DismissTheWindow::Start));

    MessageLoop::current()->Run();

    worker->Stop();

    // Force a navigation elsewhere to verify that it's fine with it.
    url = server.TestServerPage("files/printing/test1.html");
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
              tab_proxy->NavigateToURL(url));
  }
  CloseBrowserAndServer();

  EXPECT_EQ(0., CompareWithResult(L"iframe"))
      << L"iframe";
}

