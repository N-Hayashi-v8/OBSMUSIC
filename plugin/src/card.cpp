// 「OBSMUSIC 曲名」ソース。曲が変わったときだけ Direct2D で描いて、テクスチャとして OBS に渡す。
#include <string>
#include <vector>

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <winrt/base.h>

#include <obs-module.h>

#include "nowplaying.h"

static const UINT W = 720, H = 112;

static std::wstring wide(const std::string &s)
{
	if (s.empty())
		return {};
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

// 背景と文字を描いて、32bit 乗算済みアルファの BGRA を返す。再生中でなければ全面透明。
static std::vector<uint8_t> render(const NowPlaying &n)
{
	using winrt::com_ptr;
	std::vector<uint8_t> out(W * H * 4, 0);
	CoInitializeEx(nullptr, COINIT_MULTITHREADED); // すでに別の形式で初期化済みなら失敗するが、そのまま使える

	com_ptr<ID2D1Factory> d2d;
	com_ptr<IDWriteFactory> dw;
	com_ptr<IWICImagingFactory> wic;
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.put())) ||
	    FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
				       reinterpret_cast<IUnknown **>(dw.put()))) ||
	    FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wic.put()))))
		return out;

	com_ptr<IWICBitmap> bmp;
	com_ptr<ID2D1RenderTarget> rt;
	if (FAILED(wic->CreateBitmap(W, H, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bmp.put())) ||
	    FAILED(d2d->CreateWicBitmapRenderTarget(bmp.get(), D2D1::RenderTargetProperties(), rt.put())))
		return out;

	rt->BeginDraw();
	rt->Clear(D2D1::ColorF(0, 0.0f));
	if (n.playing) {
		rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
		com_ptr<ID2D1SolidColorBrush> bg, fg;
		rt->CreateSolidColorBrush(D2D1::ColorF(0, 0.5f), bg.put());
		rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), fg.put());
		rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, (float)W, (float)H), 12, 12), bg.get());

		auto fmt = [&](float size, DWRITE_FONT_WEIGHT weight) {
			com_ptr<IDWriteTextFormat> f;
			dw->CreateTextFormat(L"Yu Gothic UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
					     DWRITE_FONT_STRETCH_NORMAL, size, L"ja-jp", f.put());
			f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
			DWRITE_TRIMMING trim = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
			com_ptr<IDWriteInlineObject> ellipsis;
			dw->CreateEllipsisTrimmingSign(f.get(), ellipsis.put());
			f->SetTrimming(&trim, ellipsis.get());
			return f;
		};
		auto title = wide(n.title), artist = wide(n.artist);
		rt->DrawText(title.c_str(), (UINT32)title.size(), fmt(28, DWRITE_FONT_WEIGHT_BOLD).get(),
			     D2D1::RectF(20, 16, W - 20, 56), fg.get());
		rt->DrawText(artist.c_str(), (UINT32)artist.size(), fmt(20, DWRITE_FONT_WEIGHT_NORMAL).get(),
			     D2D1::RectF(20, 60, W - 20, 96), fg.get());
	}
	rt->EndDraw();

	com_ptr<IWICBitmapLock> lock;
	WICRect all = {0, 0, (INT)W, (INT)H};
	if (SUCCEEDED(bmp->Lock(&all, WICBitmapLockRead, lock.put()))) {
		UINT stride = 0, size = 0;
		BYTE *p = nullptr;
		lock->GetStride(&stride);
		lock->GetDataPointer(&size, &p);
		for (UINT y = 0; y < H; y++)
			memcpy(&out[y * W * 4], p + y * stride, W * 4);
	}
	return out;
}

struct Card {
	gs_texture_t *tex = nullptr;
	uint64_t seen = (uint64_t)-1;
};

static const char *card_name(void *) { return "OBSMUSIC 曲名"; }
static void *card_create(obs_data_t *, obs_source_t *) { return new Card; }
static void card_destroy(void *data)
{
	auto *c = static_cast<Card *>(data);
	obs_enter_graphics();
	if (c->tex)
		gs_texture_destroy(c->tex);
	obs_leave_graphics();
	delete c;
}
static uint32_t card_width(void *) { return W; }
static uint32_t card_height(void *) { return H; }

static void card_tick(void *data, float)
{
	auto *c = static_cast<Card *>(data);
	auto n = nowplaying_get();
	if (n.version == c->seen)
		return;
	c->seen = n.version;
	auto px = render(n);
	obs_enter_graphics();
	if (!c->tex) {
		const uint8_t *p = px.data();
		c->tex = gs_texture_create(W, H, GS_BGRA, 1, &p, GS_DYNAMIC);
	} else {
		gs_texture_set_image(c->tex, px.data(), W * 4, false);
	}
	obs_leave_graphics();
}

static void card_render(void *data, gs_effect_t *effect)
{
	auto *c = static_cast<Card *>(data);
	if (!c->tex)
		return;
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), c->tex);
	gs_draw_sprite(c->tex, 0, W, H);
	gs_blend_state_pop();
}

void card_register()
{
	obs_source_info info = {};
	info.id = "obsmusic_card";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = card_name;
	info.create = card_create;
	info.destroy = card_destroy;
	info.get_width = card_width;
	info.get_height = card_height;
	info.video_tick = card_tick;
	info.video_render = card_render;
	obs_register_source(&info);
}
