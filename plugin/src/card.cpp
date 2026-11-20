// 「OBSMUSIC 曲名」ソース。曲かプロパティが変わったときだけ Direct2D で描いて、テクスチャとして OBS に渡す。
// 描くのは別スレッド（std::async）で、OBS の描画スレッドは描き上がった絵を受け取るだけにする。
#include <atomic>
#include <future>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <winrt/base.h>

#include <obs-module.h>

#include "nowplaying.h"

// プロパティで変えられる見た目。寸法は試作の index.html（曲名 28px のとき、余白 12、ジャケット 96、
// ジャケットと文字の間 16、高さ 120）を基準に、曲名の文字の大きさに比例させる。
struct Style {
	UINT width = 720;
	std::wstring face = L"Yu Gothic UI";
	float size = 28;
	bool bold = true, italic = false;
	uint32_t text = 0xFFFFFFFF; // OBS の色は 0xAABBGGRR
	uint32_t back = 0x80000000;
	UINT height() const { return (UINT)(120 * size / 28 + 0.5f); }
};

struct Pixels {
	UINT w = 0, h = 0;
	std::vector<uint8_t> data;
};

static std::wstring wide(const std::string &s)
{
	if (s.empty())
		return {};
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

static D2D1_COLOR_F color(uint32_t c)
{
	return D2D1::ColorF((c & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, ((c >> 16) & 0xFF) / 255.0f,
			    (c >> 24) / 255.0f);
}

// ジャケットを、正方形に中央で切り抜き、角を丸めて dst に描く（CSS の object-fit: cover と同じ）。画像がなければ何もしない。
static void draw_art(ID2D1Factory *d2d, IWICImagingFactory *wic, ID2D1RenderTarget *rt, const std::vector<uint8_t> &data,
		     D2D1_RECT_F dst, float radius)
{
	using winrt::com_ptr;
	if (data.empty())
		return;
	com_ptr<IWICStream> ws;
	com_ptr<IWICBitmapDecoder> dec;
	com_ptr<IWICBitmapFrameDecode> frame;
	com_ptr<IWICFormatConverter> cv;
	com_ptr<ID2D1Bitmap> img;
	if (FAILED(wic->CreateStream(ws.put())) ||
	    FAILED(ws->InitializeFromMemory(const_cast<BYTE *>(data.data()), (DWORD)data.size())) ||
	    FAILED(wic->CreateDecoderFromStream(ws.get(), nullptr, WICDecodeMetadataCacheOnLoad, dec.put())) ||
	    FAILED(dec->GetFrame(0, frame.put())) || FAILED(wic->CreateFormatConverter(cv.put())) ||
	    FAILED(cv->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0,
				  WICBitmapPaletteTypeCustom)) ||
	    FAILED(rt->CreateBitmapFromWicBitmap(cv.get(), img.put())))
		return;

	auto sz = img->GetSize();
	float side = sz.width < sz.height ? sz.width : sz.height;
	auto src = D2D1::RectF((sz.width - side) / 2, (sz.height - side) / 2, (sz.width + side) / 2,
			       (sz.height + side) / 2);

	com_ptr<ID2D1RoundedRectangleGeometry> clip;
	com_ptr<ID2D1Layer> layer;
	d2d->CreateRoundedRectangleGeometry(D2D1::RoundedRect(dst, radius, radius), clip.put());
	rt->CreateLayer(layer.put());
	rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), clip.get()), layer.get());
	rt->DrawBitmap(img.get(), dst, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &src);
	rt->PopLayer();
}

// 背景、ジャケット、文字を描いて、32bit 乗算済みアルファの BGRA を返す。再生中でなければ全面透明。
static Pixels draw(const NowPlaying &n, const Style &st)
{
	using winrt::com_ptr;
	const UINT W = st.width, H = st.height();
	const float k = st.size / 28, PAD = 12 * k, ART = 96 * k, TX = PAD + ART + 16 * k;
	Pixels out{W, H, std::vector<uint8_t>(W * H * 4, 0)};
	CoInitializeEx(nullptr, COINIT_MULTITHREADED); // 同じスレッドで2回目以降は S_FALSE が返るだけ

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
		rt->CreateSolidColorBrush(color(st.back), bg.put());
		rt->CreateSolidColorBrush(color(st.text), fg.put());
		rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, (float)W, (float)H), 12 * k, 12 * k),
					 bg.get());

		auto fmt = [&](float size, DWRITE_FONT_WEIGHT weight) {
			com_ptr<IDWriteTextFormat> f;
			if (FAILED(dw->CreateTextFormat(st.face.c_str(), nullptr, weight,
							st.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
							DWRITE_FONT_STRETCH_NORMAL, size, L"ja-jp", f.put())))
				return f;
			f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
			DWRITE_TRIMMING trim = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
			com_ptr<IDWriteInlineObject> ellipsis;
			dw->CreateEllipsisTrimmingSign(f.get(), ellipsis.put());
			f->SetTrimming(&trim, ellipsis.get());
			return f;
		};
		auto tf = fmt(st.size, st.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
		auto af = fmt(st.size * 20 / 28, DWRITE_FONT_WEIGHT_NORMAL);
		auto title = wide(n.title), artist = wide(n.artist);
		if (tf)
			rt->DrawText(title.c_str(), (UINT32)title.size(), tf.get(), D2D1::RectF(TX, 20 * k, W - PAD, 60 * k),
				     fg.get());
		if (af)
			rt->DrawText(artist.c_str(), (UINT32)artist.size(), af.get(),
				     D2D1::RectF(TX, 64 * k, W - PAD, 100 * k), fg.get());
		draw_art(d2d.get(), wic.get(), rt.get(), n.thumb, D2D1::RectF(PAD, PAD, PAD + ART, PAD + ART), 8 * k);
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
			memcpy(&out.data[y * W * 4], p + y * stride, W * 4);
	}
	return out;
}

