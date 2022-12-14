// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTED_PAGES_SOURCE_H__
#define CHROME_BROWSER_PRINTING_PRINTED_PAGES_SOURCE_H__

#include <string>

class GURL;
class MessageLoop;

namespace printing {

class PrintedDocument;

// Source of printed pages.
class PrintedPagesSource {
 public:
  // Renders a printed page. It is not necessary to be synchronous. It must call
  // document->SetPage() once the source is done rendering the requested page.
  virtual void RenderOnePrintedPage(PrintedDocument* document,
                                    int page_number) = 0;

  // Returns the document title.
  virtual std::wstring RenderSourceName() = 0;

  // Returns the URL's source of the document if applicable.
  virtual GURL RenderSourceUrl() = 0;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTED_PAGES_SOURCE_H__

