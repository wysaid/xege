﻿/*
* EGE (Easy Graphics Engine)
* filename  image.cpp

本文件集中所有对image基本操作的接口和类定义
*/

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include "image.h"

#include "ege_head.h"
#include "ege_common.h"
#include "ege_dllimport.h"

// #ifdef _ITERATOR_DEBUG_LEVEL
// #undef _ITERATOR_DEBUG_LEVEL
// #endif

#include <png.h>


#include <math.h>

namespace ege
{

void IMAGE::reset()
{
    m_initflag  = IMAGE_INIT_FLAG;
    m_hDC       = NULL;
    m_hBmp      = NULL;
    m_width     = 0;
    m_height    = 0;
    m_pBuffer   = NULL;
    m_color     = 0;
    m_fillcolor = 0;
    m_aa        = false;
    memset(&m_vpt, 0, sizeof(m_vpt));
    memset(&m_texttype, 0, sizeof(m_texttype));
    memset(&m_linestyle, 0, sizeof(m_linestyle));
    m_linewidth = 0.0f;
    m_bk_color  = 0;
    m_texture   = NULL;
#ifdef EGE_GDIPLUS
    m_graphics = NULL;
    m_pen      = NULL;
    m_brush    = NULL;
#endif
}

void IMAGE::construct(int width, int height)
{
    HDC refDC = NULL;

    if (graph_setting.hwnd) {
        refDC = ::GetDC(graph_setting.hwnd);
    }

    dll::loadDllsIfNot();
    gdipluinit();
    reset();
    initimage(refDC, width, height);
    setdefaultattribute();

    if (refDC) {
        ::ReleaseDC(graph_setting.hwnd, refDC);
    }
}

IMAGE::IMAGE()
{
    construct(1, 1);
}

IMAGE::IMAGE(int width, int height)
{
    // 截止到 0
    if (width < 0) {
        width = 0;
    }
    if (height < 0) {
        height = 0;
    }
    construct(width, height);
}

IMAGE::IMAGE(const IMAGE& img)
{
    reset();
    initimage(img.m_hDC, img.m_width, img.m_height);
    setdefaultattribute();
    BitBlt(m_hDC, 0, 0, img.m_width, img.m_height, img.m_hDC, 0, 0, SRCCOPY);
}

IMAGE::~IMAGE()
{
    gentexture(false);
    deleteimage();
}

void IMAGE::inittest(const WCHAR* strCallFunction) const
{
    if (m_initflag != IMAGE_INIT_FLAG) {
        WCHAR str[60];
        wsprintfW(str, L"Fatal error: read/write at 0x%p. At function '%s'", this, strCallFunction);
        MessageBoxW(graph_setting.hwnd, str, L"EGE ERROR message", MB_ICONSTOP);
        ExitProcess((UINT)grError);
    }
}

void IMAGE::gentexture(bool gen)
{
    if (!gen) {
        if (m_texture != NULL) {
            delete (Gdiplus::Bitmap*)m_texture;
            m_texture = NULL;
        }
    } else {
        if (m_texture != NULL) {
            gentexture(true);
        }
        Gdiplus::Bitmap* bitmap =
            new Gdiplus::Bitmap(getwidth(), getheight(), getwidth() * 4, PixelFormat32bppARGB, (BYTE*)getbuffer());
        m_texture = bitmap;
    }
}

int IMAGE::deleteimage()
{
#ifdef EGE_GDIPLUS
    if (NULL != m_graphics) {
        delete m_graphics;
    }
    m_graphics = NULL;
    if (NULL != m_pen) {
        delete m_pen;
    }
    m_pen = NULL;
    if (NULL != m_brush) {
        delete m_brush;
    }
    m_brush = NULL;
#endif

    HBITMAP hbmp  = (HBITMAP)GetCurrentObject(m_hDC, OBJ_BITMAP);
    HBRUSH  hbr   = (HBRUSH)GetCurrentObject(m_hDC, OBJ_BRUSH);
    HPEN    hpen  = (HPEN)GetCurrentObject(m_hDC, OBJ_PEN);
    HFONT   hfont = (HFONT)GetCurrentObject(m_hDC, OBJ_FONT);

    DeleteDC(m_hDC);
    m_hDC = NULL;

    DeleteObject(hbmp);
    DeleteObject(hbr);
    DeleteObject(hpen);
    DeleteObject(hfont);

    return 0;
}

HBITMAP newbitmap(int width, int height, PDWORD* p_bmp_buf)
{
    HBITMAP    bitmap;
    BITMAPINFO bmi = {{0}};
    PDWORD     bmp_buf;

    // 截止到 1
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }

    bmi.bmiHeader.biSize      = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth     = width;
    bmi.bmiHeader.biHeight    = -height;
    bmi.bmiHeader.biPlanes    = 1;
    bmi.bmiHeader.biBitCount  = 32;
    bmi.bmiHeader.biSizeImage = width * height * 4;

    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (VOID**)&bmp_buf, NULL, 0);

    if (p_bmp_buf) {
        *p_bmp_buf = bmp_buf;
    }

    return bitmap;
}

void IMAGE::initimage(HDC refDC, int width, int height)
{
    HDC     dc = CreateCompatibleDC(refDC);
    PDWORD  bmp_buf;
    HBITMAP bitmap = newbitmap(width, height, &bmp_buf);

    if (bitmap == NULL) {
        internal_panic(L"Fatal Error: Create Bitmap fail in 'IMAGE::initimage'");
    }

    ::SelectObject(dc, bitmap);

    m_hDC     = dc;
    m_hBmp    = bitmap;
    m_width   = width;
    m_height  = height;
    m_pBuffer = bmp_buf;

    setviewport(0, 0, m_width, m_height, 1, this);
}

void IMAGE::setdefaultattribute()
{
    setcolor(LIGHTGRAY, this);
    setbkcolor(BLACK, this);
    SetBkMode(m_hDC, OPAQUE); // TRANSPARENT);
    setfillstyle(SOLID_FILL, 0, this);
    setlinestyle(PS_SOLID, 0, 1, this);
    settextjustify(LEFT_TEXT, TOP_TEXT, this);
    setfont(16, 0, "SimSun", this);
    enable_anti_alias(false);
}

#ifdef EGE_GDIPLUS

Gdiplus::Graphics* IMAGE::getGraphics()
{
    if (NULL == m_graphics) {
        m_graphics = new Gdiplus::Graphics(m_hDC);
        m_graphics->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        m_graphics->SetSmoothingMode(m_aa ? Gdiplus::SmoothingModeAntiAlias : Gdiplus::SmoothingModeNone);
        m_graphics->SetTextRenderingHint(
            m_aa ? Gdiplus::TextRenderingHintAntiAlias : Gdiplus::TextRenderingHintSystemDefault);
    }
    return m_graphics;
}

Gdiplus::Pen* IMAGE::getPen()
{
    if (NULL == m_pen) {
        m_pen = new Gdiplus::Pen(m_color, m_linewidth);
        m_pen->SetDashStyle(linestyle_to_dashstyle(m_linestyle.linestyle));
    }
    return m_pen;
}

Gdiplus::Brush* IMAGE::getBrush()
{
    if (NULL == m_brush) {
        m_brush = new Gdiplus::SolidBrush(m_fillcolor);
    }
    return m_brush;
}

void IMAGE::set_pattern(Gdiplus::Brush* brush)
{
    if (NULL != m_brush) {
        delete m_brush;
    }
    m_brush = brush;
}
#endif

void IMAGE::enable_anti_alias(bool enable)
{
    m_aa = enable;
#ifdef EGE_GDIPLUS
    if (NULL != m_graphics) {
        m_graphics->SetSmoothingMode(m_aa ? Gdiplus::SmoothingModeAntiAlias : Gdiplus::SmoothingModeNone);
        m_graphics->SetTextRenderingHint(
            m_aa ? Gdiplus::TextRenderingHintAntiAlias : Gdiplus::TextRenderingHintSystemDefault);
    }
#endif
}

int IMAGE::resize_f(int width, int height)
{
    inittest(L"IMAGE::resize_f");

    // 截止到 0
    if (width < 0) {
        width = 0;
    }
    if (height < 0) {
        height = 0;
    }

    if (width == m_width && height == m_height) {
        setviewport(0, 0, m_width, m_height, 1, this);
        return 0;
    }

    PDWORD  bmp_buf;
    HBITMAP bitmap     = newbitmap(width, height, &bmp_buf);
    HBITMAP old_bitmap = (HBITMAP)SelectObject(this->m_hDC, bitmap);
    DeleteObject(old_bitmap);

    m_hBmp    = bitmap;
    m_width   = width;
    m_height  = height;
    m_pBuffer = bmp_buf;

    // BITMAP 更换后需重新创建 Graphics 对象(否则会在已销毁的 old_bitmap 上绘制，引发异常)
    if (m_graphics != NULL) {
        Gdiplus::Graphics* newGraphics = recreateGdiplusGraphics(m_hDC, m_graphics);
        delete m_graphics;
        m_graphics = newGraphics;
    }

    setviewport(0, 0, m_width, m_height, 1, this);

    return 0;
}

int IMAGE::resize(int width, int height)
{
    inittest(L"IMAGE::resize");
    int ret = this->resize_f(width, height);
    cleardevice(this);
    return ret;
}

IMAGE& IMAGE::operator=(const IMAGE& img)
{
    inittest(L"IMAGE::operator=");
    this->copyimage(&img);
    return *this;
}

void IMAGE::copyimage(PCIMAGE pSrcImg)
{
    inittest(L"IMAGE::copyimage");
    PCIMAGE img = CONVERT_IMAGE_CONST(pSrcImg);
    this->getimage(img, 0, 0, img->getwidth(), img->getheight());
    CONVERT_IMAGE_END;
}

int IMAGE::getimage(PCIMAGE pSrcImg, int xSrc, int ySrc, int srcWidth, int srcHeight)
{
    inittest(L"IMAGE::getimage");
    PCIMAGE img = CONVERT_IMAGE_CONST(pSrcImg);
    this->resize_f(srcWidth, srcHeight);
    BitBlt(this->m_hDC, 0, 0, srcWidth, srcHeight, img->m_hDC, xSrc, ySrc, SRCCOPY);
    CONVERT_IMAGE_END;
    return grOk;
}

int IMAGE::getimage(int xSrc, int ySrc, int srcWidth, int srcHeight)
{
    PIMAGE img = CONVERT_IMAGE_CONST(0);
    this->getimage(img, xSrc, ySrc, srcWidth, srcHeight);
    CONVERT_IMAGE_END;
    return grOk;
}

void IMAGE::putimage(
    PIMAGE imgDest, int xDest, int yDest, int widthDest, int heightDest, int xSrc, int ySrc, DWORD dwRop) const
{
    inittest(L"IMAGE::putimage");
    PIMAGE img = CONVERT_IMAGE(imgDest);
    BitBlt(img->m_hDC, xDest, yDest, widthDest, heightDest, m_hDC, xSrc, ySrc, dwRop);
    CONVERT_IMAGE_END;
}

void IMAGE::putimage(PIMAGE imgDest, int xDest, int yDest, DWORD dwRop) const
{
    this->putimage(imgDest, xDest, yDest, m_width, m_height, 0, 0, dwRop);
}

void IMAGE::putimage(int xDest, int yDest, int widthDest, int heightDest, int xSrc, int ySrc, DWORD dwRop) const
{
    PIMAGE img = CONVERT_IMAGE(0);
    this->putimage(img, xDest, yDest, widthDest, heightDest, xSrc, ySrc, dwRop);
    CONVERT_IMAGE_END;
}

void IMAGE::putimage(int xDest, int yDest, DWORD dwRop) const
{
    PIMAGE img = CONVERT_IMAGE(0);
    this->putimage(img, xDest, yDest, dwRop);
    CONVERT_IMAGE_END;
}

int IMAGE::getimage(const char* filename, int zoomWidth, int zoomHeight)
{
    const std::wstring& filename_w = mb2w(filename);
    return getimage(filename_w.c_str(), zoomWidth, zoomHeight);
}

int getimage_from_bitmap(PIMAGE pimg, Gdiplus::Bitmap& bitmap)
{
    Gdiplus::PixelFormat srcPixelFormat = bitmap.GetPixelFormat();

    // 将图像尺寸调整至和 bitmap 一致
    int width  = bitmap.GetWidth();
    int height = bitmap.GetHeight();
    resize_f(pimg, width, height);

    // 设置外部缓冲区属性，指向图像缓冲区首地址
    Gdiplus::BitmapData bitmapData;
    bitmapData.Width       = width;
    bitmapData.Height      = height;
    bitmapData.Stride      = width * sizeof(color_t);    // 至下一行像素的偏移量(字节)
    bitmapData.PixelFormat = PixelFormat32bppARGB;       // 像素颜色格式: 32 位 ARGB
    bitmapData.Scan0       = getbuffer(pimg);            // 图像首行像素的首地址

    // 读取区域设置为整个图像
    Gdiplus::Rect rect(0, 0, width, height);

    // 模式: 仅读取图像数据, 缓冲区由用户分配
    int imageLockMode = Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeUserInputBuf;

    //读取 Bitmap 图像内容，以 32 位 ARGB 的像素格式写入缓冲区
    bitmap.LockBits(&rect, imageLockMode, PixelFormat32bppARGB, &bitmapData);

    // 解除锁定(如果设置了 ImageLockModeWrite 模式，还会将缓冲区内容复制到 Bitmap)
    bitmap.UnlockBits(&bitmapData);

    return grOk;
}

