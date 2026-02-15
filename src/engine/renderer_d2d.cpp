#include "renderer_d2d.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <SDL2/SDL_syswm.h>
#include <cmath>
#include <string>
#include <vector>
#include <thread>
#include <cstring>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

struct D2DRenderer
{
    HWND hwnd;
    ID2D1Factory* factory;
    ID2D1HwndRenderTarget* target;
    ID2D1SolidColorBrush* brush;
    IDWriteFactory* writeFactory;
    IWICImagingFactory* wicFactory;
    IDWriteTextFormat* textFormats[128];
    int width;
    int height;
};

template <typename T>
static void SafeRelease(T** value)
{
    if (*value)
    {
        (*value)->Release();
        *value = nullptr;
    }
}

static D2D1_COLOR_F ToD2DColor(Color c)
{
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

static bool ToWide(const char* utf8, std::wstring& out)
{
    if (!utf8)
    {
        out.clear();
        return true;
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (required <= 0)
    {
        out.clear();
        return false;
    }

    out.resize((size_t)required - 1);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), required);
    return true;
}

static bool CreateTarget(D2DRenderer* ctx, int width, int height)
{
    if (!ctx || !ctx->factory)
    {
        return false;
    }

    SafeRelease(&ctx->target);
    SafeRelease(&ctx->brush);

    HRESULT hr = ctx->factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(ctx->hwnd, D2D1::SizeU((UINT32)width, (UINT32)height)),
        &ctx->target
    );
    if (FAILED(hr))
    {
        return false;
    }

    hr = ctx->target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &ctx->brush);
    if (FAILED(hr))
    {
        SafeRelease(&ctx->target);
        return false;
    }

    ctx->target->SetDpi(96.0f, 96.0f);
    ctx->target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    ctx->target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    ctx->width = width;
    ctx->height = height;
    return true;
}

D2DRenderer* D2DRenderer_Create(SDL_Window* window, int width, int height)
{
    if (!window)
    {
        return nullptr;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo))
    {
        return nullptr;
    }

    D2DRenderer* ctx = new D2DRenderer();
    ctx->hwnd = wmInfo.info.win.window;
    ctx->factory = nullptr;
    ctx->target = nullptr;
    ctx->brush = nullptr;
    ctx->writeFactory = nullptr;
    ctx->wicFactory = nullptr;
    memset(ctx->textFormats, 0, sizeof(ctx->textFormats));
    ctx->width = width;
    ctx->height = height;

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &ctx->factory);
    if (FAILED(hr))
    {
        delete ctx;
        return nullptr;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&ctx->writeFactory)
    );
    if (FAILED(hr))
    {
        SafeRelease(&ctx->factory);
        delete ctx;
        return nullptr;
    }

    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&ctx->wicFactory)
    );
    if (FAILED(hr))
    {
        SafeRelease(&ctx->writeFactory);
        SafeRelease(&ctx->factory);
        delete ctx;
        return nullptr;
    }

    if (!CreateTarget(ctx, width, height))
    {
        D2DRenderer_Destroy(ctx);
        return nullptr;
    }

    return ctx;
}

void D2DRenderer_Destroy(D2DRenderer* ctx)
{
    if (!ctx)
    {
        return;
    }

    for (int i = 0; i < 128; ++i)
    {
        SafeRelease(&ctx->textFormats[i]);
    }
    SafeRelease(&ctx->brush);
    SafeRelease(&ctx->target);
    SafeRelease(&ctx->writeFactory);
    SafeRelease(&ctx->wicFactory);
    SafeRelease(&ctx->factory);
    delete ctx;
}

void D2DRenderer_Resize(D2DRenderer* ctx, int width, int height)
{
    if (!ctx)
    {
        return;
    }

    if (ctx->target)
    {
        ctx->target->Resize(D2D1::SizeU((UINT32)width, (UINT32)height));
        ctx->width = width;
        ctx->height = height;
    }
}

int D2DRenderer_BeginFrame(D2DRenderer* ctx)
{
    if (!ctx || !ctx->target)
    {
        return 0;
    }

    ctx->target->BeginDraw();
    return 1;
}

void D2DRenderer_EndFrame(D2DRenderer* ctx)
{
    if (!ctx || !ctx->target)
    {
        return;
    }

    HRESULT hr = ctx->target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        CreateTarget(ctx, ctx->width, ctx->height);
    }
}