struct Card {
	std::mutex mu;
	Style style;                     // mu で守る（update は UI のスレッド、tick は描画スレッドから呼ばれる）
	std::atomic<uint64_t> style_ver{1};
	uint64_t seen_song = (uint64_t)-1, seen_style = 0;
	std::future<Pixels> job;
	gs_texture_t *tex = nullptr;
	std::atomic<UINT> w{720}, h{120};
};

static const char *card_name(void *) { return "OBSMUSIC 曲名"; }

static void card_defaults(obs_data_t *s)
{
	Style d;
	obs_data_t *font = obs_data_create();
	obs_data_set_default_string(font, "face", "Yu Gothic UI");
	obs_data_set_default_int(font, "size", (int)d.size);
	obs_data_set_default_int(font, "flags", OBS_FONT_BOLD);
	obs_data_set_default_obj(s, "font", font);
	obs_data_release(font);
	obs_data_set_default_int(s, "width", d.width);
	obs_data_set_default_int(s, "text_color", d.text);
	obs_data_set_default_int(s, "back_color", d.back);
}

static obs_properties_t *card_properties(void *)
{
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_font(p, "font", "フォント（大きさは曲名の文字。全体の高さもこれに合わせて変わる）");
	obs_properties_add_int(p, "width", "幅", 200, 3840, 10);
	obs_properties_add_color_alpha(p, "text_color", "文字の色");
	obs_properties_add_color_alpha(p, "back_color", "背景の色");
	return p;
}

static void card_update(void *data, obs_data_t *s)
{
	auto *c = static_cast<Card *>(data);
	Style st;
	obs_data_t *font = obs_data_get_obj(s, "font");
	st.face = wide(obs_data_get_string(font, "face"));
	st.size = (float)obs_data_get_int(font, "size");
	auto flags = obs_data_get_int(font, "flags");
	obs_data_release(font);
	st.bold = flags & OBS_FONT_BOLD;
	st.italic = flags & OBS_FONT_ITALIC;
	if (st.size < 8)
		st.size = 8;
	st.width = (UINT)obs_data_get_int(s, "width");
	st.text = (uint32_t)obs_data_get_int(s, "text_color");
	st.back = (uint32_t)obs_data_get_int(s, "back_color");
	std::lock_guard<std::mutex> g(c->mu);
	c->style = st;
	c->style_ver++;
}

static void *card_create(obs_data_t *s, obs_source_t *)
{
	auto *c = new Card;
	card_update(c, s);
	return c;
}

static void card_destroy(void *data)
{
	auto *c = static_cast<Card *>(data);
	if (c->job.valid())
		c->job.wait();
	obs_enter_graphics();
	if (c->tex)
		gs_texture_destroy(c->tex);
	obs_leave_graphics();
	delete c;
}

static uint32_t card_width(void *data) { return static_cast<Card *>(data)->w; }
static uint32_t card_height(void *data) { return static_cast<Card *>(data)->h; }

static void card_tick(void *data, float)
{
	auto *c = static_cast<Card *>(data);

	// 描き上がっていれば、テクスチャに入れる。大きさが変わったら作り直す。
	if (c->job.valid()) {
		if (c->job.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return;
		Pixels px = c->job.get();
		const uint8_t *p = px.data.data();
		obs_enter_graphics();
		if (c->tex && (gs_texture_get_width(c->tex) != px.w || gs_texture_get_height(c->tex) != px.h)) {
			gs_texture_destroy(c->tex);
			c->tex = nullptr;
		}
		if (!c->tex)
			c->tex = gs_texture_create(px.w, px.h, GS_BGRA, 1, &p, GS_DYNAMIC);
		else
			gs_texture_set_image(c->tex, p, px.w * 4, false);
		obs_leave_graphics();
		c->w = px.w;
		c->h = px.h;
	}

	// 曲かプロパティが変わっていれば、描き直しを頼む。
	auto n = nowplaying_get();
	uint64_t sv = c->style_ver;
	if (n.version == c->seen_song && sv == c->seen_style)
		return;
	c->seen_song = n.version;
	c->seen_style = sv;
	Style st;
	{
		std::lock_guard<std::mutex> g(c->mu);
		st = c->style;
	}
	c->job = std::async(std::launch::async, [n, st] { return draw(n, st); });
}

static void card_render(void *data, gs_effect_t *effect)
{
	auto *c = static_cast<Card *>(data);
	if (!c->tex)
		return;
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), c->tex);
	gs_draw_sprite(c->tex, 0, c->w, c->h);
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
	info.get_defaults = card_defaults;
	info.get_properties = card_properties;
	info.update = card_update;
	info.video_tick = card_tick;
	info.video_render = card_render;
	obs_register_source(&info);
}
