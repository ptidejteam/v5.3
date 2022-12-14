// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drag_utils.h"

#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "base/file_util.h"
#include "base/gfx/gdi_util.h"
#include "base/gfx/point.h"
#include "base/string_util.h"
#include "chrome/app/theme/theme_resources.h"
#include "chrome/browser/views/bookmark_bar_view.h"
#include "chrome/common/gfx/chrome_canvas.h"
#include "chrome/common/gfx/chrome_font.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/win_util.h"
#include "chrome/views/text_button.h"
#include "googleurl/src/gurl.h"

namespace drag_utils {

// Maximum width of the link drag image in pixels.
static const int kLinkDragImageMaxWidth = 200;
static const int kLinkDragImageVPadding = 3;
static const int kLinkDragImageVSpacing = 2;
static const int kLinkDragImageHPadding = 4;
static const SkColor kLinkDragImageBGColor = SkColorSetRGB(131, 146, 171);
//static const SkColor kLinkDragImageBGColor = SkColorSetRGB(195, 217, 255);
static const SkColor kLinkDragImageTextColor = SK_ColorBLACK;

// File dragging pixel measurements
static const int kFileDragImageMaxWidth = 200;
static const SkColor kFileDragImageTextColor = SK_ColorBLACK;

static void SetDragImageOnDataObject(HBITMAP hbitmap,
                                     int width,
                                     int height,
                                     int cursor_offset_x,
                                     int cursor_offset_y,
                                     IDataObject* data_object) {
  IDragSourceHelper* helper = NULL;
  HRESULT rv = CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
      IID_IDragSourceHelper, reinterpret_cast<LPVOID*>(&helper));
  if (SUCCEEDED(rv)) {
    SHDRAGIMAGE sdi;
    sdi.sizeDragImage.cx = width;
    sdi.sizeDragImage.cy = height;
    sdi.crColorKey = 0xFFFFFFFF;
    sdi.hbmpDragImage = hbitmap;
    sdi.ptOffset.x = cursor_offset_x;
    sdi.ptOffset.y = cursor_offset_y;
    helper->InitializeFromBitmap(&sdi, data_object);
  }
};

// Blit the contents of the canvas to a new HBITMAP. It is the caller's
// responsibility to release the |bits| buffer.
static HBITMAP CreateBitmapFromCanvas(const ChromeCanvas& canvas,
                                      int width,
                                      int height) {
  HDC screen_dc = GetDC(NULL);
  BITMAPINFOHEADER header;
  gfx::CreateBitmapHeader(width, height, &header);
  void* bits;
  HBITMAP bitmap =
      CreateDIBSection(screen_dc, reinterpret_cast<BITMAPINFO*>(&header),
                       DIB_RGB_COLORS, &bits, NULL, 0);
  HDC compatible_dc = CreateCompatibleDC(screen_dc);
  HGDIOBJ old_object = SelectObject(compatible_dc, bitmap);
  BitBlt(compatible_dc, 0, 0, width, height,
         canvas.getTopPlatformDevice().getBitmapDC(),
         0, 0, SRCCOPY);
  SelectObject(compatible_dc, old_object);
  ReleaseDC(NULL, compatible_dc);
  ReleaseDC(NULL, screen_dc);
  return bitmap;
}

