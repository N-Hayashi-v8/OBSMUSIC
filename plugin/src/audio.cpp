// 「ツール」メニューの音声まわり。
// 音声の診断: 読むだけ。OBS 全体のモニタリングデバイスと、モニタリングしている入力を並べ、音が二重になる設定なら知らせる。
// モニタリングデバイスの設定: 一覧から選んで設定・保存する。
#include <string>

#include <windows.h>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include "nowplaying.h"

static std::wstring wide(const std::string &s)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
	return w;
}

struct Report {
	std::string inputs; // 1行ずつ「  入力名: 設定」
	int monitored = 0;
};

static bool add_input(void *data, obs_source_t *src)
{
	auto *r = static_cast<Report *>(data);
	if (!(obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO))
		return true;
	const char *mode = "オフ";
	switch (obs_source_get_monitoring_type(src)) {
	case OBS_MONITORING_TYPE_MONITOR_ONLY:
		mode = "モニターのみ";
		break;
	case OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT:
		mode = "モニターと出力";
		break;
	default:
		break;
	}
	if (obs_source_get_monitoring_type(src) != OBS_MONITORING_TYPE_NONE)
		r->monitored++;
	r->inputs += std::string("  ") + obs_source_get_name(src) + ": " + mode + "\n";
	return true;
}

static void diagnose(void *)
{
	const char *name = nullptr, *id = nullptr;
	obs_get_audio_monitoring_device(&name, &id);
	Report r;
	obs_enum_sources(add_input, &r);

	std::string msg = std::string("モニタリングデバイス: ") + (name ? name : "") + "\n\n入力ごとのモニタリング:\n" +
			  (r.inputs.empty() ? "  （音声のある入力がありません）\n" : r.inputs);
	bool doubled = r.monitored > 0 && id && strcmp(id, "default") == 0;
	if (doubled)
		msg += "\n注意: モニタリングデバイスが既定のデバイスです。モニタリングしている入力の音が、"
		       "元の音と重なって二重に聞こえます。";
	MessageBoxW((HWND)obs_frontend_get_main_window_handle(), wide(msg).c_str(), L"OBSMUSIC 音声の診断",
		    MB_OK | (doubled ? MB_ICONWARNING : MB_ICONINFORMATION));
}

// 「ツール → OBSMUSIC モニタリングデバイスの設定」。一覧から選んだデバイスを OBS 全体のモニタリングデバイスにする。
// OBS の設定画面と同じく、すぐ反映し、プロファイルの設定（Audio の MonitoringDeviceName / MonitoringDeviceId）に保存する。
static void choose(void *)
{
	QDialog dlg(static_cast<QWidget *>(obs_frontend_get_main_window()));
	dlg.setWindowTitle("OBSMUSIC モニタリングデバイスの設定");
	auto *lay = new QVBoxLayout(&dlg);
	lay->addWidget(new QLabel("モニタリングの音を流すデバイスを選んでください。\n"
				  "音が二重に聞こえないように、音の出ないデバイス（使っていないライン出力や仮想デバイスなど）を選びます。"));
	auto *box = new QComboBox;
	box->addItem("既定（Windows の既定の再生デバイス。音が二重に聞こえます）", "default");
	obs_enum_audio_monitoring_devices(
		[](void *d, const char *name, const char *id) {
			static_cast<QComboBox *>(d)->addItem(QString::fromUtf8(name), QString::fromUtf8(id));
			return true;
		},
		box);
	const char *cur_name = nullptr, *cur_id = nullptr;
	obs_get_audio_monitoring_device(&cur_name, &cur_id);
	int cur = box->findData(QString::fromUtf8(cur_id ? cur_id : ""));
	if (cur >= 0)
		box->setCurrentIndex(cur);
	lay->addWidget(box);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	lay->addWidget(buttons);
	if (dlg.exec() != QDialog::Accepted || box->currentIndex() == cur)
		return;

	std::string name = box->currentText().toStdString(), id = box->currentData().toString().toStdString();
	obs_set_audio_monitoring_device(name.c_str(), id.c_str());
	config_t *cfg = obs_frontend_get_profile_config();
	config_set_string(cfg, "Audio", "MonitoringDeviceName", name.c_str());
	config_set_string(cfg, "Audio", "MonitoringDeviceId", id.c_str());
	config_save_safe(cfg, "tmp", nullptr);
	blog(LOG_INFO, "[obsmusic] monitoring device: %s (%s)", name.c_str(), id.c_str());
}

void audio_register()
{
	obs_frontend_add_tools_menu_item("OBSMUSIC 音声の診断", diagnose, nullptr);
	obs_frontend_add_tools_menu_item("OBSMUSIC モニタリングデバイスの設定", choose, nullptr);
}