void D2DRenderer_Clear(D2DRenderer* ctx, Color color)
{
    if (!ctx || !ctx->target)
    {
        return;
    }

    ctx->target->Clear(ToD2DColor(color));
}

void D2DRenderer_DrawRectangle(D2DRenderer* ctx, Rect rect, Color color)
{
    if (!ctx || !ctx->target || !ctx->brush)
    {
        return;
    }

    ctx->brush->SetColor(ToD2DColor(color));
    D2D1_RECT_F r = D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
    ctx->target->FillRectangle(r, ctx->brush);
}

void D2DRenderer_DrawRectangleLines(D2DRenderer* ctx, Rect rect, float line_thick, Color color)
{
    if (!ctx || !ctx->target || !ctx->brush)
    {
        return;
    }

    ctx->brush->SetColor(ToD2DColor(color));
    D2D1_RECT_F r = D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);
    float stroke = (line_thick > 1.0f) ? line_thick : 1.0f;
    ctx->target->DrawRectangle(r, ctx->brush, stroke);
}

void D2DRenderer_DrawLine(D2DRenderer* ctx, Vec2 start, Vec2 end, float thick, Color color)
{
    if (!ctx || !ctx->target || !ctx->brush)
    {
        return;
    }

    ctx->brush->SetColor(ToD2DColor(color));
    float stroke = (thick > 1.0f) ? thick : 1.0f;
    ctx->target->DrawLine(D2D1::Point2F(start.x, start.y), D2D1::Point2F(end.x, end.y), ctx->brush, stroke);
}

void D2DRenderer_DrawCircle(D2DRenderer* ctx, Vec2 center, float radius, Color color, int filled)
{
    if (!ctx || !ctx->target || !ctx->brush)
    {
        return;
    }

    ctx->brush->SetColor(ToD2DColor(color));
    D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F(center.x, center.y), radius, radius);
    if (filled)
    {
        ctx->target->FillEllipse(e, ctx->brush);
    }
    else
    {
        ctx->target->DrawEllipse(e, ctx->brush, 1.0f);
    }
}