int IMAGE::getimage(const wchar_t* filename, int zoomWidth, int zoomHeight)
{
    (void)zoomWidth, (void)zoomHeight; // ignore
    inittest(L"IMAGE::getimage");
    {
        int ret = getimage_pngfile(this, filename);
        // grIOerror means it's not a png file
        if (ret != grIOerror) {
            return ret;
        }
    }

    OLECHAR          wszPath[MAX_PATH * 2 + 1];
    WCHAR            szPath[MAX_PATH * 2 + 1] = L"";

    if (wcsstr(filename, L"http://")) {
        lstrcpyW(szPath, filename);
    } else if (filename[1] == ':') {
        lstrcpyW(szPath, filename);
    } else {
        GetCurrentDirectoryW(MAX_PATH * 2, szPath);
        lstrcatW(szPath, L"\\");
        lstrcatW(szPath, filename);
    }

    lstrcpyW(wszPath, szPath);

    Gdiplus::Bitmap bitmap(wszPath);

    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        return grIOerror;
    }

    getimage_from_bitmap(this, bitmap);

    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        return grError;
    }

    return grOk;
}

int IMAGE::saveimage(const char* filename, bool withAlphaChannel) const
{
    return saveimage(mb2w(filename).c_str(), withAlphaChannel);
}

int IMAGE::saveimage(const wchar_t* filename, bool withAlphaChannel) const
{
    return ege::saveimage(this, filename, withAlphaChannel);
}

void getimage_from_png_struct(PIMAGE self, void* vpng_ptr, void* vinfo_ptr)
{
    png_structp png_ptr  = (png_structp)vpng_ptr;
    png_infop   info_ptr = (png_infop)vinfo_ptr;

    #if defined(PNG_SKIP_sRGB_CHECK_PROFILE) && defined(PNG_SET_OPTION_SUPPORTED)
    png_set_option(png_ptr, PNG_SKIP_sRGB_CHECK_PROFILE,
           PNG_OPTION_ON);
    #endif

    // 读取 PNG 文件信息, 存入 info_ptr 中
    png_read_info(png_ptr, info_ptr);

    const png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    const png_byte bit_depth  = png_get_bit_depth(png_ptr, info_ptr);    // 每通道位深度

    // 将颜色类型转为 RGB 或 RGBA
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    } else if ((color_type == PNG_COLOR_TYPE_GRAY) && (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)) {
        png_set_gray_to_rgb(png_ptr);
    }

    // 将位深度设置为每通道 8 bit
    // (PNG 格式规定，RGB(RGBA) 位深度为 8 或 16，不会低于 8)
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);  // bit depth 16 to 8
    }

    // 补齐 alpha 通道
    if ((color_type != PNG_COLOR_TYPE_RGB_ALPHA) && (color_type != PNG_COLOR_TYPE_GRAY_ALPHA)) {
        // 若在 tRNS 块中存在透明度信息则将其转为 alpha 通道
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png_ptr);
        } else {
            // 添加 alpha 通道(最高字节)并以 0xFF 填充
            png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
        }
    }

    // 调整通道存储顺序，字节从低到高依次为 B, G, R, A
    png_set_bgr(png_ptr);

    // 根据以上设置的目标格式更新 png_info 结构
    png_read_update_info(png_ptr, info_ptr);

    // 经过以上设置，像素格式为 ARGB (字节顺序从低至高依次为 B, G, R, A), 位深度为 8 (每通道)

    const png_uint_32 width  = png_get_image_width(png_ptr, info_ptr);
    const png_uint_32 height = png_get_image_height(png_ptr, info_ptr);

    // 将图像宽高调整至与 PNG 图片一致
    self->resize_f((int)width, (int)height);

    // 创建 png_bytep 数组，指向图像缓冲区中每一行像素的首地址
    const png_bytepp row_pointers = new png_bytep[height];
    color_t* buffer = (color_t*)self->m_pBuffer;

    for (png_uint_32 i = 0; i < height; i++) {
        row_pointers[i] = (png_bytep)(&buffer[i * width]);
    }

    // 读取图像数据，存入 row_pointers 所指向的内存区域
    png_read_image(png_ptr, row_pointers);

    delete[] row_pointers;
}

int IMAGE::getpngimg(FILE* fp)
{
    png_structp png_ptr;
    png_infop   info_ptr;

    {
        char   header[16];
        uint32 number = 8;
        fread(header, 1, number, fp);
        int isn_png = png_sig_cmp((png_const_bytep)header, 0, number);

        if (isn_png) {
            return grIOerror;
        }
        fseek(fp, 0, SEEK_SET);
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        return grAllocError;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return grAllocError;
    }

    png_init_io(png_ptr, fp);
    getimage_from_png_struct(this, png_ptr, info_ptr);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return grOk;
}

int IMAGE::savepngimg(FILE* fp, bool withAlphaChannel) const
{
    unsigned long i, j;
    png_structp   png_ptr;
    png_infop     info_ptr;
    png_colorp    palette;
    png_byte*     image;
    png_bytep*    row_pointers;
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    uint32 pixelsize = withAlphaChannel ? 4 : 3;
    uint32 width = m_width, height = m_height;

    if (png_ptr == NULL) {
        return -1;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, NULL);
        return -1;
    }

    /*if (setjmp(png_jmpbuf(png_ptr)))
    {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return -1;
    }// */

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, width, height, 8, withAlphaChannel ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    palette = (png_colorp)png_malloc(png_ptr, PNG_MAX_PALETTE_LENGTH * sizeof(png_color));
    png_set_PLTE(png_ptr, info_ptr, palette, PNG_MAX_PALETTE_LENGTH);

    png_write_info(png_ptr, info_ptr);

    png_set_packing(png_ptr);

    image = (png_byte*)malloc(width * height * pixelsize * sizeof(png_byte) + 4);
    if (image == NULL) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return -1;
    }

    row_pointers = (png_bytep*)malloc(height * sizeof(png_bytep));
    if (row_pointers == NULL) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        free(image);
        image = NULL;
        return -1;
    }

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; ++j) {
            (DWORD&)image[(i * width + j) * pixelsize] = RGBTOBGR(m_pBuffer[i * width + j]);
        }
        row_pointers[i] = (png_bytep)image + i * width * pixelsize;
    }

    png_write_image(png_ptr, row_pointers);

    png_write_end(png_ptr, info_ptr);

    png_free(png_ptr, palette);
    palette = NULL;

    png_destroy_write_struct(&png_ptr, &info_ptr);

    free(row_pointers);
    row_pointers = NULL;

    free(image);
    image = NULL;

    return 0;
}

struct MemCursor
{
    png_const_bytep data;
    png_size_t      size;
    png_size_t      cur;
};

static void read_data_from_MemCursor(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead)
{
    MemCursor& cursor = *(MemCursor*)png_get_io_ptr(png_ptr);
    png_size_t count  = MIN(cursor.size - cursor.cur, byteCountToRead);
    memcpy(outBytes, cursor.data + cursor.cur, count);
    cursor.cur += count;
}

inline int getimage_from_resource(PIMAGE self, HRSRC hrsrc)
{
    if (hrsrc) {
        HGLOBAL hg     = LoadResource(0, hrsrc);
        DWORD   dwSize = SizeofResource(0, hrsrc);
        LPVOID  pvRes  = LockResource(hg);

        if (dwSize >= 8 && png_check_sig((png_const_bytep)pvRes, 8)) {
            png_structp png_ptr;
            png_infop   info_ptr;
            MemCursor   cursor;
            cursor.data = (png_const_bytep)pvRes;
            cursor.size = dwSize;
            cursor.cur  = 0;

            png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
            if (png_ptr == NULL) {
                return grAllocError;
            }
            info_ptr = png_create_info_struct(png_ptr);
            if (info_ptr == NULL) {
                png_destroy_read_struct(&png_ptr, NULL, NULL);
                return grAllocError;
            }

            png_set_read_fn(png_ptr, &cursor, read_data_from_MemCursor);
            getimage_from_png_struct(self, png_ptr, info_ptr);

            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        } else {
            HGLOBAL          hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwSize);
            LPVOID           pvData;
            IStream*         pStm;

            if (hGlobal == NULL || (pvData = GlobalLock(hGlobal)) == NULL) {
                return grAllocError;
            }
            memcpy(pvData, pvRes, dwSize);
            GlobalUnlock(hGlobal);
            if (S_OK != dll::CreateStreamOnHGlobal(hGlobal, TRUE, &pStm)) {
                return grNullPointer;
            }

            Gdiplus::Bitmap bitmap(pStm);

            int status = bitmap.GetLastStatus();
            if (status != Gdiplus::Ok) {
                return grError;
            }

            getimage_from_bitmap(self, bitmap);

            // GlobalFree 不能在 Bitmap 读取完成之前调用
            // 否则 Bitmap::LockBits() 返回 Win32Error 错误码
            GlobalFree(hGlobal);
        }

        return grOk;
    }

    return grIOerror;
}

int IMAGE::getimage(const char* resType, const char* resName, int zoomWidth, int zoomHeight)
{
    const std::wstring& pResType_w = mb2w(resType);
    const std::wstring& pResName_w = mb2w(resName);
    return getimage(pResType_w.c_str(), pResName_w.c_str(), zoomWidth, zoomHeight);
}

int IMAGE::getimage(const wchar_t* resType, const wchar_t* resName, int zoomWidth, int zoomHeight)
{
    (void)zoomWidth, (void)zoomHeight; // ignore
    inittest(L"IMAGE::getimage");
    struct _graph_setting* pg    = &graph_setting;
    HRSRC                  hrsrc = FindResourceW(pg->instance, resName, resType);
    return getimage_from_resource(this, hrsrc);
}

int IMAGE::getimage(void* pMem, long size)
{
    inittest(L"IMAGE::getimage");

    if (pMem) {
        DWORD            dwSize  = size;
        HGLOBAL          hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwSize);
        LPVOID           pvData;
        IStream*         pStm;

        if (hGlobal == NULL || (pvData = GlobalLock(hGlobal)) == NULL) {
            return grAllocError;
        }
        memcpy(pvData, pMem, dwSize);
        GlobalUnlock(hGlobal);
        if (S_OK != dll::CreateStreamOnHGlobal(hGlobal, TRUE, &pStm)) {
            return grNullPointer;
        }

        Gdiplus::Bitmap bitmap(pStm);

        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            return grError;
        }

        getimage_from_bitmap(this, bitmap);

        // GlobalFree 不能在 Bitmap 读取完成之前调用
        // 否则 Bitmap::LockBits() 返回 Win32Error 错误码
        GlobalFree(hGlobal);

        return grOk;
    }

    return grIOerror;
}

void IMAGE::putimage(PIMAGE imgDest, int xDest, int yDest, int widthDest, int heightDest, int xSrc, int ySrc, int srcWidth,
    int srcHeight, DWORD dwRop) const
{
    inittest(L"IMAGE::putimage");
    const PCIMAGE img = CONVERT_IMAGE(imgDest);
    if (img) {
        SetStretchBltMode(img->m_hDC, COLORONCOLOR);
        StretchBlt(img->m_hDC, xDest, yDest, widthDest, heightDest, m_hDC, xSrc, ySrc, srcWidth, srcHeight, dwRop);
    }
    CONVERT_IMAGE_END;
}

/* private function */
static void fix_rect_1size(PCIMAGE pdest, PCIMAGE psrc, int* xDest, int* yDest,
        int* xSrc, int* ySrc, int* widthSrc, int* heightSrc)
{
    /* prepare viewport region and carry out coordinate transformation */
    struct viewporttype _vpt  = pdest->m_vpt;
    *xDest            += _vpt.left;
    *yDest            += _vpt.top;
    /* default value proc */
    if (*widthSrc == 0) {
        *widthSrc  = psrc->m_width;
        *heightSrc = psrc->m_height;
    }
    /* fix src rect */
    if (*widthSrc > psrc->m_width) {
        *widthSrc -= *widthSrc - psrc->m_width;
        *widthSrc  = psrc->m_width;
    }
    if (*heightSrc > psrc->m_height) {
        *heightSrc -= *heightSrc - psrc->m_height;
        *heightSrc  = psrc->m_height;
    }
    if (*xSrc < 0) {
        *widthSrc    += *xSrc;
        *xDest += *xSrc;
        *xSrc   = 0;
    }
    if (*ySrc < 0) {
        *heightSrc   += *ySrc;
        *yDest += *ySrc;
        *ySrc   = 0;
    }
    /* fix dest vpt rect */
    if (*xDest < _vpt.left) {
        int dx         = _vpt.left - *xDest;
        *xDest += dx;
        *xSrc  += dx;
        *widthSrc    -= dx;
    }
    if (*yDest < _vpt.top) {
        int dy         = _vpt.top - *yDest;
        *yDest += dy;
        *ySrc  += dy;
        *heightSrc   -= dy;
    }
    if (*xDest + *widthSrc > _vpt.right) {
        int dx      = *xDest + *widthSrc - _vpt.right;
        *widthSrc -= dx;
    }
    if (*yDest + *heightSrc > _vpt.bottom) {
        int dy       = *yDest + *heightSrc - _vpt.bottom;
        *heightSrc -= dy;
    }
}

int IMAGE::putimage_transparent(PIMAGE imgDest,           // handle to dest
    int                                xDest,             // x-coord of destination upper-left corner
    int                                yDest,             // y-coord of destination upper-left corner
    color_t                            transparentColor,  // color to make transparent
    int                                xSrc,              // x-coord of source upper-left corner
    int                                ySrc,              // y-coord of source upper-left corner
    int                                widthSrc,          // width of source rectangle
    int                                heightSrc          // height of source rectangle
) const
{
    inittest(L"IMAGE::putimage_transparent");
    const PIMAGE img = CONVERT_IMAGE(imgDest);
    if (img) {
        PCIMAGE imgSrc = this;
        int     y, x;
        DWORD   ddx, dsx;
        DWORD * pdp, *psp, cr;
        // fix rect
        fix_rect_1size(img, imgSrc, &xDest, &yDest, &xSrc, &ySrc, &widthSrc, &heightSrc);
        // draw
        pdp = img->m_pBuffer + yDest * img->m_width + xDest;
        psp = imgSrc->m_pBuffer + ySrc * imgSrc->m_width + xSrc;
        ddx = img->m_width - widthSrc;
        dsx = imgSrc->m_width - widthSrc;
        cr  = transparentColor & 0x00FFFFFF;
        for (y = 0; y < heightSrc; ++y) {
            for (x = 0; x < widthSrc; ++x, ++psp, ++pdp) {
                if ((*psp & 0x00FFFFFF) != cr) {
                    *pdp = EGECOLORA(*psp, EGEGET_A(*pdp));
                }
            }
            pdp += ddx;
            psp += dsx;
        }
    }
    CONVERT_IMAGE_END;
    return grOk;
}

