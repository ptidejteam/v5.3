// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_UTIL_H__
#define CHROME_BROWSER_TAB_UTIL_H__

class TabContents;
class URLRequest;

namespace tab_util {

// Helper to get the IDs necessary for looking up a TabContents.
// Should only be called from the IO thread, since it accesses an URLRequest.
bool GetTabContentsID(URLRequest* request, int* render_process_host_id,
                      int* routing_id);

// Helper to find the TabContents that originated the given request. Can be
// NULL if the tab has been closed or some other error occurs.
// Should only be called from the UI thread, since it accesses TabContent.
TabContents* GetTabContentsByID(int render_process_host_id, int routing_id);

}  // namespace tab_util

#endif  // CHROME_BROWSER_TAB_UTIL_H__