void D2DRenderer_DrawTriangle(D2DRenderer* ctx, Vec2 v1, Vec2 v2, Vec2 v3, Color color, int filled)
{
    if (!ctx || !ctx->factory || !ctx->target || !ctx->brush)
    {
        return;
    }

    ID2D1PathGeometry* geometry = nullptr;
    ID2D1GeometrySink* sink = nullptr;
    if (FAILED(ctx->factory->CreatePathGeometry(&geometry)))
    {
        return;
    }
    if (FAILED(geometry->Open(&sink)))
    {
        SafeRelease(&geometry);
        return;
    }

    sink->BeginFigure(D2D1::Point2F(v1.x, v1.y), filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddLine(D2D1::Point2F(v2.x, v2.y));
    sink->AddLine(D2D1::Point2F(v3.x, v3.y));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    ctx->brush->SetColor(ToD2DColor(color));
    if (filled)
    {
        ctx->target->FillGeometry(geometry, ctx->brush);
    }
    else
    {
        ctx->target->DrawGeometry(geometry, ctx->brush, 1.0f);
    }

    SafeRelease(&sink);
    SafeRelease(&geometry);
}

void D2DRenderer_DrawTrianglesColored(D2DRenderer* ctx, const float* positions, const float* colors, int vertexCount)
{
    if (!ctx || !positions || !colors || vertexCount < 3)
    {
        return;
    }

    struct RectRun
    {
        Rect r;
        Color c;
    };
    struct TriRun
    {
        Vec2 a;
        Vec2 b;
        Vec2 c;
        Color col;
    };

    auto colorEq = [](const float* a, const float* b) -> bool
    {
        const float eps = 0.0001f;
        return fabsf(a[0] - b[0]) < eps &&
               fabsf(a[1] - b[1]) < eps &&
               fabsf(a[2] - b[2]) < eps &&
               fabsf(a[3] - b[3]) < eps;
    };

    std::vector<RectRun> rects;
    std::vector<TriRun> tris;

    const bool canThread = (vertexCount % 6 == 0) && (vertexCount >= 12000);
    if (canThread)
    {
        int groups = vertexCount / 6;
        unsigned int hw = std::thread::hardware_concurrency();
        int threads = (hw == 0) ? 4 : (int)hw;
        if (threads > 8) threads = 8;
        if (threads < 1) threads = 1;
        if (groups < threads) threads = groups;

        struct LocalOut
        {
            std::vector<RectRun> rects;
            std::vector<TriRun> tris;
        };
        std::vector<LocalOut> local((size_t)threads);
        std::vector<std::thread> workers;
        workers.reserve((size_t)threads);

        for (int t = 0; t < threads; ++t)
        {
            int start = (groups * t) / threads;
            int end = (groups * (t + 1)) / threads;
            workers.emplace_back([&, t, start, end]()
            {
                auto& out = local[(size_t)t];
                out.rects.reserve((size_t)(end - start));
                for (int g = start; g < end; ++g)
                {
                    int i = g * 6;
                    const float* c0 = &colors[(i + 0) * 4];
                    const float* c1 = &colors[(i + 1) * 4];
                    const float* c2 = &colors[(i + 2) * 4];
                    const float* c3 = &colors[(i + 3) * 4];
                    const float* c4 = &colors[(i + 4) * 4];
                    const float* c5 = &colors[(i + 5) * 4];

                    if (colorEq(c0, c1) && colorEq(c0, c2) && colorEq(c0, c3) && colorEq(c0, c4) && colorEq(c0, c5))
                    {
                        float x0 = positions[(i + 0) * 2 + 0];
                        float y0 = positions[(i + 0) * 2 + 1];
                        float x1 = positions[(i + 1) * 2 + 0];
                        float y1 = positions[(i + 1) * 2 + 1];
                        float x2 = positions[(i + 2) * 2 + 0];
                        float y2 = positions[(i + 2) * 2 + 1];
                        float x4 = positions[(i + 4) * 2 + 0];
                        float y4 = positions[(i + 4) * 2 + 1];
                        float x5 = positions[(i + 5) * 2 + 0];
                        float y5 = positions[(i + 5) * 2 + 1];

                        const float eps = 0.0001f;
                        bool quadPattern =
                            fabsf(y0 - y1) < eps &&
                            fabsf(x1 - x2) < eps &&
                            fabsf(y2 - y4) < eps &&
                            fabsf(x4 - x5) < eps;

                        if (quadPattern)
                        {
                            float minX = (x0 < x1) ? x0 : x1;
                            float maxX = (x0 > x1) ? x0 : x1;
                            float minY = (y0 < y2) ? y0 : y2;
                            float maxY = (y0 > y2) ? y0 : y2;
                            out.rects.push_back(RectRun{
                                Rect{ minX, minY, maxX - minX, maxY - minY },
                                Color{ c0[0], c0[1], c0[2], c0[3] }
                            });
                            continue;
                        }
                    }

                    TriRun t0 = {
                        Vec2{ positions[(i + 0) * 2 + 0], positions[(i + 0) * 2 + 1] },
                        Vec2{ positions[(i + 1) * 2 + 0], positions[(i + 1) * 2 + 1] },
                        Vec2{ positions[(i + 2) * 2 + 0], positions[(i + 2) * 2 + 1] },
                        Color{
                            (colors[(i + 0) * 4 + 0] + colors[(i + 1) * 4 + 0] + colors[(i + 2) * 4 + 0]) / 3.0f,
                            (colors[(i + 0) * 4 + 1] + colors[(i + 1) * 4 + 1] + colors[(i + 2) * 4 + 1]) / 3.0f,
                            (colors[(i + 0) * 4 + 2] + colors[(i + 1) * 4 + 2] + colors[(i + 2) * 4 + 2]) / 3.0f,
                            (colors[(i + 0) * 4 + 3] + colors[(i + 1) * 4 + 3] + colors[(i + 2) * 4 + 3]) / 3.0f
                        }
                    };
                    TriRun t1 = {
                        Vec2{ positions[(i + 3) * 2 + 0], positions[(i + 3) * 2 + 1] },
                        Vec2{ positions[(i + 4) * 2 + 0], positions[(i + 4) * 2 + 1] },
                        Vec2{ positions[(i + 5) * 2 + 0], positions[(i + 5) * 2 + 1] },
                        Color{
                            (colors[(i + 3) * 4 + 0] + colors[(i + 4) * 4 + 0] + colors[(i + 5) * 4 + 0]) / 3.0f,
                            (colors[(i + 3) * 4 + 1] + colors[(i + 4) * 4 + 1] + colors[(i + 5) * 4 + 1]) / 3.0f,
                            (colors[(i + 3) * 4 + 2] + colors[(i + 4) * 4 + 2] + colors[(i + 5) * 4 + 2]) / 3.0f,
                            (colors[(i + 3) * 4 + 3] + colors[(i + 4) * 4 + 3] + colors[(i + 5) * 4 + 3]) / 3.0f
                        }
                    };
                    out.tris.push_back(t0);
                    out.tris.push_back(t1);
                }
            });
        }

        for (auto& w : workers) w.join();
        for (const auto& out : local)
        {
            rects.insert(rects.end(), out.rects.begin(), out.rects.end());
            tris.insert(tris.end(), out.tris.begin(), out.tris.end());
        }
    }
    else
    {
        int i = 0;
        while (i + 2 < vertexCount)
        {
            if (i + 5 < vertexCount)
            {
                const float* c0 = &colors[(i + 0) * 4];
                const float* c1 = &colors[(i + 1) * 4];
                const float* c2 = &colors[(i + 2) * 4];
                const float* c3 = &colors[(i + 3) * 4];
                const float* c4 = &colors[(i + 4) * 4];
                const float* c5 = &colors[(i + 5) * 4];

                if (colorEq(c0, c1) && colorEq(c0, c2) && colorEq(c0, c3) && colorEq(c0, c4) && colorEq(c0, c5))
                {
                    float x0 = positions[(i + 0) * 2 + 0];
                    float y0 = positions[(i + 0) * 2 + 1];
                    float x1 = positions[(i + 1) * 2 + 0];
                    float y1 = positions[(i + 1) * 2 + 1];
                    float x2 = positions[(i + 2) * 2 + 0];
                    float y2 = positions[(i + 2) * 2 + 1];
                    float x4 = positions[(i + 4) * 2 + 0];
                    float y4 = positions[(i + 4) * 2 + 1];
                    float x5 = positions[(i + 5) * 2 + 0];
                    float y5 = positions[(i + 5) * 2 + 1];

                    const float eps = 0.0001f;
                    bool quadPattern =
                        fabsf(y0 - y1) < eps &&
                        fabsf(x1 - x2) < eps &&
                        fabsf(y2 - y4) < eps &&
                        fabsf(x4 - x5) < eps;

                    if (quadPattern)
                    {
                        float minX = (x0 < x1) ? x0 : x1;
                        float maxX = (x0 > x1) ? x0 : x1;
                        float minY = (y0 < y2) ? y0 : y2;
                        float maxY = (y0 > y2) ? y0 : y2;
                        rects.push_back(RectRun{
                            Rect{ minX, minY, maxX - minX, maxY - minY },
                            Color{ c0[0], c0[1], c0[2], c0[3] }
                        });
                        i += 6;
                        continue;
                    }
                }
            }

            Vec2 a = { positions[(i + 0) * 2 + 0], positions[(i + 0) * 2 + 1] };
            Vec2 b = { positions[(i + 1) * 2 + 0], positions[(i + 1) * 2 + 1] };
            Vec2 c = { positions[(i + 2) * 2 + 0], positions[(i + 2) * 2 + 1] };
            Color avg = {
                (colors[(i + 0) * 4 + 0] + colors[(i + 1) * 4 + 0] + colors[(i + 2) * 4 + 0]) / 3.0f,
                (colors[(i + 0) * 4 + 1] + colors[(i + 1) * 4 + 1] + colors[(i + 2) * 4 + 1]) / 3.0f,
                (colors[(i + 0) * 4 + 2] + colors[(i + 1) * 4 + 2] + colors[(i + 2) * 4 + 2]) / 3.0f,
                (colors[(i + 0) * 4 + 3] + colors[(i + 1) * 4 + 3] + colors[(i + 2) * 4 + 3]) / 3.0f
            };
            tris.push_back(TriRun{a, b, c, avg});
            i += 3;
        }
    }

    if (!rects.empty())
    {
        auto sameColor = [](const Color& a, const Color& b) -> bool
        {
            const float eps = 0.0001f;
            return fabsf(a.r - b.r) < eps && fabsf(a.g - b.g) < eps &&
                   fabsf(a.b - b.b) < eps && fabsf(a.a - b.a) < eps;
        };

        std::vector<RectRun> merged;
        merged.reserve(rects.size());
        for (const auto& rr : rects)
        {
            bool didMerge = false;
            if (!merged.empty())
            {
                RectRun& back = merged.back();
                const float eps = 0.0001f;
                bool sameRow = fabsf(back.r.y - rr.r.y) < eps && fabsf(back.r.height - rr.r.height) < eps;
                bool adjacent = fabsf((back.r.x + back.r.width) - rr.r.x) < eps;
                if (sameRow && adjacent && sameColor(back.c, rr.c))
                {
                    back.r.width += rr.r.width;
                    didMerge = true;
                }
            }
            if (!didMerge)
            {
                merged.push_back(rr);
            }
        }

        for (const auto& rr : merged)
        {
            D2DRenderer_DrawRectangle(ctx, rr.r, rr.c);
        }
    }

    for (const auto& t : tris)
    {
        D2DRenderer_DrawTriangle(ctx, t.a, t.b, t.c, t.col, 1);
    }
}

static void DrawTextPass(D2DRenderer* ctx, const std::wstring& text, float x, float y, float fontSize, Color color)
{
    if (!ctx || !ctx->target || !ctx->brush || !ctx->writeFactory || text.empty())
    {
        return;
    }

    int sizeKey = (int)floorf(((fontSize > 8.0f) ? fontSize : 8.0f) + 0.5f);
    if (sizeKey < 1) sizeKey = 1;
    if (sizeKey > 127) sizeKey = 127;
    IDWriteTextFormat* format = ctx->textFormats[sizeKey];
    if (!format)
    {
        HRESULT hr = ctx->writeFactory->CreateTextFormat(
            L"Segoe UI",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            (float)sizeKey,
            L"en-us",
            &format
        );
        if (FAILED(hr))
        {
            return;
        }
        ctx->textFormats[sizeKey] = format;
    }

    ctx->brush->SetColor(ToD2DColor(color));
    D2D1_RECT_F rect = D2D1::RectF(x, y, (float)ctx->width + 2048.0f, (float)ctx->height + 2048.0f);
    ctx->target->DrawText(text.c_str(), (UINT32)text.size(), format, rect, ctx->brush, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
}

void D2DRenderer_DrawText(D2DRenderer* ctx, const char* text, float x, float y, float fontSize, Color color, TextStyle style)
{
    if (!ctx || !text)
    {
        return;
    }

    std::wstring wide;
    if (!ToWide(text, wide))
    {
        return;
    }

    const float sizeScale = 1.03f;
    float drawSize = fontSize * sizeScale;
    float yBias = -0.08f * drawSize;
    float drawY = y + yBias;

    int outline_px = (int)((floorf(fontSize / 12.0f) > 1.0f) ? floorf(fontSize / 12.0f) : 1.0f);
    Color shadow = { 0.0f, 0.0f, 0.0f, 0.65f };
    Color outline = { 0.0f, 0.0f, 0.0f, 0.75f };

    if (style == TEXT_STYLE_SHADOW || style == TEXT_STYLE_OUTLINE_SHADOW)
    {
        DrawTextPass(ctx, wide, x + (float)outline_px, drawY + (float)outline_px, drawSize, shadow);
    }

    if (style == TEXT_STYLE_OUTLINE || style == TEXT_STYLE_OUTLINE_SHADOW)
    {
        for (int oy = -outline_px; oy <= outline_px; ++oy)
        {
            for (int ox = -outline_px; ox <= outline_px; ++ox)
            {
                if (ox == 0 && oy == 0)
                {
                    continue;
                }
                DrawTextPass(ctx, wide, x + (float)ox, drawY + (float)oy, drawSize, outline);
            }
        }
    }

    DrawTextPass(ctx, wide, x, drawY, drawSize, color);
}

void* D2DRenderer_LoadBitmap(D2DRenderer* ctx, const char* path, int* width, int* height)
{
    if (!ctx || !ctx->target || !ctx->wicFactory || !path)
    {
        return nullptr;
    }

    std::wstring widePath;
    if (!ToWide(path, widePath))
    {
        return nullptr;
    }

    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    ID2D1Bitmap* bitmap = nullptr;

    HRESULT hr = ctx->wicFactory->CreateDecoderFromFilename(
        widePath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );
    if (FAILED(hr))
    {
        return nullptr;
    }

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
        SafeRelease(&decoder);
        return nullptr;
    }

    hr = ctx->wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr))
    {
        SafeRelease(&frame);
        SafeRelease(&decoder);
        return nullptr;
    }

    hr = converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr))
    {
        SafeRelease(&converter);
        SafeRelease(&frame);
        SafeRelease(&decoder);
        return nullptr;
    }

    hr = ctx->target->CreateBitmapFromWicBitmap(converter, nullptr, &bitmap);
    if (FAILED(hr))
    {
        SafeRelease(&converter);
        SafeRelease(&frame);
        SafeRelease(&decoder);
        return nullptr;
    }

    if (width || height)
    {
        D2D1_SIZE_F s = bitmap->GetSize();
        if (width) *width = (int)(s.width + 0.5f);
        if (height) *height = (int)(s.height + 0.5f);
    }

    SafeRelease(&converter);
    SafeRelease(&frame);
    SafeRelease(&decoder);
    return bitmap;
}