int IMAGE::putimage_alphablend(PIMAGE imgDest,  // handle to dest
    int                               xDest,    // x-coord of destination upper-left corner
    int                               yDest,    // y-coord of destination upper-left corner
    unsigned char                     alpha,    // alpha
    int                               xSrc,     // x-coord of source upper-left corner
    int                               ySrc,     // y-coord of source upper-left corner
    int                               widthSrc, // width of source rectangle
    int                               heightSrc // height of source rectangle
) const
{
    inittest(L"IMAGE::putimage_alphablend");
    const PIMAGE img = CONVERT_IMAGE(imgDest);
    if (img) {
        PCIMAGE imgSrc = this;
        int     y, x;
        DWORD   ddx, dsx;
        DWORD * pdp, *psp;
        // fix rect
        fix_rect_1size(img, imgSrc, &xDest, &yDest, &xSrc, &ySrc, &widthSrc, &heightSrc);
        // draw
        pdp = img->m_pBuffer + yDest * img->m_width + xDest;
        psp = imgSrc->m_pBuffer + ySrc * imgSrc->m_width + xSrc;
        ddx = img->m_width - widthSrc;
        dsx = imgSrc->m_width - widthSrc;

        for (y = 0; y < heightSrc; ++y) {
            for (x = 0; x < widthSrc; ++x, ++psp, ++pdp) {
                DWORD d = *pdp, s = *psp;
                *pdp = alphablend_inline(d, s, alpha);
            }
            pdp += ddx;
            psp += dsx;
        }
    }
    CONVERT_IMAGE_END;
    return grOk;
}

int IMAGE::putimage_alphatransparent(PIMAGE imgDest,       // handle to dest
    int                                     xDest,  // x-coord of destination upper-left corner
    int                                     yDest,  // y-coord of destination upper-left corner
    color_t                                 transparentColor, // color to make transparent
    unsigned char                           alpha,         // alpha
    int                                     xSrc,   // x-coord of source upper-left corner
    int                                     ySrc,   // y-coord of source upper-left corner
    int                                     widthSrc,     // width of source rectangle
    int                                     heightSrc     // height of source rectangle
) const
{
    inittest(L"IMAGE::putimage_alphatransparent");
    const PIMAGE img = CONVERT_IMAGE(imgDest);
    if (img) {
        PCIMAGE imgSrc = this;
        int     y, x;
        DWORD   ddx, dsx;
        DWORD * pdp, *psp, cr;
        // fix rect
        fix_rect_1size(img, imgSrc, &xDest, &yDest, &xSrc, &ySrc, &widthSrc, &heightSrc);
        // draw
        pdp = img->m_pBuffer + yDest * img->m_width + xDest;
        psp = imgSrc->m_pBuffer + ySrc * imgSrc->m_width + xSrc;
        ddx = img->m_width - widthSrc;
        dsx = imgSrc->m_width - widthSrc;
        cr  = transparentColor & 0x00FFFFFF;
        for (y = 0; y < heightSrc; ++y) {
            for (x = 0; x < widthSrc; ++x, ++psp, ++pdp) {
                if ((*psp & 0x00FFFFFF) != cr) {
                    DWORD d = *pdp, s = *psp;
                    *pdp = alphablend_inline(d, s, alpha);
                }
            }
            pdp += ddx;
            psp += dsx;
        }
    }
    CONVERT_IMAGE_END;
    return grOk;
}

int IMAGE::putimage_withalpha(PIMAGE imgDest,      // handle to dest
    int                              xDest, // x-coord of destination upper-left corner
    int                              yDest, // y-coord of destination upper-left corner
    int                              xSrc,  // x-coord of source upper-left corner
    int                              ySrc,  // y-coord of source upper-left corner
    int                              widthSrc,    // width of source rectangle
    int                              heightSrc    // height of source rectangle
) const
{
    inittest(L"IMAGE::putimage_withalpha");
    const PIMAGE img = CONVERT_IMAGE(imgDest);
    if (img) {
        PCIMAGE imgSrc = this;
        int     y, x;
        DWORD   ddx, dsx;
        DWORD * pdp, *psp;
        // fix rect
        fix_rect_1size(img, imgSrc, &xDest, &yDest, &xSrc, &ySrc, &widthSrc, &heightSrc);
        // draw
        pdp = img->m_pBuffer + yDest * img->m_width + xDest;
        psp = imgSrc->m_pBuffer + ySrc * imgSrc->m_width + xSrc;
        ddx = img->m_width - widthSrc;
        dsx = imgSrc->m_width - widthSrc;
        for (y = 0; y < heightSrc; ++y) {
            for (x = 0; x < widthSrc; ++x, ++psp, ++pdp) {
                DWORD d = *pdp, s = *psp;
                *pdp = alphablend_inline(d, s);
            }
            pdp += ddx;
            psp += dsx;
        }
    }
    CONVERT_IMAGE_END;
    return grOk;
}

int IMAGE::putimage_withalpha(PIMAGE imgDest,   // handle to dest
    int                              xDest,     // x-coord of destination upper-left corner
    int                              yDest,     // y-coord of destination upper-left corner
    int                              widthDest, // width of destination rectangle
    int                              heightDest,// height of destination rectangle
    int                              xSrc,      // x-coord of source upper-left corner
    int                              ySrc,      // y-coord of source upper-left corner
    int                              widthSrc,  // width of source rectangle
    int                              heightSrc,  // height of source rectangle
    bool                             smooth
) const
{
    inittest(L"IMAGE::putimage_withalpha");
    imgDest = CONVERT_IMAGE(imgDest);
    if (imgDest) {
        PCIMAGE imgSrc = this;
        #if 0
        int     x, y;
        DWORD   ddx, dsx;
        DWORD * pdp, *psp;
        PIMAGE  alphaSrc = newimage(widthSrc, heightSrc);

        BLENDFUNCTION bf;
        // fix rect
        fix_rect_1size(img, imgSrc, &xDest, &yDest, &xSrc, &ySrc, &widthSrc, &heightSrc);
        // premultiply alpha channel
        pdp = alphaSrc->m_pBuffer;
        psp = imgSrc->m_pBuffer + ySrc * imgSrc->m_width + xSrc;
        ddx = 0;
        dsx = imgSrc->m_width - widthSrc;
        for (y = 0; y < heightSrc; ++y) {
            for (x = 0; x < widthSrc; ++x, ++psp, ++pdp) {
                DWORD s     = *psp;
                DWORD alpha = EGEGET_A(s);
                DWORD r     = EGEGET_R(s);
                DWORD g     = EGEGET_G(s);
                DWORD b     = EGEGET_B(s);
                *pdp        = EGERGBA(r * alpha >> 8, g * alpha >> 8, b * alpha >> 8, alpha);
            }
            pdp += ddx;
            psp += dsx;
        }

        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = 0xff;
        bf.AlphaFormat         = AC_SRC_ALPHA;
        // draw
        dll::AlphaBlend(img->m_hDC, xDest, yDest, widthDest, heightDest, alphaSrc->m_hDC, 0, 0, widthSrc,
            heightSrc, bf);
        delimage(alphaSrc);
        #endif

        if (widthSrc   <= 0) widthSrc   = imgSrc->m_width;
        if (heightSrc  <= 0) heightSrc  = imgSrc->m_height;
        if (widthDest  <= 0) widthDest  = widthSrc;
        if (heightDest <= 0) heightDest = heightSrc;

        const viewporttype& vptDest = imgDest->m_vpt;
        const viewporttype& vptSrc  = imgSrc->m_vpt;
        Rect drawDest(xDest + vptDest.left, yDest + vptDest.top, widthDest, heightDest);
        Rect drawSrc(xSrc + vptSrc.left, ySrc + vptSrc.top, widthSrc, heightSrc);
        Rect clipDest(0, 0, imgDest->m_width, imgDest->m_height);

        if (vptDest.clipflag) {
            clipDest = Rect(vptDest.left, vptDest.top, vptDest.right - vptDest.left, vptDest.bottom - vptDest.top);
        }
        Gdiplus::GraphicsPath path;
        path.AddRectangle(Gdiplus::Rect(clipDest.x, clipDest.y, clipDest.width, clipDest.height));
        Gdiplus::Region region(&path);

        Gdiplus::Graphics* graphics = imgDest->getGraphics();
        graphics->SetClip(&region);

        if (smooth) {
            graphics->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        } else {
            graphics->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        }

        Gdiplus::RectF rectDest((float)drawDest.x, (float)drawDest.y, (float)drawDest.width, (float)drawDest.height);
        Gdiplus::RectF rectSrc((float)drawSrc.x, (float)drawSrc.y, (float)drawSrc.width, (float)drawSrc.height);

        Gdiplus::Bitmap bitmap(imgSrc->m_width, imgSrc->m_height, sizeof(color_t) * imgSrc->m_width,
        PixelFormat32bppARGB, (BYTE*)imgSrc->m_pBuffer);
        graphics->DrawImage(&bitmap, rectDest, rectSrc.X, rectSrc.Y, rectSrc.Width, rectSrc.Height, Gdiplus::UnitPixel, NULL);
    }
    CONVERT_IMAGE_END;
    return grOk;
}

int IMAGE::putimage_alphafilter(PIMAGE imgDest,     // handle to dest
    int                                xDest,       // x-coord of destination upper-left corner
    int                                yDest,       // y-coord of destination upper-left corner
    PCIMAGE                            imgAlpha,    // alpha
    int                                xSrc,        // x-coord of source upper-left corner
    int                                ySrc,        // y-coord of source upper-left corner
    int                                widthSrc,    // width of source rectangle
    int                                heightSrc    // height of source rectangle
) const
{
    inittest(L"IMAGE::putimage_alphafilter");
    const PIMAGE img = CONVERT_IMAGE(imgDest);
    if (img) {
        PCIMAGE imgSrc = this;
        int     y, x;
        DWORD   ddx, dsx;
        DWORD * pdp, *psp, *pap;
        // DWORD sa = alpha + 1, da = 0xFF - alpha;
        //  fix rect
        fix_rect_1size(img, imgSrc, &xDest, &yDest, &xSrc, &ySrc, &widthSrc, &heightSrc);
        // draw
        pdp = img->m_pBuffer + yDest * img->m_width + xDest;
        psp = imgSrc->m_pBuffer + ySrc * imgSrc->m_width + xSrc;
        pap = imgAlpha->m_pBuffer + ySrc * imgAlpha->m_width + xSrc;
        ddx = img->m_width - widthSrc;
        dsx = imgSrc->m_width - widthSrc;
        for (y = 0; y < heightSrc; ++y) {
            for (x = 0; x < widthSrc; ++x, ++psp, ++pdp, ++pap) {
                DWORD d = *pdp, s = *psp;
                unsigned char alpha = *pap & 0xFF;
                if (*pap) {
                    *pdp = alphablend_inline(d, s, alpha);
                }
            }
            pdp += ddx;
            psp += dsx;
            pap += dsx;
        }
    }
    CONVERT_IMAGE_END;
    return grOk;
}

/* private function */
static void fix_rect_0size(PIMAGE pdest,
    int*                          xDest, // x-coord of destination upper-left corner
    int*                          yDest, // y-coord of destination upper-left corner
    int*                          widthDest,   // width of destination rectangle
    int*                          heightDest   // height of destination rectangle
)
{
    struct viewporttype _vpt = {0, 0, pdest->m_width, pdest->m_height};
    if (*widthDest == 0) {
        *widthDest = pdest->m_width;
    }
    if (*heightDest == 0) {
        *heightDest = pdest->m_height;
    }
    if (*xDest < _vpt.left) {
        int dx         = _vpt.left - *xDest;
        *xDest += dx;
    }
    if (*yDest < _vpt.top) {
        int dy         = _vpt.top - *yDest;
        *yDest += dy;
    }
    if (*xDest + *widthDest > _vpt.right) {
        int dx       = *xDest + *widthDest - _vpt.right;
        *widthDest -= dx;
    }
    if (*yDest + *heightDest > _vpt.bottom) {
        int dy        = *yDest + *heightDest - _vpt.bottom;
        *heightDest -= dy;
    }
}

