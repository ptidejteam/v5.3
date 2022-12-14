// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_H__
#define CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_H__

#include "chrome/browser/jsmessage_box_handler.h"

class JavascriptBeforeUnloadHandler : public JavascriptMessageBoxHandler {
 public:
  // This will display a modal dialog box with a header and footer asking the
  // the user if they wish to navigate away from a page, with additional text
  // |message_text| between the header and footer. The users response is
  // returned to the renderer using |reply_msg|.
  static void RunBeforeUnloadDialog(WebContents* web_contents,
                                    const std::wstring& message_text,
                                    IPC::Message* reply_msg);
  virtual ~JavascriptBeforeUnloadHandler() {}

  // views::DialogDelegate Methods:
  virtual std::wstring GetWindowTitle() const;
  virtual std::wstring GetDialogButtonLabel(DialogButton button) const;

 private:
  // Called from RunBeforeUnloadDialog. Calls JavascriptMessageBoxHandler's
  // constructor.
  JavascriptBeforeUnloadHandler(WebContents* web_contents,
                                const std::wstring& message_text,
                                IPC::Message* reply_msg);

  DISALLOW_EVIL_CONSTRUCTORS(JavascriptBeforeUnloadHandler);
};

#endif // CHROME_BROWSER_JS_BEFORE_UNLOAD_HANDLER_H__

