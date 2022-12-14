// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_WIN_PRINTING_CONTEXT_H__
#define CHROME_BROWSER_PRINTING_WIN_PRINTING_CONTEXT_H__

#include <ocidl.h>
#include <commdlg.h>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/printing/print_settings.h"

namespace printing {

// Describe the user selected printing context for Windows. This includes the
// OS-dependent UI to ask the user about the print settings. This class directly
// talk to the printer and manages the document and pages breaks.
class PrintingContext {
 public:
  // Tri-state result for user behavior-dependent functions.
  enum Result {
    OK,
    CANCEL,
    FAILED,
  };

  PrintingContext();
  ~PrintingContext();

  // Asks the user what printer and format should be used to print. Updates the
  // context with the select device settings.
  Result AskUserForSettings(HWND window, int max_pages);

  // Selects the user's default printer and format. Updates the context with the
  // default device settings.
  Result UseDefaultSettings();

  // Initializes with predefined settings.
  Result InitWithSettings(const PrintSettings& settings);

  // Reinitializes the settings to uninitialized for object reuse.
  void ResetSettings();

  // Does platform specific setup of the printer before the printing. Signal the
  // printer that a document is about to be spooled.
  // Warning: This function enters a message loop. That may cause side effects
  // like IPC message processing! Some printers have side-effects on this call
  // like virtual printers that ask the user for the path of the saved document;
  // for example a PDF printer.
  Result NewDocument(const std::wstring& document_name);

  // Starts a new page.
  Result NewPage();

  // Closes the printed page.
  Result PageDone();

  // Closes the printing job. After this call the object is ready to start a new
  // document.
  Result DocumentDone();

  // Cancels printing. Can be used in a multithreaded context. Takes effect
  // immediately.
  void Cancel();

  // Dismiss the Print... dialog box if shown.
  void DismissDialog();

  HDC context() {
    return hdc_;
  }

  const PrintSettings& settings() const {
    return settings_;
  }

 private:
  // Class that manages the PrintDlgEx() callbacks. This is meant to be a
  // temporary object used during the Print... dialog display.
  class CallbackHandler;

  // Does bookeeping when an error occurs.
  PrintingContext::Result OnErrror();

  // Used in response to the user cancelling the printing.
  static BOOL CALLBACK AbortProc(HDC hdc, int nCode);

  // Reads the settings from the selected device context. Updates settings_ and
  // its margins.
  bool InitializeSettings(const DEVMODE& dev_mode,
                          const std::wstring& new_device_name,
                          const PRINTPAGERANGE* ranges,
                          int number_ranges);

  // Retrieves the printer's default low-level settings. hdc_ is allocated with
  // this call.
  bool GetPrinterSettings(HANDLE printer,
                          const std::wstring& device_name);

  // Allocates the HDC for a specific DEVMODE.
  bool AllocateContext(const std::wstring& printer_name,
                       const DEVMODE* dev_mode);

  // Parses the result of a PRINTDLGEX result.
  Result ParseDialogResultEx(const PRINTDLGEX& dialog_options);
  Result ParseDialogResult(const PRINTDLG& dialog_options);

  // The selected printer context.
  HDC hdc_;

  // Complete print context settings.
  PrintSettings settings_;

#ifdef _DEBUG
  // Current page number in the print job.
  int page_number_;
#endif

  // The dialog box for the time it is shown.
  volatile HWND dialog_box_;

  // The dialog box has been dismissed.
  volatile bool dialog_box_dismissed_;

  // Is a print job being done.
  volatile bool in_print_job_;

  // Did the user cancel the print job.
  volatile bool abort_printing_;

  DISALLOW_EVIL_CONSTRUCTORS(PrintingContext);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_WIN_PRINTING_CONTEXT_H__