int IMAGE::imagefilter_blurring_4(
    int intensity, int alpha, int xDest, int yDest, int widthDest, int heightDest)
{
    inittest(L"IMAGE::imagefilter_blurring_4");
    struct _graph_setting* pg   = &graph_setting;
    DWORD*                 buff = pg->g_t_buff;
    int                    x2, y2, ix, iy;
    DWORD *                pdp, lsum, sumRB, sumG;
    int                    ddx, dldx;
    int                    centerintensity;
    int                    intensity2, intensity3, intensity4;
    int                    intensity2f, intensity3f, intensity4f;
    PIMAGE                 imgDest = this;

    x2              = xDest + widthDest - 1;
    y2              = yDest + heightDest - 1;
    pdp             = imgDest->m_pBuffer + yDest * imgDest->m_width + xDest;
    ddx             = imgDest->m_width - widthDest;
    dldx            = imgDest->m_width;
    centerintensity = (0xFF - intensity) * alpha >> 8;
    intensity2      = intensity * alpha / 2 >> 8;
    intensity3      = intensity * alpha / 3 >> 8;
    intensity4      = intensity * alpha / 4 >> 8;
    intensity2f     = (intensity * alpha % (2 * alpha)) / 2;
    intensity3f     = (intensity * alpha % (3 * alpha)) / 3;
    intensity4f     = (intensity * alpha % (4 * alpha)) / 4;
    {
        ix = xDest;
        {
            sumRB    = (pdp[dldx] & 0xFF00FF) + (pdp[1] & 0xFF00FF);
            sumG     = (pdp[dldx] & 0xFF00) + (pdp[1] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        for (ix = xDest + 1; ix < x2; ++ix) {
            sumRB    = (lsum & 0xFF00FF) + (pdp[dldx] & 0xFF00FF) + (pdp[1] & 0xFF00FF);
            sumG     = (lsum & 0xFF00) + (pdp[dldx] & 0xFF00) + (pdp[1] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        {
            sumRB    = (lsum & 0xFF00FF) + (pdp[dldx] & 0xFF00FF);
            sumG     = (lsum & 0xFF00) + (pdp[dldx] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        pdp += ddx;
    }
    for (iy = yDest + 1; iy < y2; ++iy) {
        ix = xDest;
        {
            sumRB    = (buff[ix] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF) + (pdp[1] & 0xFF00FF);
            sumG     = (buff[ix] & 0xFF00) + (pdp[dldx] & 0xFF00) + (pdp[1] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        for (ix = xDest + 1; ix < x2; ++ix) {
            sumRB    = (lsum & 0xFF00FF) + (buff[ix] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF) + (pdp[1] & 0xFF00FF);
            sumG     = (lsum & 0xFF00) + (buff[ix] & 0xFF00) + (pdp[dldx] & 0xFF00) + (pdp[1] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity4 + ((sumRB * intensity4f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity4 + ((sumG * intensity4f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        {
            sumRB    = (lsum & 0xFF00FF) + (buff[ix] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF);
            sumG     = (lsum & 0xFF00) + (buff[ix] & 0xFF00) + (pdp[dldx] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        pdp += ddx;
    }
    {
        ix = xDest;
        {
            sumRB    = (buff[ix] & 0xFF00FF) + (pdp[1] & 0xFF00FF);
            sumG     = (buff[ix] & 0xFF00) + (pdp[1] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        for (ix = xDest + 1; ix < x2; ++ix) {
            sumRB    = (lsum & 0xFF00FF) + (buff[ix] & 0xFF00FF) + (pdp[1] & 0xFF00FF);
            sumG     = (lsum & 0xFF00) + (buff[ix] & 0xFF00) + (pdp[1] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        {
            sumRB    = (lsum & 0xFF00FF) + (buff[ix] & 0xFF00FF);
            sumG     = (lsum & 0xFF00) + (buff[ix] & 0xFF00);
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        // pdp += ddx;
    }
    return grOk;
}

int IMAGE::imagefilter_blurring_8(
    int intensity, int alpha, int xDest, int yDest, int widthDest, int heightDest)
{
    inittest(L"IMAGE::imagefilter_blurring_4");
    struct _graph_setting* pg   = &graph_setting;
    DWORD *                buff = pg->g_t_buff, lbuf;
    int                    x2, y2, ix, iy;
    DWORD *                pdp, lsum, sumRB, sumG;
    int                    ddx, dldx;
    int                    centerintensity;
    int                    intensity2, intensity3, intensity4;
    int                    intensity2f, intensity3f, intensity4f;

    PIMAGE imgDest = this;
    x2             = xDest + widthDest - 1;
    y2             = yDest + heightDest - 1;
    pdp            = imgDest->m_pBuffer + yDest * imgDest->m_width + xDest;
    ddx            = imgDest->m_width - widthDest;
    dldx           = imgDest->m_width;

    centerintensity = (0xFF - intensity) * alpha >> 8;
    intensity2      = intensity * alpha / 3 >> 8;
    intensity3      = intensity * alpha / 5 >> 8;
    intensity4      = intensity * alpha / 8 >> 8;
    intensity2f     = (intensity * alpha % (3 * alpha)) / 3;
    intensity3f     = (intensity * alpha % (5 * alpha)) / 5;
    intensity4f     = (intensity * alpha % (8 * alpha)) / 8;
    {
        ix = xDest;
        {
            sumRB    = (pdp[1] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF) + (pdp[dldx + 1] & 0xFF00FF);
            sumG     = +(pdp[1] & 0xFF00) + (pdp[dldx] & 0xFF00) + (pdp[dldx + 1] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        for (ix = xDest + 1; ix < x2; ++ix) {
            sumRB = (lsum & 0xFF00FF) + (pdp[1] & 0xFF00FF) + (pdp[dldx - 1] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF) +
                (pdp[dldx + 1] & 0xFF00FF);
            sumG = (lsum & 0xFF00) + (pdp[1] & 0xFF00) + (pdp[dldx - 1] & 0xFF00) + (pdp[dldx] & 0xFF00) +
                (pdp[dldx + 1] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        {
            sumRB    = (lsum & 0xFF00FF) + (pdp[dldx - 1] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF);
            sumG     = (lsum & 0xFF00) + (pdp[dldx - 1] & 0xFF00) + (pdp[dldx] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        pdp += ddx;
    }
    for (iy = yDest + 1; iy < y2; ++iy) {
        ix = xDest;
        {
            sumRB = (buff[ix] & 0xFF00FF) + (buff[ix + 1] & 0xFF00FF) + (pdp[1] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF) +
                (pdp[dldx + 1] & 0xFF00FF);
            sumG = (buff[ix] & 0xFF00) + (buff[ix + 1] & 0xFF00) + (pdp[1] & 0xFF00) + (pdp[dldx] & 0xFF00) +
                (pdp[dldx + 1] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        for (ix = xDest + 1; ix < x2; ++ix) {
            sumRB = (lbuf & 0xFF00FF) + (buff[ix] & 0xFF00FF) + (buff[ix + 1] & 0xFF00FF) + (lsum & 0xFF00FF) +
                (pdp[1] & 0xFF00FF) + (pdp[dldx - 1] & 0xFF00FF) + (pdp[dldx] & 0xFF00FF) + (pdp[dldx + 1] & 0xFF00FF);
            sumG = (lbuf & 0xFF00) + (buff[ix] & 0xFF00) + (buff[ix + 1] & 0xFF00) + (lsum & 0xFF00) +
                (pdp[1] & 0xFF00) + (pdp[dldx - 1] & 0xFF00) + (pdp[dldx] & 0xFF00) + (pdp[dldx + 1] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity4 + ((sumRB * intensity4f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity4 + ((sumG * intensity4f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        {
            sumRB = (lbuf & 0xFF00FF) + (buff[ix] & 0xFF00FF) + (lsum & 0xFF00FF) + (lbuf & 0xFF00FF) +
                (pdp[dldx] & 0xFF00FF);
            sumG = (buff[ix - 1] & 0xFF00) + (buff[ix] & 0xFF00) + (lsum & 0xFF00) + (pdp[dldx - 1] & 0xFF00) +
                (pdp[dldx] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        pdp += ddx;
    }
    {
        ix = xDest;
        {
            sumRB    = (buff[ix] & 0xFF00FF) + (buff[ix + 1] & 0xFF00FF) + (pdp[1] & 0xFF00FF);
            sumG     = (buff[ix] & 0xFF00) + (buff[ix + 1] & 0xFF00) + (pdp[1] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        for (ix = xDest + 1; ix < x2; ++ix) {
            sumRB = (lbuf & 0xFF00FF) + (buff[ix] & 0xFF00FF) + (buff[ix + 1] & 0xFF00FF) + (lsum & 0xFF00FF) +
                (pdp[1] & 0xFF00FF);
            sumG =
                (lbuf & 0xFF00) + (buff[ix] & 0xFF00) + (buff[ix + 1] & 0xFF00) + (lsum & 0xFF00) + (pdp[1] & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity3 + ((sumRB * intensity3f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity3 + ((sumG * intensity3f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        {
            sumRB    = (lbuf & 0xFF00FF) + (buff[ix] & 0xFF00FF) + (lsum & 0xFF00FF);
            sumG     = (lbuf & 0xFF00) + (buff[ix] & 0xFF00) + (lsum & 0xFF00);
            lbuf     = buff[ix];
            buff[ix] = lsum = *pdp;
            sumRB = sumRB * intensity2 + ((sumRB * intensity2f >> 8) & 0xFF00FF) + (lsum & 0xFF00FF) * centerintensity;
            sumG  = sumG * intensity2 + ((sumG * intensity2f >> 8)) + (lsum & 0xFF00) * centerintensity;
            *pdp  = ((sumRB & 0xFF00FF00) | (sumG & 0xFF0000)) >> 8;
            ++pdp;
        }
        // pdp += ddx;
    }
    return grOk;
}

int IMAGE::imagefilter_blurring(
    int intensity, int alpha, int xDest, int yDest, int widthDest, int heightDest)
{
    inittest(L"IMAGE::imagefilter_blurring");
    PIMAGE imgDest = this;

    fix_rect_0size(imgDest, &xDest, &yDest, &widthDest, &heightDest);
    if (widthDest <= 0 || heightDest <= 0) {
        return grInvalidRegion;
    }

    if (alpha < 0 || alpha > 0x100) {
        alpha = 0x100;
    }

    if (intensity <= 0x80) {
        imagefilter_blurring_4(intensity * 2, alpha, xDest, yDest, widthDest, heightDest);
    } else {
        imagefilter_blurring_8((intensity - 0x80) * 2, alpha, xDest, yDest, widthDest, heightDest);
    }
    return grOk;
}

int IMAGE::putimage_rotate(PIMAGE imgtexture, int xDest, int yDest, float centerx, float centery,
    float radian,
    int   btransparent, // transparent (1) or not (0)
    int   alpha,        // in range[0, 256], alpha== -1 means no alpha
    int   smooth)
{
    return ege::putimage_rotate(
        this, imgtexture, xDest, yDest, centerx, centery, radian, btransparent, alpha, smooth);
}

int IMAGE::putimage_rotatezoom(PIMAGE imgtexture, int xDest, int yDest, float centerx, float centery,
    float radian, float zoom,
    int btransparent, // transparent (1) or not (0)
    int alpha,        // in range[0, 256], alpha== -1 means no alpha
    int smooth)
{
    return ege::putimage_rotatezoom(
        this, imgtexture, xDest, yDest, centerx, centery, radian, zoom, btransparent, alpha, smooth);
}

#define BILINEAR_INTERPOLATION(s, LT, RT, LB, RB, x, y)                                     \
    {                                                                                       \
        alphaA = (int)(x * 0x100);                                                          \
        alphaB = 0xFF - alphaA;                                                             \
        Trb    = (((LT & 0xFF00FF) * alphaB + (RT & 0xFF00FF) * alphaA) & 0xFF00FF00) >> 8; \
        Tg     = (((LT & 0xFF00) * alphaB + (RT & 0xFF00) * alphaA) & 0xFF0000) >> 8;       \
        Brb    = (((LB & 0xFF00FF) * alphaB + (RB & 0xFF00FF) * alphaA) & 0xFF00FF00) >> 8; \
        Bg     = (((LB & 0xFF00) * alphaB + (RB & 0xFF00) * alphaA) & 0xFF0000) >> 8;       \
        alphaA = (int)(y * 0x100);                                                          \
        alphaB = 0xFF - alphaA;                                                             \
        crb    = ((Trb * alphaB + Brb * alphaA) & 0xFF00FF00);                              \
        cg     = ((Tg * alphaB + Bg * alphaA) & 0xFF0000);                                  \
        s      = (crb | cg) >> 8;                                                           \
    }

/* ege 3d 结构 */
struct point2d
{
    float x;
    float y;
};

struct point3d
{
    float x;
    float y;
    float z;
};

struct vector2d
{
    struct point2d p[2];
};

struct vector3d
{
    struct point3d p[2];
};

typedef struct trangle2d
{
    struct point2d p[3];
    int            color;
} trangle2d;

struct trangle3d
{
    struct point3d p[3];
    int            color;
};

/* private funcion */
static int float2int(float f)
{
    if (f >= 0) {
        return (int)(f + .5);
    } else {
        return (int)(f - .5);
    }
}

/* private funcion */
static void draw_flat_scanline(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2)
{
    float dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int s = float2int((float)vt->p[0].x), e = float2int((float)vt->p[1].x), y = float2int((float)vt->p[0].y), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    int    dest_w           = dc_dest->getwidth();
    int    src_w            = dc_src->getwidth();
    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x + .5f, cury = _svt.p[0].y + .5f;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                lp_dest_bmp_byte[dest_w * y + i] = lp_src_bmp_byte[src_w * (int)(cury) + (int)(curx)];
            }
        }
    }
}

/* private funcion */
static void draw_flat_scanline_transparent(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2)
{
    float  dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int    s = (int)(vt->p[0].x + .5), e = (int)(vt->p[1].x + .5), y = (int)(vt->p[0].y + .5), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    DWORD  col;
    int    dest_w = dc_dest->getwidth();
    int    src_w  = dc_src->getwidth();
    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x + .5f, cury = _svt.p[0].y + .5f;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                col = lp_src_bmp_byte[src_w * (int)(cury) + (int)(curx)];
                if (col) {
                    lp_dest_bmp_byte[dest_w * y + i] = col;
                }
            }
        }
    }
}

/* private funcion */
static void draw_flat_scanline_alpha(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2, int alpha)
{
    float dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int s = float2int((float)vt->p[0].x), e = float2int((float)vt->p[1].x), y = float2int((float)vt->p[0].y), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    DWORD  sa = alpha, da = 0xFF - sa;

    int dest_w = dc_dest->getwidth();
    int src_w  = dc_src->getwidth();

    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x + .5f, cury = _svt.p[0].y + .5f;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                DWORD d = lp_dest_bmp_byte[dest_w * y + i], s = lp_src_bmp_byte[src_w * (int)(cury) + (int)(curx)];
                d                                = ((d & 0xFF00FF) * da & 0xFF00FF00) | ((d & 0xFF00) * da >> 16 << 16);
                s                                = ((s & 0xFF00FF) * sa & 0xFF00FF00) | ((s & 0xFF00) * sa >> 16 << 16);
                lp_dest_bmp_byte[dest_w * y + i] = (d + s) >> 8;
            }
        }
    }
}

/* private funcion */
static void draw_flat_scanline_alphatrans(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2, int alpha)
{
    float  dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int    s = (int)(vt->p[0].x + .5), e = (int)(vt->p[1].x + .5), y = (int)(vt->p[0].y + .5), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    DWORD  sa = alpha, da = 0xFF - sa;

    int dest_w = dc_dest->getwidth();
    int src_w  = dc_src->getwidth();
    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x + .5f, cury = _svt.p[0].y + .5f;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                DWORD s = lp_src_bmp_byte[src_w * (int)(cury) + (int)(curx)];
                if (s) {
                    DWORD d = lp_dest_bmp_byte[dest_w * y + i];
                    d       = ((d & 0xFF00FF) * da & 0xFF00FF00) | ((d & 0xFF00) * da >> 16 << 16);
                    s       = ((s & 0xFF00FF) * sa & 0xFF00FF00) | ((s & 0xFF00) * sa >> 16 << 16);
                    lp_dest_bmp_byte[dest_w * y + i] = (d + s) >> 8;
                }
            }
        }
    }
}

/* private funcion */
/* static
color_t bilinear_interpolation(color_t LT, color_t RT, color_t LB, color_t RB, double x, double y) {
    color_t Trb, Tg, Brb, Bg, crb, cg;
    color_t alphaA, alphaB;
    alphaA = (color_t)(x * 0x100);
    alphaB = 0xFF - alphaA;
    Trb = (((LT & 0xFF00FF) * alphaB + (RT & 0xFF00FF) * alphaA) & 0xFF00FF00) >> 8;
    Tg =  (((LT & 0xFF00) * alphaB + (RT & 0xFF00) * alphaA) & 0xFF0000) >> 8;
    Brb = (((LB & 0xFF00FF) * alphaB + (RB & 0xFF00FF) * alphaA) & 0xFF00FF00) >> 8;
    Bg =  (((LB & 0xFF00) * alphaB + (RB & 0xFF00) * alphaA) & 0xFF0000) >> 8;
    alphaA = (color_t)(y * 0x100);
    alphaB = 0xFF - alphaA;
    crb = ((Trb * alphaB + Brb * alphaA) & 0xFF00FF00);
    cg =  ((Tg * alphaB + Bg * alphaA) & 0xFF0000);
    return (crb | cg) >> 8;
}// */

/* private funcion */
static void draw_flat_scanline_s(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2)
{
    float dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int s = float2int((float)vt->p[0].x), e = float2int((float)vt->p[1].x), y = float2int((float)vt->p[0].y), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    DWORD  Trb, Tg, Brb, Bg, crb, cg;
    DWORD  alphaA, alphaB;
    int    dest_w = dc_dest->getwidth();
    int    src_w  = dc_src->getwidth();
    // int src_h = dc_src->h;

    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x, cury = _svt.p[0].y;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                int    ix = (int)curx, iy = (int)cury;
                double fx = curx - ix, fy = cury - iy;
                DWORD* lp_src_byte = lp_src_bmp_byte + src_w * iy + ix;
                DWORD  col;
                BILINEAR_INTERPOLATION(
                    col, lp_src_byte[0], lp_src_byte[1], lp_src_byte[src_w], lp_src_byte[src_w + 1], fx, fy);
                lp_dest_bmp_byte[dest_w * y + i] = col;
            }
        }
    }
}

/* private funcion */
static void draw_flat_scanline_transparent_s(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2)
{
    float dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int s = float2int((float)vt->p[0].x), e = float2int((float)vt->p[1].x), y = float2int((float)vt->p[0].y), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    DWORD  Trb, Tg, Brb, Bg, crb, cg;
    DWORD  alphaA, alphaB;
    int    dest_w = dc_dest->getwidth();
    int    src_w  = dc_src->getwidth();
    // int src_h = dc_src->h;

    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x, cury = _svt.p[0].y;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                int    ix = (int)curx, iy = (int)cury;
                float  fx = curx - ix, fy = cury - iy;
                DWORD* lp_src_byte = lp_src_bmp_byte + src_w * iy + ix;
                DWORD  col;
                BILINEAR_INTERPOLATION(
                    col, lp_src_byte[0], lp_src_byte[1], lp_src_byte[src_w], lp_src_byte[src_w + 1], fx, fy);
                if (col) {
                    lp_dest_bmp_byte[dest_w * y + i] = col;
                }
            }
        }
    }
}

/* private funcion */
static void draw_flat_scanline_alpha_s(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2, int alpha)
{
    float dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int s = float2int((float)vt->p[0].x), e = float2int((float)vt->p[1].x), y = float2int((float)vt->p[0].y), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    DWORD  Trb, Tg, Brb, Bg, crb, cg;
    DWORD  alphaA, alphaB;
    DWORD  sa = alpha, da = 0xFF - sa;

    int dest_w = dc_dest->getwidth();
    int src_w  = dc_src->getwidth();
    // int src_h = dc_src->h;

    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x, cury = _svt.p[0].y;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                int    ix = (int)curx, iy = (int)cury;
                float  fx = curx - ix, fy = cury - iy;
                DWORD* lp_src_byte = lp_src_bmp_byte + src_w * iy + ix;
                DWORD  col;
                BILINEAR_INTERPOLATION(
                    col, lp_src_byte[0], lp_src_byte[1], lp_src_byte[src_w], lp_src_byte[src_w + 1], fx, fy);
                {
                    DWORD d = lp_dest_bmp_byte[dest_w * y + i];
                    d       = (((d & 0xFF00FF) * da & 0xFF00FF00) | ((d & 0xFF00) * da & 0xFF0000)) >> 8;
                    col     = (((col & 0xFF00FF) * sa & 0xFF00FF00) | ((col & 0xFF00) * sa & 0xFF0000)) >> 8;
                    lp_dest_bmp_byte[dest_w * y + i] = d + col;
                }
            }
        }
    }
}

/* private funcion */
static void draw_flat_scanline_alphatrans_s(
    PIMAGE dc_dest, const struct vector2d* vt, PCIMAGE dc_src, const struct vector2d* svt, int x1, int x2, int alpha)
{
    float dw = vt->p[1].x - vt->p[0].x, rw = svt->p[1].x - svt->p[0].x;
    int s = float2int((float)vt->p[0].x), e = float2int((float)vt->p[1].x), y = float2int((float)vt->p[0].y), w = e - s;
    DWORD* lp_dest_bmp_byte = (DWORD*)dc_dest->getbuffer();
    DWORD* lp_src_bmp_byte  = (DWORD*)dc_src->getbuffer();
    DWORD  Trb, Tg, Brb, Bg, crb, cg;
    DWORD  alphaA, alphaB;
    DWORD  sa = alpha, da = 0xFF - sa;

    int dest_w = dc_dest->getwidth();
    int src_w  = dc_src->getwidth();
    // int src_h = dc_src->h;

    if (w > 0) {
        int             i, bx = s;
        struct vector2d _svt  = *svt;
        _svt.p[0].x          += (float)((s - vt->p[0].x) * rw / dw);
        _svt.p[1].x          += (float)((e - vt->p[1].x) * rw / dw);
        {
            float curx = _svt.p[0].x, cury = _svt.p[0].y;
            float dx = (_svt.p[1].x - _svt.p[0].x) / w;
            float dy = (_svt.p[1].y - _svt.p[0].y) / w;

            if (s < x1) {
                s = x1;
            }
            if (e > x2) {
                e = x2;
            }
            curx += (s - bx) * dx;
            cury += (s - bx) * dy;

            for (i = s; i < e; ++i, curx += dx, cury += dy) {
                int    ix = (int)curx, iy = (int)cury;
                float  fx = curx - ix, fy = cury - iy;
                DWORD* lp_src_byte = lp_src_bmp_byte + src_w * iy + ix;
                DWORD  col;
                BILINEAR_INTERPOLATION(
                    col, lp_src_byte[0], lp_src_byte[1], lp_src_byte[src_w], lp_src_byte[src_w + 1], fx, fy);
                if (col) {
                    DWORD d = lp_dest_bmp_byte[dest_w * y + i];
                    d       = (((d & 0xFF00FF) * da & 0xFF00FF00) | ((d & 0xFF00) * da & 0xFF0000)) >> 8;
                    col     = (((col & 0xFF00FF) * sa & 0xFF00FF00) | ((col & 0xFF00) * sa & 0xFF0000)) >> 8;
                    lp_dest_bmp_byte[dest_w * y + i] = d + col;
                }
            }
        }
    }
}

/* private funcion */
static void draw_flat_trangle_alpha(PIMAGE dc_dest, const struct trangle2d* dt, PCIMAGE dc_src,
    const struct trangle2d* tt, int x1, int y1, int x2, int y2, int transparent, int alpha)
{
    struct trangle2d t2d = *dt;
    struct trangle2d t3d = *tt;
    int              b_alpha;
    // int width = x2 - x1, height = y2 - y1;

    if (alpha >= 0 && alpha < 0x100) {
        b_alpha = 1;
    } else {
        b_alpha = 0;
    }
    {
        struct point2d _t;
        struct point2d _t3;

        if (t2d.p[1].y > t2d.p[2].y) {
            SWAP(t2d.p[1], t2d.p[2], _t);
            SWAP(t3d.p[1], t3d.p[2], _t3);
        }
        if (t2d.p[0].y > t2d.p[1].y) {
            SWAP(t2d.p[0], t2d.p[1], _t);
            SWAP(t3d.p[0], t3d.p[1], _t3);
        }
        if (t2d.p[1].y > t2d.p[2].y) {
            SWAP(t2d.p[1], t2d.p[2], _t);
            SWAP(t3d.p[1], t3d.p[2], _t3);
        }
    }
    {
        float dd;
        int   s = float2int((float)t2d.p[0].y), e = float2int((float)t2d.p[2].y), h, m = float2int((float)t2d.p[1].y);
        int   rs, re;
        int   i, lh, rh;
        float dm = t2d.p[1].y, dh;
        struct point2d pl, pr, pt;
        struct point2d spl, spr;

        pl.x  = t2d.p[1].x - t2d.p[0].x;
        pr.x  = t2d.p[2].x - t2d.p[0].x;
        pl.y  = t2d.p[1].y - t2d.p[0].y;
        pr.y  = t2d.p[2].y - t2d.p[0].y;
        spl.x = t3d.p[1].x - t3d.p[0].x;
        spr.x = t3d.p[2].x - t3d.p[0].x;
        spl.y = t3d.p[1].y - t3d.p[0].y;
        spr.y = t3d.p[2].y - t3d.p[0].y;
        dh    = dm - s;
        h     = m - s;
        rs    = s;
        if (s < y1) {
            s = y1;
        }
        if (m >= y2) {
            m = y2;
        }
        /*if (pl.y > pr.y) {
        dd = pr.y / pl.y;
        pl.x *= dd;
        spl.x *= dd;
        spl.y *= dd;
        } else {
        dd = pl.y / pr.y;
        pr.x *= dd;
        spr.x *= dd;
        spr.y *= dd;
        }// */
        if (pl.x > pr.x) {
            SWAP(pl, pr, pt);
            SWAP(spl, spr, pt);
        } else {
            ;
        }
        lh = float2int((float)(pl.y + t2d.p[0].y)) - float2int((float)(t2d.p[0].y));
        rh = float2int((float)(pr.y + t2d.p[0].y)) - float2int((float)(t2d.p[0].y));
        if (h > 0) {
            for (i = s; i < m; ++i) {
                struct vector2d vt;
                struct vector2d svt;
                // float dt = (float)(i - rs) / h;
                float dlt = (float)(i - rs) / lh;
                float drt = (float)(i - rs) / rh;

                /*
                vt.p[0].x = t2d.p[0].x + pl.x * dt;
                vt.p[1].x = t2d.p[0].x + pr.x * dt;
                vt.p[0].y = vt.p[1].y = i;

                svt.p[0].x = t3d.p[0].x + spl.x * dt;
                svt.p[1].x = t3d.p[0].x + spr.x * dt;
                svt.p[0].y = t3d.p[0].y + spl.y * dt;
                svt.p[1].y = t3d.p[0].y + spr.y * dt;
                // */
                vt.p[0].x = t2d.p[0].x + pl.x * dlt;
                vt.p[1].x = t2d.p[0].x + pr.x * drt;
                vt.p[0].y = vt.p[1].y = (float)(i);

                svt.p[0].x = t3d.p[0].x + spl.x * dlt;
                svt.p[1].x = t3d.p[0].x + spr.x * drt;
                svt.p[0].y = t3d.p[0].y + spl.y * dlt;
                svt.p[1].y = t3d.p[0].y + spr.y * drt;

                if (b_alpha) {
                    if (transparent) {
                        draw_flat_scanline_alphatrans(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    } else {
                        draw_flat_scanline_alpha(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    }
                } else {
                    if (transparent) {
                        draw_flat_scanline_transparent(dc_dest, &vt, dc_src, &svt, x1, x2);
                    } else {
                        draw_flat_scanline(dc_dest, &vt, dc_src, &svt, x1, x2);
                    }
                }
            }
        }
        if (pl.y > pr.y) {
            dd     = pr.y / pl.y;
            pl.x  *= dd;
            spl.x *= dd;
            spl.y *= dd;
        } else {
            dd     = pl.y / pr.y;
            pr.x  *= dd;
            spr.x *= dd;
            spr.y *= dd;
        }
        if (m >= y1 && m < y2 && m < e) {
            struct vector2d vt;
            struct vector2d svt;

            vt.p[0].x = t2d.p[0].x + pl.x;
            vt.p[1].x = t2d.p[0].x + pr.x;
            vt.p[0].y = vt.p[1].y = (float)(m);

            svt.p[0].x = t3d.p[0].x + spl.x;
            svt.p[1].x = t3d.p[0].x + spr.x;
            svt.p[0].y = t3d.p[0].y + spl.y;
            svt.p[1].y = t3d.p[0].y + spr.y;

            if (b_alpha) {
                if (transparent) {
                    draw_flat_scanline_alphatrans(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                } else {
                    draw_flat_scanline_alpha(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                }
            } else {
                if (transparent) {
                    draw_flat_scanline_transparent(dc_dest, &vt, dc_src, &svt, x1, x2);
                } else {
                    draw_flat_scanline(dc_dest, &vt, dc_src, &svt, x1, x2);
                }
            }
        } // */
        pl.x  = t2d.p[0].x - t2d.p[2].x;
        pr.x  = t2d.p[1].x - t2d.p[2].x;
        pl.y  = t2d.p[0].y - t2d.p[2].y;
        pr.y  = t2d.p[1].y - t2d.p[2].y;
        spl.x = t3d.p[0].x - t3d.p[2].x;
        spr.x = t3d.p[1].x - t3d.p[2].x;
        spl.y = t3d.p[0].y - t3d.p[2].y;
        spr.y = t3d.p[1].y - t3d.p[2].y;

        dh = e - dm;
        h  = e - m;
        re = e;
        if (m < y1) {
            m = y1 - 1;
        }
        if (e >= y2) {
            e = y2 - 1;
        }
        /*if (pl.y < pr.y) {
        dd = pr.y / pl.y;
        pl.x *= dd;
        spl.x *= dd;
        spl.y *= dd;
        } else {
        dd = pl.y / pr.y;
        pr.x *= dd;
        spr.x *= dd;
        spr.y *= dd;
        }// */
        if (pl.x > pr.x) {
            SWAP(pl, pr, pt);
            SWAP(spl, spr, pt);
        } else {
            ;
        }
        lh = float2int((float)(t2d.p[2].y)) - float2int((float)(pl.y + t2d.p[2].y));
        rh = float2int((float)(t2d.p[2].y)) - float2int((float)(pr.y + t2d.p[2].y));
        if (h > 0) {
            for (i = e; i > m; --i) {
                struct vector2d vt;
                struct vector2d svt;
                // float dt = (float)(re - i) / h;
                float dlt = (float)(re - i) / lh;
                float drt = (float)(re - i) / rh;

                /*
                vt.p[0].x = t2d.p[2].x + pl.x * dt;
                vt.p[1].x = t2d.p[2].x + pr.x * dt;
                vt.p[0].y = vt.p[1].y = i;

                svt.p[0].x = t3d.p[2].x + spl.x * dt;
                svt.p[1].x = t3d.p[2].x + spr.x * dt;
                svt.p[0].y = t3d.p[2].y + spl.y * dt;
                svt.p[1].y = t3d.p[2].y + spr.y * dt;
                // */
                vt.p[0].x = t2d.p[2].x + pl.x * dlt;
                vt.p[1].x = t2d.p[2].x + pr.x * drt;
                vt.p[0].y = vt.p[1].y = (float)(i);

                svt.p[0].x = t3d.p[2].x + spl.x * dlt;
                svt.p[1].x = t3d.p[2].x + spr.x * drt;
                svt.p[0].y = t3d.p[2].y + spl.y * dlt;
                svt.p[1].y = t3d.p[2].y + spr.y * drt;

                if (b_alpha) {
                    if (transparent) {
                        draw_flat_scanline_alphatrans(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    } else {
                        draw_flat_scanline_alpha(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    }
                } else {
                    if (transparent) {
                        draw_flat_scanline_transparent(dc_dest, &vt, dc_src, &svt, x1, x2);
                    } else {
                        draw_flat_scanline(dc_dest, &vt, dc_src, &svt, x1, x2);
                    }
                }
            }
        }
    }
}

/* private funcion */
static void draw_flat_trangle_alpha_s(PIMAGE dc_dest, const struct trangle2d* dt, PCIMAGE dc_src,
    const struct trangle2d* tt, int x1, int y1, int x2, int y2, bool transparent, int alpha)
{
    struct trangle2d t2d = *dt;
    struct trangle2d t3d = *tt;
    int              b_alpha;
    // int width = x2 - x1, height = y2 - y1;

    if (alpha >= 0 && alpha < 0x100) {
        b_alpha = 1;
    } else {
        b_alpha = 0;
    }
    {
        struct point2d _t;
        struct point2d _t3;

        if (t2d.p[1].y > t2d.p[2].y) {
            SWAP(t2d.p[1], t2d.p[2], _t);
            SWAP(t3d.p[1], t3d.p[2], _t3);
        }
        if (t2d.p[0].y > t2d.p[1].y) {
            SWAP(t2d.p[0], t2d.p[1], _t);
            SWAP(t3d.p[0], t3d.p[1], _t3);
        }
        if (t2d.p[1].y > t2d.p[2].y) {
            SWAP(t2d.p[1], t2d.p[2], _t);
            SWAP(t3d.p[1], t3d.p[2], _t3);
        }
    }
    {
        float dd;
        int   s = float2int((float)t2d.p[0].y), e = float2int((float)t2d.p[2].y), h, m = float2int((float)t2d.p[1].y);
        int   rs, re;
        int   i, lh, rh;
        float dm = t2d.p[1].y, dh;
        struct point2d pl, pr, pt;
        struct point2d spl, spr;

        pl.x  = t2d.p[1].x - t2d.p[0].x;
        pr.x  = t2d.p[2].x - t2d.p[0].x;
        pl.y  = t2d.p[1].y - t2d.p[0].y;
        pr.y  = t2d.p[2].y - t2d.p[0].y;
        spl.x = t3d.p[1].x - t3d.p[0].x;
        spr.x = t3d.p[2].x - t3d.p[0].x;
        spl.y = t3d.p[1].y - t3d.p[0].y;
        spr.y = t3d.p[2].y - t3d.p[0].y;
        dh    = dm - s;
        h     = m - s;
        rs    = s;
        if (s < y1) {
            s = y1;
        }
        if (m >= y2) {
            m = y2;
        }
        /*if (pl.y > pr.y) {
        dd = pr.y / pl.y;
        pl.x *= dd;
        spl.x *= dd;
        spl.y *= dd;
        } else {
        dd = pl.y / pr.y;
        pr.x *= dd;
        spr.x *= dd;
        spr.y *= dd;
        }// */
        if (pl.x > pr.x) {
            SWAP(pl, pr, pt);
            SWAP(spl, spr, pt);
        } else {
            ;
        }
        lh = float2int((float)(pl.y + t2d.p[0].y)) - float2int((float)(t2d.p[0].y));
        rh = float2int((float)(pr.y + t2d.p[0].y)) - float2int((float)(t2d.p[0].y));
        if (h > 0) {
            for (i = s; i < m; ++i) {
                struct vector2d vt;
                struct vector2d svt;
                // float dt = (float)(i - rs) / h;
                float dlt = (float)(i - rs) / lh;
                float drt = (float)(i - rs) / rh;

                /*
                vt.p[0].x = t2d.p[0].x + pl.x * dt;
                vt.p[1].x = t2d.p[0].x + pr.x * dt;
                vt.p[0].y = vt.p[1].y = i;

                svt.p[0].x = t3d.p[0].x + spl.x * dt;
                svt.p[1].x = t3d.p[0].x + spr.x * dt;
                svt.p[0].y = t3d.p[0].y + spl.y * dt;
                svt.p[1].y = t3d.p[0].y + spr.y * dt;
                // */
                vt.p[0].x = t2d.p[0].x + pl.x * dlt;
                vt.p[1].x = t2d.p[0].x + pr.x * drt;
                vt.p[0].y = vt.p[1].y = (float)(i);

                svt.p[0].x = t3d.p[0].x + spl.x * dlt;
                svt.p[1].x = t3d.p[0].x + spr.x * drt;
                svt.p[0].y = t3d.p[0].y + spl.y * dlt;
                svt.p[1].y = t3d.p[0].y + spr.y * drt;

                if (b_alpha) {
                    if (transparent) {
                        draw_flat_scanline_alphatrans_s(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    } else {
                        draw_flat_scanline_alpha_s(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    }
                } else {
                    if (transparent) {
                        draw_flat_scanline_transparent_s(dc_dest, &vt, dc_src, &svt, x1, x2);
                    } else {
                        draw_flat_scanline_s(dc_dest, &vt, dc_src, &svt, x1, x2);
                    }
                }
            }
        }
        if (pl.y > pr.y) {
            dd     = pr.y / pl.y;
            pl.x  *= dd;
            spl.x *= dd;
            spl.y *= dd;
        } else {
            dd     = pl.y / pr.y;
            pr.x  *= dd;
            spr.x *= dd;
            spr.y *= dd;
        }
        if (m >= y1 && m < y2 && m < e) {
            struct vector2d vt;
            struct vector2d svt;

            vt.p[0].x = t2d.p[0].x + pl.x;
            vt.p[1].x = t2d.p[0].x + pr.x;
            vt.p[0].y = vt.p[1].y = (float)(m);

            svt.p[0].x = t3d.p[0].x + spl.x;
            svt.p[1].x = t3d.p[0].x + spr.x;
            svt.p[0].y = t3d.p[0].y + spl.y;
            svt.p[1].y = t3d.p[0].y + spr.y;

            if (b_alpha) {
                if (transparent) {
                    draw_flat_scanline_alphatrans_s(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                } else {
                    draw_flat_scanline_alpha_s(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                }
            } else {
                if (transparent) {
                    draw_flat_scanline_transparent_s(dc_dest, &vt, dc_src, &svt, x1, x2);
                } else {
                    draw_flat_scanline_s(dc_dest, &vt, dc_src, &svt, x1, x2);
                }
            }
        }

        pl.x  = t2d.p[0].x - t2d.p[2].x;
        pr.x  = t2d.p[1].x - t2d.p[2].x;
        pl.y  = t2d.p[0].y - t2d.p[2].y;
        pr.y  = t2d.p[1].y - t2d.p[2].y;
        spl.x = t3d.p[0].x - t3d.p[2].x;
        spr.x = t3d.p[1].x - t3d.p[2].x;
        spl.y = t3d.p[0].y - t3d.p[2].y;
        spr.y = t3d.p[1].y - t3d.p[2].y;

        dh = e - dm;
        h  = e - m;
        re = e;
        if (m < y1) {
            m = y1 - 1;
        }
        if (e >= y2) {
            e = y2 - 1;
        }
        /*if (pl.y < pr.y) {
        dd = pr.y / pl.y;
        pl.x *= dd;
        spl.x *= dd;
        spl.y *= dd;
        } else {
        dd = pl.y / pr.y;
        pr.x *= dd;
        spr.x *= dd;
        spr.y *= dd;
        }// */
        if (pl.x > pr.x) {
            SWAP(pl, pr, pt);
            SWAP(spl, spr, pt);
        } else {
            ;
        }

        lh = float2int((float)(t2d.p[2].y)) - float2int((float)(pl.y + t2d.p[2].y));
        rh = float2int((float)(t2d.p[2].y)) - float2int((float)(pr.y + t2d.p[2].y));

        if (h > 0) {
            for (i = e; i > m; --i) {
                struct vector2d vt;
                struct vector2d svt;
                // float dt = (float)(re - i) / h;
                float dlt = (float)(re - i) / lh;
                float drt = (float)(re - i) / rh;

                /*
                vt.p[0].x = t2d.p[2].x + pl.x * dt;
                vt.p[1].x = t2d.p[2].x + pr.x * dt;
                vt.p[0].y = vt.p[1].y = i;

                svt.p[0].x = t3d.p[2].x + spl.x * dt;
                svt.p[1].x = t3d.p[2].x + spr.x * dt;
                svt.p[0].y = t3d.p[2].y + spl.y * dt;
                svt.p[1].y = t3d.p[2].y + spr.y * dt;
                // */
                vt.p[0].x = t2d.p[2].x + pl.x * dlt;
                vt.p[1].x = t2d.p[2].x + pr.x * drt;
                vt.p[0].y = vt.p[1].y = (float)(i);

                svt.p[0].x = t3d.p[2].x + spl.x * dlt;
                svt.p[1].x = t3d.p[2].x + spr.x * drt;
                svt.p[0].y = t3d.p[2].y + spl.y * dlt;
                svt.p[1].y = t3d.p[2].y + spr.y * drt;

                if (b_alpha) {
                    if (transparent) {
                        draw_flat_scanline_alphatrans_s(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    } else {
                        draw_flat_scanline_alpha_s(dc_dest, &vt, dc_src, &svt, x1, x2, alpha);
                    }
                } else {
                    if (transparent) {
                        draw_flat_scanline_transparent_s(dc_dest, &vt, dc_src, &svt, x1, x2);
                    } else {
                        draw_flat_scanline_s(dc_dest, &vt, dc_src, &svt, x1, x2);
                    }
                }
            }
        }
    }
}

int putimage_trangle(PIMAGE imgDest, PCIMAGE imgtexture,
    const struct trangle2d* dt, // dest trangle, original
    const struct trangle2d* tt, // textture trangle uv 0.0 - 1.0
    bool transparent, int alpha, bool smooth)
{
    PIMAGE  dc_dest = imgDest;
    PCIMAGE dc_src  = imgtexture;

    if (dc_dest) {
        struct trangle2d _dt = *dt;
        struct trangle2d _tt = *tt;
        int              x1 = 0, y1 = 0, x2 = dc_dest->getwidth(), y2 = dc_dest->getheight(), i;

        if (smooth) {
            for (i = 0; i < 3; ++i) {
                _tt.p[i].x = (float)float2int((float)(_tt.p[i].x * (dc_src->getwidth() - 2)));
                _tt.p[i].y = (float)float2int((float)(_tt.p[i].y * (dc_src->getheight() - 2)));
            }
        } else {
            for (i = 0; i < 3; ++i) {
                _tt.p[i].x = (float)float2int((float)(_tt.p[i].x * (dc_src->getwidth() - 1)));
                _tt.p[i].y = (float)float2int((float)(_tt.p[i].y * (dc_src->getheight() - 1)));
            }
        }

        if (smooth) {
            if (dc_src->getwidth() > 1 && dc_src->getheight() > 1) {
                draw_flat_trangle_alpha_s(dc_dest, &_dt, dc_src, &_tt, x1, y1, x2, y2, transparent, alpha);
            }
        } else {
            draw_flat_trangle_alpha(dc_dest, &_dt, dc_src, &_tt, x1, y1, x2, y2, transparent, alpha);
        }
    }
    return grOk;
}

int putimage_rotate(PIMAGE imgDest, PCIMAGE imgtexture, int xDest, int yDest, float centerx,
    float centery, float radian,
    bool transparent,
    int alpha,        // in range[0, 256], alpha==256 means no alpha
    bool smooth)
{
    PIMAGE  dc_dest = CONVERT_IMAGE(imgDest);
    PCIMAGE dc_src  = imgtexture;

    if (dc_dest) {
        struct trangle2d _tt[2];
        struct trangle2d _dt[2];
        double           dx, dy, cr = cos(radian), sr = -sin(radian);
        int              i, j;
        _tt[0].p[0].x = 0;
        _tt[0].p[0].y = 0;
        _tt[0].p[1].x = 0;
        _tt[0].p[1].y = 1;
        _tt[0].p[2].x = 1;
        _tt[0].p[2].y = 1;
        _tt[1].p[0]   = _tt[0].p[2];
        _tt[1].p[1].x = 1;
        _tt[1].p[1].y = 0;
        _tt[1].p[2]   = _tt[0].p[0];
        memcpy(&_dt, &_tt, sizeof(trangle2d) * 2);

        for (j = 0; j < 2; ++j) {
            for (i = 0; i < 3; ++i) {
                _dt[j].p[i].x = (_dt[j].p[i].x - centerx) * (dc_src->getwidth());
                _dt[j].p[i].y = (_dt[j].p[i].y - centery) * (dc_src->getheight());
                dx            = cr * _dt[j].p[i].x - sr * _dt[j].p[i].y;
                dy            = sr * _dt[j].p[i].x + cr * _dt[j].p[i].y;
                _dt[j].p[i].x = (float)float2int((float)((dx + xDest) + FLOAT_EPS));
                _dt[j].p[i].y = (float)float2int((float)((dy + yDest) + FLOAT_EPS));
            }
        }

        putimage_trangle(dc_dest, imgtexture, &_dt[0], &_tt[0], transparent, alpha, smooth);
        putimage_trangle(dc_dest, imgtexture, &_dt[1], &_tt[1], transparent, alpha, smooth);
    }
    CONVERT_IMAGE_END;
    return grOk;
}

int putimage_rotatezoom(PIMAGE imgDest, PCIMAGE imgtexture, int xDest, int yDest, float centerx,
    float centery, float radian, float zoom,
    bool transparent, // transparent (1) or not (0)
    int alpha,        // in range[0, 256], alpha==256 means no alpha
    bool smooth)
{
    PIMAGE  dc_dest = CONVERT_IMAGE(imgDest);
    PCIMAGE dc_src  = imgtexture;
    if (dc_dest) {
        struct trangle2d _tt[2];
        struct trangle2d _dt[2];
        double           dx, dy, cr = cos(radian), sr = -sin(radian);
        int              i, j;
        _tt[0].p[0].x = 0;
        _tt[0].p[0].y = 0;
        _tt[0].p[1].x = 0;
        _tt[0].p[1].y = 1;
        _tt[0].p[2].x = 1;
        _tt[0].p[2].y = 1;
        _tt[1].p[0]   = _tt[0].p[2];
        _tt[1].p[1].x = 1;
        _tt[1].p[1].y = 0;
        _tt[1].p[2]   = _tt[0].p[0];
        memcpy(&_dt, &_tt, sizeof(trangle2d) * 2);

        for (j = 0; j < 2; ++j) {
            for (i = 0; i < 3; ++i) {
                _dt[j].p[i].x = (_dt[j].p[i].x - centerx) * (dc_src->getwidth());
                _dt[j].p[i].y = (_dt[j].p[i].y - centery) * (dc_src->getheight());
                dx            = cr * _dt[j].p[i].x - sr * _dt[j].p[i].y;
                dy            = sr * _dt[j].p[i].x + cr * _dt[j].p[i].y;
                _dt[j].p[i].x = (float)float2int((float)((dx * zoom + xDest) + FLOAT_EPS));
                _dt[j].p[i].y = (float)float2int((float)((dy * zoom + yDest) + FLOAT_EPS));
            }
        }

        putimage_trangle(dc_dest, imgtexture, &_dt[0], &_tt[0], transparent, alpha, smooth);
        putimage_trangle(dc_dest, imgtexture, &_dt[1], &_tt[1], transparent, alpha, smooth);
    }
    CONVERT_IMAGE_END;
    return grOk;
}

int putimage_rotatetransparent(PIMAGE imgDest, PCIMAGE imgSrc, int xCenterDest, int yCenterDest, int xOriginSrc,
    int yOriginSrc, int widthSrc, int heightSrc, int xCenterSrc, int yCenterSrc, color_t transparentColor, float radian,
    float zoom)
{
    const PIMAGE img             = CONVERT_IMAGE(imgDest);
    int          zoomed_width    = widthSrc * zoom;
    int          zoomed_height   = heightSrc * zoom;
    int          zoomed_center_x = (xCenterSrc - xOriginSrc) * zoom;
    int          zoomed_center_y = (yCenterSrc - yOriginSrc) * zoom;
    /* zoom */
    PIMAGE zoomed_img = newimage(zoomed_width, zoomed_height);
    putimage(
        zoomed_img, 0, 0, zoomed_width, zoomed_height, imgSrc, xOriginSrc, yOriginSrc, widthSrc, heightSrc, SRCCOPY);
    /* rotation */
    for (int x = 0; x < zoomed_width; x++) {
        for (int y = 0; y < zoomed_height; y++) {
            /* zoomed_img is newly created and have no transform/viewport, so we can use buffer directly */
            color_t color = zoomed_img->m_pBuffer[y * zoomed_img->m_width + x];
            double  src_x = ((x - zoomed_center_x) * cos(radian) - (y - zoomed_center_y) * sin(radian)) + xCenterDest;
            double  src_y = ((x - zoomed_center_x) * sin(radian) + (y - zoomed_center_y) * cos(radian)) + yCenterDest;
            if (color != transparentColor) {
                /*
                the rotated pixel may span(partly) more than one pixels
                see:
                https://stackoverflow.com/questions/36201381/how-to-rotate-image-canvas-pixel-manipulation
                */
                putpixel_savealpha(src_x, src_y, color, img);
                putpixel_savealpha(src_x + 0.5, src_y, color, img);
                putpixel_savealpha(src_x, src_y + 0.5, color, img);
                putpixel_savealpha(src_x + 0.5, src_y + 0.5, color, img);
            }
        }
    }
    delimage(zoomed_img);
    CONVERT_IMAGE_END;
    return grOk;
};

int putimage_rotatetransparent(PIMAGE imgDest, PCIMAGE imgSrc, int xCenterDest, int yCenterDest, int xCenterSrc,
    int yCenterSrc, color_t transparentColor, float radian, float zoom)
{
    return putimage_rotatetransparent(imgDest, imgSrc, xCenterDest, yCenterDest, 0, 0, imgSrc->getwidth(),
        imgSrc->getheight(), xCenterSrc, yCenterSrc, transparentColor, radian, zoom);
}

int getwidth(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        return img->m_width;
    }

    CONVERT_IMAGE_END;
    return 0;
}

int getheight(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        return img->m_height;
    }

    CONVERT_IMAGE_END;
    return 0;
}

int getx(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        CONVERT_IMAGE_END;
        return pt.x;
    }

    CONVERT_IMAGE_END;
    return -1;
}

int gety(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        CONVERT_IMAGE_END;
        return pt.y;
    }

    CONVERT_IMAGE_END;
    return -1;
}

PIMAGE newimage()
{
    return new IMAGE(1, 1);
}

PIMAGE newimage(int width, int height)
{
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    return new IMAGE(width, height);
}

void delimage(PCIMAGE pImg)
{
    delete const_cast<PIMAGE>(pImg);
}

color_t* getbuffer(PIMAGE pImg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pImg);
    CONVERT_IMAGE_END;
    return img->getbuffer();
}

const color_t* getbuffer(PCIMAGE pImg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pImg);
    CONVERT_IMAGE_END;
    return img->getbuffer();
}

HDC getHDC(PCIMAGE pImg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pImg);
    CONVERT_IMAGE_END;
    return img->getdc();
}

int resize_f(PIMAGE imgDest, int width, int height)
{
    return imgDest->resize_f(width, height);
}

int resize(PIMAGE imgDest, int width, int height)
{
    return imgDest->resize(width, height);
}

#define EGE_GETIMAGE_CHK_NULL(p)                                          \
    do {                                                                  \
        if (p == NULL)                                                    \
            internal_panic(L"Fatal Error: pass NULL to `ege::getimage`"); \
    } while (0)

int getimage(PIMAGE imgDest, int xSrc, int ySrc, int srcWidth, int srcHeight)
{
    EGE_GETIMAGE_CHK_NULL(imgDest);
    return imgDest->getimage(xSrc, ySrc, srcWidth, srcHeight);
}

int getimage(PIMAGE imgDest, PCIMAGE pSrcImg, int xSrc, int ySrc, int srcWidth, int srcHeight)
{
    EGE_GETIMAGE_CHK_NULL(imgDest);
    return imgDest->getimage(pSrcImg, xSrc, ySrc, srcWidth, srcHeight);
}

void putimage(int xDest, int yDest, PCIMAGE pSrcImg, DWORD dwRop)
{
    pSrcImg = CONVERT_IMAGE_CONST(pSrcImg);
    pSrcImg->putimage(xDest, yDest, dwRop);
}

void putimage(int xDest, int yDest, int widthDest, int heightDest, PCIMAGE pSrcImg, int xSrc, int ySrc, DWORD dwRop)
{
    pSrcImg = CONVERT_IMAGE_CONST(pSrcImg);
    pSrcImg->putimage(xDest, yDest, widthDest, heightDest, xSrc, ySrc, dwRop);
}

void putimage(PIMAGE imgDest, int xDest, int yDest, PCIMAGE pSrcImg, DWORD dwRop)
{
    pSrcImg = CONVERT_IMAGE_CONST(pSrcImg);
    pSrcImg->putimage(imgDest, xDest, yDest, dwRop);
}

void putimage(
    PIMAGE imgDest, int xDest, int yDest, int widthDest, int heightDest, PCIMAGE pSrcImg, int xSrc, int ySrc, DWORD dwRop)
{
    pSrcImg = CONVERT_IMAGE_CONST(pSrcImg);
    pSrcImg->putimage(imgDest, xDest, yDest, widthDest, heightDest, xSrc, ySrc, dwRop);
}

int getimage(PIMAGE imgDest, const char* imageFile, int zoomWidth, int zoomHeight)
{
    EGE_GETIMAGE_CHK_NULL(imgDest);
    return imgDest->getimage(imageFile, zoomWidth, zoomHeight);
}

int getimage(PIMAGE imgDest, const wchar_t* imageFile, int zoomWidth, int zoomHeight)
{
    EGE_GETIMAGE_CHK_NULL(imgDest);
    return imgDest->getimage(imageFile, zoomWidth, zoomHeight);
}

int getimage(PIMAGE imgDest, const char* resType, const char* resName, int zoomWidth, int zoomHeight)
{
    EGE_GETIMAGE_CHK_NULL(imgDest);
    return imgDest->getimage(resType, resName, zoomWidth, zoomHeight);
}

int getimage(PIMAGE imgDest, const wchar_t* resType, const wchar_t* resName, int zoomWidth, int zoomHeight)
{
    EGE_GETIMAGE_CHK_NULL(imgDest);
    return imgDest->getimage(resType, resName, zoomWidth, zoomHeight);
}

void putimage(PIMAGE imgDest, int xDest, int yDest, int widthDest, int heightDest, PCIMAGE pSrcImg, int xSrc, int ySrc,
    int srcWidth, int srcHeight, DWORD dwRop)
{
    pSrcImg = CONVERT_IMAGE_CONST(pSrcImg);
    pSrcImg->putimage(imgDest, xDest, yDest, widthDest, heightDest, xSrc, ySrc, srcWidth, srcHeight, dwRop);
}

void putimage(int xDest, int yDest, int widthDest, int heightDest, PCIMAGE pSrcImg, int xSrc, int ySrc, int srcWidth,
    int srcHeight, DWORD dwRop)
{
    pSrcImg = CONVERT_IMAGE_CONST(pSrcImg);
    pSrcImg->putimage(NULL, xDest, yDest, widthDest, heightDest, xSrc, ySrc, srcWidth, srcHeight, dwRop);
}

int putimage_transparent(PIMAGE imgDest,       // handle to dest
    PCIMAGE                     imgSrc,        // handle to source
    int                         xDest,  // x-coord of destination upper-left corner
    int                         yDest,  // y-coord of destination upper-left corner
    color_t                     transparentColor, // color to make transparent
    int                         xSrc,   // x-coord of source upper-left corner
    int                         ySrc,   // y-coord of source upper-left corner
    int                         widthSrc,     // width of source rectangle
    int                         heightSrc     // height of source rectangle
)
{
    imgSrc = CONVERT_IMAGE_CONST(imgSrc);
    return imgSrc->putimage_transparent(
        imgDest, xDest, yDest, transparentColor, xSrc, ySrc, widthSrc, heightSrc);
}

int putimage_alphablend(PIMAGE imgDest,      // handle to dest
    PCIMAGE                    imgSrc,       // handle to source
    int                        xDest, // x-coord of destination upper-left corner
    int                        yDest, // y-coord of destination upper-left corner
    unsigned char              alpha,        // alpha
    int                        xSrc,  // x-coord of source upper-left corner
    int                        ySrc,  // y-coord of source upper-left corner
    int                        widthSrc,    // width of source rectangle
    int                        heightSrc    // height of source rectangle
)
{
    imgSrc = CONVERT_IMAGE_CONST(imgSrc);
    return imgSrc->putimage_alphablend(
        imgDest, xDest, yDest, alpha, xSrc, ySrc, widthSrc, heightSrc);
}

int putimage_alphatransparent(PIMAGE imgDest,       // handle to dest
    PCIMAGE                          imgSrc,        // handle to source
    int                              xDest,  // x-coord of destination upper-left corner
    int                              yDest,  // y-coord of destination upper-left corner
    color_t                          transparentColor, // color to make transparent
    unsigned char                    alpha,         // alpha
    int                              xSrc,   // x-coord of source upper-left corner
    int                              ySrc,   // y-coord of source upper-left corner
    int                              widthSrc,     // width of source rectangle
    int                              heightSrc     // height of source rectangle
)
{
    imgSrc = CONVERT_IMAGE_CONST(imgSrc);
    return imgSrc->putimage_alphatransparent(
        imgDest, xDest, yDest, transparentColor, alpha, xSrc, ySrc, widthSrc, heightSrc);
}

int putimage_withalpha(PIMAGE imgDest,  // handle to dest
    PCIMAGE                   imgSrc,   // handle to source
    int                       xDest,    // x-coord of destination upper-left corner
    int                       yDest,    // y-coord of destination upper-left corner
    int                       xSrc,     // x-coord of source upper-left corner
    int                       ySrc,     // y-coord of source upper-left corner
    int                       widthSrc, // width of source rectangle
    int                       heightSrc // height of source rectangle
)
{
    imgSrc = CONVERT_IMAGE_CONST(imgSrc);
    return imgSrc->putimage_withalpha(
        imgDest, xDest, yDest, xSrc, ySrc, widthSrc, heightSrc);
}

int EGEAPI putimage_withalpha(PIMAGE imgDest,    // handle to dest
    PCIMAGE                          imgSrc,     // handle to source
    int                              xDest,      // x-coord of destination upper-left corner
    int                              yDest,      // y-coord of destination upper-left corner
    int                              widthDest,  // width of destination rectangle
    int                              heightDest, // height of destination rectangle
    int                              xSrc,       // x-coord of source upper-left corner
    int                              ySrc,       // y-coord of source upper-left corner
    int                              widthSrc,   // width of source rectangle
    int                              heightSrc,  // height of source rectangle
    bool                             smooth
)
{
    imgSrc = CONVERT_IMAGE_CONST(imgSrc);
    return imgSrc->putimage_withalpha(
        imgDest, xDest, yDest, widthDest, heightDest, xSrc, ySrc, widthSrc, heightSrc, smooth);
}

int putimage_alphafilter(PIMAGE imgDest,    // handle to dest
    PCIMAGE                     imgSrc,     // handle to source
    int                         xDest,      // x-coord of destination upper-left corner
    int                         yDest,      // y-coord of destination upper-left corner
    PCIMAGE                     imgAlpha,   // alpha
    int                         xSrc,       // x-coord of source upper-left corner
    int                         ySrc,       // y-coord of source upper-left corner
    int                         widthSrc,   // width of source rectangle
    int                         heightSrc   // height of source rectangle
)
{
    imgSrc = CONVERT_IMAGE_CONST(imgSrc);
    return imgSrc->putimage_alphafilter(
        imgDest, xDest, yDest, imgAlpha, xSrc, ySrc, widthSrc, heightSrc);
}

int imagefilter_blurring(
    PIMAGE imgDest, int intensity, int alpha, int xDest, int yDest, int widthDest, int heightDest)
{
    PIMAGE img = CONVERT_IMAGE(imgDest);
    int    ret = 0;

    if (img) {
        ret = img->imagefilter_blurring(intensity, alpha, xDest, yDest, widthDest, heightDest);
    }

    CONVERT_IMAGE_END;
    return ret;
}

static BOOL nocaseends(LPCWSTR suffix, LPCWSTR text)
{
    int     len_suffix, len_text;
    LPCWSTR p_suffix;
    LPCWSTR p_text;
    len_suffix = (int)wcslen(suffix);
    len_text   = (int)wcslen(text);

    if ((len_text < len_suffix) || (len_text == 0)) {
        return FALSE;
    }

    p_suffix = suffix;
    p_text   = (text + (len_text - len_suffix));

    while (*p_text != 0) {
        if (towupper(*p_text) != towupper(*p_suffix)) {
            return FALSE;
        }
        p_text++;
        p_suffix++;
    }

    return TRUE;
}

int saveimage(PCIMAGE pimg, const char* filename, bool withAlphaChannel)
{
    const std::wstring& filename_w = mb2w(filename);
    return saveimage(pimg, filename_w.c_str(), withAlphaChannel);
}

int saveimage(PCIMAGE pimg, const wchar_t* filename, bool withAlphaChannel)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    int     ret = 0;

    if (img) {
        if (nocaseends(L".bmp", filename)) {
            ret = savebmp(pimg, filename, withAlphaChannel);
        } else if (nocaseends(L".png", filename)) {
            ret = savepng(pimg, filename, withAlphaChannel);
        } else {
            ret = savepng(pimg, filename, withAlphaChannel);
        }
    }

    CONVERT_IMAGE_END;
    return ret;
}

int getimage_pngfile(PIMAGE pimg, const char* filename)
{
    const std::wstring& filename_w = mb2w(filename);
    return getimage_pngfile(pimg, filename_w.c_str());
}

int getimage_pngfile(PIMAGE pimg, const wchar_t* filename)
{
    FILE* fp = NULL;
    int   ret;
    fp = _wfopen(filename, L"rb");

    if (fp == NULL) {
        return grFileNotFound;
    }

    ret = pimg->getpngimg(fp);
    fclose(fp);
    return ret;
}

int savepng(PCIMAGE pimg, const char* filename, bool withAlphaChannel)
{
    const std::wstring& filename_w = mb2w(filename);
    return savepng(pimg, filename_w.c_str(), withAlphaChannel);
}

int savepng(PCIMAGE pimg, const wchar_t* filename, bool withAlphaChannel)
{
    FILE* fp = NULL;
    int   ret;
    pimg = CONVERT_IMAGE_CONST(pimg);
    fp   = _wfopen(filename, L"wb");

    if (fp == NULL) {
        return grFileNotFound;
    }

    ret = pimg->savepngimg(fp, withAlphaChannel);
    fclose(fp);
    return ret;
}

/**
 * @brief 将图像以 BMP 格式保存到文件中
 *
 * @param pimg              要保存的图像
 * @param filename          文件名
 * @param withAlphaChannel  是否保存 alpha 通道
 * @return int  错误码(graphics_errors)
 * @note  保存 alpha 通道则使用 BITMAPV4HEADER, 不保存则使用 BITMAPINFOHEADER
 */
int savebmp(PCIMAGE pimg, const char* filename, bool withAlphaChannel)
{
    return savebmp(pimg, mb2w(filename).c_str(), withAlphaChannel);
}

/**
 * @brief 将图像以 BMP 格式保存到文件中
 *
 * @param pimg              要保存的图像
 * @param filename          文件名
 * @param withAlphaChannel  是否保存 alpha 通道
 * @return int  错误码(graphics_errors)
 * @note  保存 alpha 通道则使用 BITMAPV4HEADER, 不保存则使用 BITMAPINFOHEADER
 */
int savebmp(PCIMAGE pimg, const wchar_t* filename, bool withAlphaChannel)
{
    FILE* file = _wfopen(filename, L"wb");
    if (file == NULL) {
        return grIOerror;
    }

    int errorCode = savebmp(pimg, file, withAlphaChannel);
    fclose(file);
    return errorCode;
}

/**
 * @brief 将图像以 BMP 格式写入 file 所指的文件中
 *
 * @param pimg             要保存的图像
 * @param file             文件指针
 * @param withAlphaChannel 是否保存 alpha 通道
 * @return int  错误码(graphics_errors)
 * @note  保存 alpha 通道则使用 BITMAPV4HEADER, 不保存则使用 BITMAPINFOHEADER
 */
int savebmp(PCIMAGE pimg, FILE* file, bool withAlphaChannel)
{
    if (file == NULL)
        return grIOerror;

    int infoHeaderSize = 0;  // InfoHeader 大小 (BITMAPINFOHEADER: 40), (BITMAPV4HEADER: 108)
    int bytesPerPixel  = 4;  // 每像素所占字节数
    int paddingBytes   = 0;  // 每行填充字节数( BMP 格式规定:除非使用 RLE 压缩，否则每行像素必须是4字节对齐)
    DWORD Compression  = 0;  // 图像压缩方式

    if (withAlphaChannel) {
        // 以 ARGB 格式保存, 使用 BITMAPV4HEADER
        infoHeaderSize = sizeof(BITMAPV4HEADER);    // 108
        bytesPerPixel = 4;                          // 32 位
        Compression = BI_BITFIELDS;                 // ARGB (各颜色分量由 Mask 确定)
    } else {
        // 以 RGB24 格式保存, 使用 BITMAPINFOHEADER
        infoHeaderSize = sizeof(BITMAPINFOHEADER);  // 40
        bytesPerPixel = 3;                          // 24 位
        Compression = BI_RGB;                       // RGB

        // 计算每行填充字节(每行4字节对齐)
        int originalBytesPerRow  = getwidth(pimg) * bytesPerPixel;
        int remBytes = originalBytesPerRow % 4;
        if (remBytes != 0) {
            paddingBytes = 4 - remBytes;
        }
    }

    int bytesPerRow = bytesPerPixel * getwidth(pimg) + paddingBytes;    // 每行所占字节数(包括填充字节)

    BITMAPV4HEADER bitmapInfoHeader = {0};
    // -------- BITMAPINFOHEADER, BITMAPV4HEADER 共有参数 --------
    bitmapInfoHeader.bV4Size          = infoHeaderSize;                 // Header 大小(可用于识别 Header 版本)
    bitmapInfoHeader.bV4Width         = getwidth(pimg);                 // 位图宽度(单位为像素)
    bitmapInfoHeader.bV4Height        = getheight(pimg);                // 位图高度(单位为像素, 正数表示从下到上存储, 负数为从上到下)
    bitmapInfoHeader.bV4Planes        = 1;                              // 固定值，必须为 1
    bitmapInfoHeader.bV4BitCount      = (WORD)bytesPerPixel * 8;        // 每像素的位数(即 位深度)
    bitmapInfoHeader.bV4V4Compression = Compression;                    // 压缩方式
    bitmapInfoHeader.bV4SizeImage     = bytesPerRow * getheight(pimg);  // 图像像素部分所占大小(单位为字节，包括填充字节)
    bitmapInfoHeader.bV4XPelsPerMeter = 3780;                           // 96 DPI x 39.3701(inch/m)
    bitmapInfoHeader.bV4YPelsPerMeter = 3780;                           // 96 DPI x 39.3701(inch/m)
    bitmapInfoHeader.bV4ClrUsed       = 0;                              // 不使用颜色表
    bitmapInfoHeader.bV4ClrImportant  = 0;                              // 颜色表中所有颜色都重要

    // --------------- BITMAPV4HEADER 特有参数 ------------------
    bitmapInfoHeader.bV4RedMask       = 0x00FF0000;
    bitmapInfoHeader.bV4GreenMask     = 0x0000FF00;
    bitmapInfoHeader.bV4BlueMask      = 0x000000FF;
    bitmapInfoHeader.bV4AlphaMask     = 0xFF000000;
    bitmapInfoHeader.bV4CSType        = LCS_sRGB;                       // 使用标准 RGB 颜色空间
    // 当 bV4CSType 为 'sRGB' 或 'Win ' 时忽略以下参数
    //bitmapInfoHeader.bV4Endpoints
    //bitmapInfoHeader.bV4GammaRed
    //bitmapInfoHeader.bV4GammaGreen
    //bitmapInfoHeader.bV4GammaBlue

    BITMAPFILEHEADER bitmapFileHeader = {0};
    bitmapFileHeader.bfType    = (WORD&)"BM";                               // Windows Bitmap 格式
    bitmapFileHeader.bfSize    = sizeof(BITMAPFILEHEADER) + infoHeaderSize + bytesPerRow * getheight(pimg) ; // BMP 文件总字节数
    bitmapFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + infoHeaderSize; // 从文件起始至像素数据的偏移字节数

    // 1. 写入 File Header
    if (fwrite(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, file) != 1) {
        return grIOerror;
    };

    // 2. 写入 Info Header
    if (fwrite(&bitmapInfoHeader, infoHeaderSize, 1, file) != 1) {
        return grIOerror;
    };

    const color_t* buffer = getbuffer(pimg);
    const int rowCnt = getheight(pimg);
    const int colCnt = getwidth(pimg);

    // 3. 写入图像数据(从下到上进行存储)
    if (bytesPerPixel == 4) {
        for (int row = rowCnt-1; row >= 0; row--) {
            if (fwrite(&buffer[row * colCnt], bytesPerRow, 1, file) != 1) {
                return grIOerror;
            }
        }
    } else if (bytesPerPixel == 3) {
        const unsigned char zeroPadding[4] = {0};

        for (int row = rowCnt-1; row >= 0; row--) {
            const color_t* pixels = &buffer[row * colCnt];   // 每行像素首地址

            for(int col = 0; col < colCnt; col++) {
                if (fwrite(&pixels[col], bytesPerPixel, 1, file) != 1) {
                    return grIOerror;
                }
            }

            // 每行末尾填充对齐 4 字节
            if (paddingBytes != 0) {
                if (fwrite(zeroPadding, paddingBytes,1, file) != 1) {
                    return grIOerror;
                }
            }
        }
    }

    return grOk;
}


void ege_enable_aa(bool enable, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    img->enable_anti_alias(enable);
    CONVERT_IMAGE_END;
}

} // namespace ege