void D2DRenderer_FreeBitmap(void* bitmap)
{
    ID2D1Bitmap* bmp = (ID2D1Bitmap*)bitmap;
    SafeRelease(&bmp);
}

void D2DRenderer_DrawBitmap(D2DRenderer* ctx, void* bitmap, Rect source, Rect dest, Vec2 origin, float rotation, Color tint)
{
    if (!ctx || !ctx->target || !bitmap)
    {
        return;
    }

    ID2D1Bitmap* bmp = (ID2D1Bitmap*)bitmap;
    D2D1_RECT_F src = D2D1::RectF(source.x, source.y, source.x + source.width, source.y + source.height);
    D2D1_RECT_F dst = D2D1::RectF(dest.x, dest.y, dest.x + dest.width, dest.y + dest.height);

    D2D1_MATRIX_3X2_F prev;
    ctx->target->GetTransform(&prev);
    if (rotation != 0.0f)
    {
        D2D1_POINT_2F center = D2D1::Point2F(dest.x + origin.x, dest.y + origin.y);
        D2D1_MATRIX_3X2_F rot = D2D1::Matrix3x2F::Rotation(rotation * 57.2957795f, center);
        ctx->target->SetTransform(rot * prev);
    }

    float opacity = (tint.a < 0.0f) ? 0.0f : ((tint.a > 1.0f) ? 1.0f : tint.a);
    ctx->target->DrawBitmap(bmp, dst, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
    ctx->target->SetTransform(prev);
}

#else

struct D2DRenderer {};

D2DRenderer* D2DRenderer_Create(SDL_Window*, int, int) { return nullptr; }
void D2DRenderer_Destroy(D2DRenderer*) {}
void D2DRenderer_Resize(D2DRenderer*, int, int) {}
int D2DRenderer_BeginFrame(D2DRenderer*) { return 0; }
void D2DRenderer_EndFrame(D2DRenderer*) {}
void D2DRenderer_Clear(D2DRenderer*, Color) {}
void D2DRenderer_DrawRectangle(D2DRenderer*, Rect, Color) {}
void D2DRenderer_DrawRectangleLines(D2DRenderer*, Rect, float, Color) {}
void D2DRenderer_DrawLine(D2DRenderer*, Vec2, Vec2, float, Color) {}
void D2DRenderer_DrawCircle(D2DRenderer*, Vec2, float, Color, int) {}
void D2DRenderer_DrawTriangle(D2DRenderer*, Vec2, Vec2, Vec2, Color, int) {}
void D2DRenderer_DrawTrianglesColored(D2DRenderer*, const float*, const float*, int) {}
void D2DRenderer_DrawText(D2DRenderer*, const char*, float, float, float, Color, TextStyle) {}
void* D2DRenderer_LoadBitmap(D2DRenderer*, const char*, int*, int*) { return nullptr; }
void D2DRenderer_FreeBitmap(void*) {}
void D2DRenderer_DrawBitmap(D2DRenderer*, void*, Rect, Rect, Vec2, float, Color) {}

#endif