void CreateDragImageForLink(const GURL& url,
                            const std::wstring& title,
                            IDataObject* data_object) {
  // First calculate our dimensions.
  ChromeFont title_font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont).
          DeriveFont(0, ChromeFont::BOLD);
  ChromeFont url_font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::SmallFont);
  const int title_height = title_font.height();
  const int url_height = url_font.height();
  const int width = kLinkDragImageMaxWidth;
  const int height = kLinkDragImageVPadding * 2 + kLinkDragImageVSpacing +
      title_height + url_height;

  // Now create a canvas and render the dragged representation into it.
  ChromeCanvas canvas(width, height, true);

  // Paint the alpha layer
  canvas.FillRectInt(SK_ColorWHITE, 0, 0,
                     kLinkDragImageMaxWidth, height); // kLinkDragImageTransparentColor

  // Paint the background
  SkPaint paint;
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(kLinkDragImageBGColor);
  SkRect bounds_rect;
  bounds_rect.set(0, 0, SkIntToScalar(width), SkIntToScalar(height));
  canvas.drawRoundRect(bounds_rect, 3, 3, paint);

  // Paint the link title
  canvas.DrawStringInt(title, title_font, kLinkDragImageTextColor,
                       kLinkDragImageHPadding, kLinkDragImageVPadding,
                       kLinkDragImageMaxWidth - 2 * kLinkDragImageHPadding,
                       title_height);

  // Paint the link URL
  canvas.DrawStringInt(UTF8ToWide(url.spec()), url_font,
                       kLinkDragImageTextColor, kLinkDragImageHPadding,
                       kLinkDragImageVPadding + title_height +
                           kLinkDragImageVSpacing,
                       kLinkDragImageMaxWidth - 2 * kLinkDragImageHPadding,
                       url_height);

  SetDragImageOnDataObject(canvas, kLinkDragImageMaxWidth, height,
                           kLinkDragImageMaxWidth / 2,
                           height - kLinkDragImageVPadding, data_object);
}

void SetURLAndDragImage(const GURL& url,
                        const std::wstring& title,
                        const SkBitmap& icon,
                        OSExchangeData* data) {
  DCHECK(url.is_valid() && data);

  data->SetURL(url, title);

  // Create a button to render the drag image for us.
  views::TextButton button(title.empty() ? UTF8ToWide(url.spec()) : title);
  button.set_max_width(BookmarkBarView::kMaxButtonWidth);
  if (icon.isNull()) {
    button.SetIcon(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
                   IDR_DEFAULT_FAVICON));
  } else {
    button.SetIcon(icon);
  }
  gfx::Size prefsize = button.GetPreferredSize();
  button.SetBounds(0, 0, prefsize.width(), prefsize.height());

  // Render the image.
  ChromeCanvas canvas(prefsize.width(), prefsize.height(), false);
  button.Paint(&canvas, true);
  SetDragImageOnDataObject(canvas, prefsize.width(), prefsize.height(),
                           prefsize.width() / 2, prefsize.height() / 2,
                           data);
}

void CreateDragImageForFile(const std::wstring& file_name,
                            SkBitmap* icon,
                            IDataObject* data_object) {
  DCHECK(icon);
  DCHECK(data_object);

  // Set up our text portion
  const std::wstring& name = file_util::GetFilenameFromPath(file_name);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ChromeFont font = rb.GetFont(ResourceBundle::BaseFont);

  const int width = kFileDragImageMaxWidth;
  const int height = font.height() + icon->height() + kLinkDragImageVPadding;
  ChromeCanvas canvas(width, height, false /* translucent */);

  // Paint the icon.
  canvas.DrawBitmapInt(*icon, (width - icon->width()) / 2, 0);

  // Paint the file name.
  canvas.DrawStringInt(name, font, kFileDragImageTextColor,
                       0, icon->height() + kLinkDragImageVPadding,
                       width, font.height(), ChromeCanvas::TEXT_ALIGN_CENTER);

  SetDragImageOnDataObject(canvas, width, height, width / 2,
                           kLinkDragImageVPadding, data_object);
}

void SetDragImageOnDataObject(const ChromeCanvas& canvas,
                              int width,
                              int height,
                              int cursor_x_offset,
                              int cursor_y_offset,
                              IDataObject* data_object) {
  DCHECK(data_object && width > 0 && height > 0);
  // SetDragImageOnDataObject(HBITMAP) takes ownership of the bitmap.
  HBITMAP bitmap = CreateBitmapFromCanvas(canvas, width, height);

  // Attach 'bitmap' to the data_object.
  SetDragImageOnDataObject(bitmap, width, height,
                           cursor_x_offset,
                           cursor_y_offset,
                           data_object);
}

} // namespace drag_utils

