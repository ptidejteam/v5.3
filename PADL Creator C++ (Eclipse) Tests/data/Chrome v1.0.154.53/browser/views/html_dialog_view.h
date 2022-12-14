// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H__
#define CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H__

#include <string>

#include "chrome/browser/dom_ui/html_dialog_contents.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/browser/views/dom_view.h"

namespace views {
class Window;
}

////////////////////////////////////////////////////////////////////////////////
//
// HtmlDialogView is a view used to display an HTML dialog to the user. The
// content of the dialogs is determined by the delegate
// (HtmlDialogContentsDelegate), but is basically a file URL along with a
// JSON input string. The HTML is supposed to show a UI to the user and is
// expected to send back a JSON file as a return value.
//
////////////////////////////////////////////////////////////////////////////////
class HtmlDialogView
  : public DOMView,
    public HtmlDialogContentsDelegate,
    public TabContentsDelegate {
 public:
  HtmlDialogView(Browser* parent_browser,
                 Profile* profile,
                 HtmlDialogContentsDelegate* delegate);
  virtual ~HtmlDialogView();

  // Initializes the contents of the dialog (the DOMView and the callbacks).
  void InitDialog();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::WindowDelegate:
  virtual bool CanResize() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

  // Overridden from HtmlDialogContentsDelegate:
  virtual GURL GetDialogContentURL() const;
  virtual void GetDialogSize(CSize* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);

 private:
  // The Browser object which created this html dialog; we send all
  // window opening/navigations to this object.
  Browser* parent_browser_;

  Profile* profile_;

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  HtmlDialogContentsDelegate* delegate_;

  DISALLOW_EVIL_CONSTRUCTORS(HtmlDialogView);
};

#endif  // CHROME_BROWSER_VIEWS_HTML_DIALOG_VIEW_H__

