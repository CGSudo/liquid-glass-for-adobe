// Liquid Glass for Adobe: one-click Windows installer.
// The window is a Windows 11 acrylic (glass) window showing setup.html in WebView2: a live tour of the looks and one
// Install button. This file does the installing:
//   Photoshop:   Adobe's plugin installer (UPIA, part of Creative Cloud) installs the .ccx for the current user.
//   Illustrator and After Effects: native files go into each app's own Plug-ins folder via --copy-native <payload>.
//                Program Files needs admin rights, so Windows asks once.
// Without the WebView2 runtime it asks with a message box and installs the same way. --silent installs with no window.
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "InstallPaths.h"
#include "WebView2.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

#ifndef LG_VERSION
#define LG_VERSION L"0.0.0"
#endif
static const wchar_t* kName = L"Liquid Glass for Adobe";
static const wchar_t* kPluginId = L"com.liquidglass.photoshop"; // UXP id: kept from the first release so upgrades replace it
static const UINT WM_PAGE = WM_APP + 1;                         // lParam: new std::wstring JSON for the page

static std::wstring gDir; // extracted payload: setup.html, scripts, engine, plug-ins
static HWND gWnd;
static ComPtr<ICoreWebView2Controller> gController;
static ComPtr<ICoreWebView2> gView;
static std::atomic<bool> gBusy{ false };

static std::wstring Env(const wchar_t* name)
{
	wchar_t buf[MAX_PATH];
	DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH);
	return n && n < MAX_PATH ? buf : L"";
}

// Folders matching "<Program Files>\Adobe\<prefix>*", e.g. every installed Illustrator.
static std::vector<std::wstring> AppFolders(const wchar_t* prefix)
{
	std::vector<std::wstring> out;
	std::wstring base = Env(L"ProgramFiles") + L"\\Adobe\\";
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW((base + prefix + L"*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return out;
	do
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) out.push_back(base + fd.cFileName);
	while (FindNextFileW(h, &fd));
	FindClose(h);
	return out;
}

static bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

static std::vector<std::wstring> AEFolders()
{
	std::vector<std::wstring> out;
	auto add = [&](std::wstring path) {
		while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
		const std::wstring support = L"\\Support Files";
		if (path.size() >= support.size() && !_wcsicmp(path.c_str() + path.size() - support.size(), support.c_str()))
			path.resize(path.size() - support.size());
		if (!Exists(path + L"\\Support Files\\AfterFX.exe")) return;
		for (const auto& old : out) if (!_wcsicmp(old.c_str(), path.c_str())) return;
		out.push_back(path);
	};
	for (const auto& path : AppFolders(L"Adobe After Effects "))
		if (setup::SupportedAE(path.substr(path.find_last_of(L'\\') + 1 + wcslen(L"Adobe After Effects ")))) add(path);
	for (HKEY hive : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER }) {
		HKEY key;
		if (RegOpenKeyExW(hive, L"SOFTWARE\\Adobe\\After Effects", 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) continue;
		wchar_t version[256];
		for (DWORD i = 0, n = 256; RegEnumKeyExW(key, i, version, &n, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS; ++i, n = 256) {
			if (!setup::SupportedAE(version)) continue;
			HKEY sub;
			if (RegOpenKeyExW(key, version, 0, KEY_READ, &sub) != ERROR_SUCCESS) continue;
			wchar_t value[32768]; DWORD bytes = sizeof value;
			if (RegGetValueW(sub, nullptr, L"InstallPath", RRF_RT_REG_SZ, nullptr, value, &bytes) == ERROR_SUCCESS) add(value);
			RegCloseKey(sub);
		}
		RegCloseKey(key);
	}
	return out;
}

static bool Running(const wchar_t* exe)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32W pe{ sizeof pe };
	bool found = false;
	for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) found = !_wcsicmp(pe.szExeFile, exe);
	CloseHandle(snap);
	return found;
}

static std::wstring Upia()
{
	std::wstring p = Env(L"CommonProgramFiles") +
		L"\\Adobe\\Adobe Desktop Common\\RemoteComponents\\UPI\\UnifiedPluginInstallerAgent\\UnifiedPluginInstallerAgent.exe";
	return GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES ? L"" : p;
}

static DWORD Run(const std::wstring& exe, const std::wstring& args)
{
	std::wstring cmd = L"\"" + exe + L"\" " + args;
	STARTUPINFOW si{ sizeof si };
	PROCESS_INFORMATION pi{};
	if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return DWORD(-1);
	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD code = 1;
	GetExitCodeProcess(pi.hProcess, &code);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return code;
}

static std::string ReadAll(const std::wstring& path)
{
	std::string s;
	HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (f == INVALID_HANDLE_VALUE) return s;
	char buf[65536];
	for (DWORD n; ReadFile(f, buf, sizeof buf, &n, nullptr) && n;) s.append(buf, n);
	CloseHandle(f);
	return s;
}

static std::wstring Parent(const std::wstring& path) { return path.substr(0, path.find_last_of(L'\\')); }

static bool AppsOpen()
{
	return Running(L"Photoshop.exe") || Running(L"Illustrator.exe") || Running(L"AfterFX.exe") || Running(L"aerender.exe") ||
		(Exists(Env(L"ProgramFiles") + L"\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\CGSudo\\LiquidGlass.aex") && Running(L"Adobe Premiere Pro.exe"));
}

// Elevated part stages every file before moving old binaries aside; a failed replacement restores the originals.
static int CopyNative(const std::wstring& payload)
{
	if (AppsOpen()) return 10;
	const auto plan = setup::NativePlan(AppFolders(L"Adobe Illustrator "), AEFolders(), Env(L"ProgramFiles"));
	const std::wstring suffix = L".lg-setup-" + std::to_wstring(GetCurrentProcessId());
	std::vector<std::wstring> targets = plan.obsolete, staged, installed, saved;
	for (const auto& c : plan.copies) targets.push_back(c.destination);
	// Refuse redirected folders so cleanup cannot escape the detected app's own plug-in tree.
	for (const auto& path : targets) {
		for (auto dir = Parent(path); dir.size() > 3; dir = Parent(dir)) {
			DWORD attrs = GetFileAttributesW(dir.c_str());
			if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return 11;
		}
		DWORD attrs = GetFileAttributesW(path.c_str());
		if (attrs == INVALID_FILE_ATTRIBUTES) {
			if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) return 11;
			continue;
		}
		if (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY)) return 11;
		HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (f == INVALID_HANDLE_VALUE) return 10;
		CloseHandle(f);
	}
	bool ok = true;
	for (const auto& c : plan.copies) {
		const auto dir = Parent(c.destination), stage = c.destination + suffix + L".new";
		int made = SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
		if (made != ERROR_SUCCESS && made != ERROR_ALREADY_EXISTS) { ok = false; break; }
		staged.push_back(stage);
		if (!CopyFileW((payload + L"\\" + c.file).c_str(), stage.c_str(), TRUE)) { ok = false; break; }
		const auto source = ReadAll(payload + L"\\" + c.file);
		if (source.empty() || ReadAll(stage) != source) { ok = false; break; }
	}
	if (ok && AppsOpen()) ok = false;
	if (ok) for (const auto& path : targets) {
		if (!Exists(path)) continue;
		if (!MoveFileExW(path.c_str(), (path + suffix).c_str(), MOVEFILE_WRITE_THROUGH)) { ok = false; break; }
		saved.push_back(path);
	}
	if (ok) for (const auto& c : plan.copies) {
		if (!MoveFileExW((c.destination + suffix + L".new").c_str(), c.destination.c_str(), MOVEFILE_WRITE_THROUGH)) { ok = false; break; }
		installed.push_back(c.destination);
	}
	if (ok) for (const auto& c : plan.copies)
		if (ReadAll(c.destination) != ReadAll(payload + L"\\" + c.file)) { ok = false; break; }
	bool restored = true;
	if (!ok) {
		for (const auto& path : installed) if (!DeleteFileW(path.c_str())) restored = false;
		for (auto i = saved.rbegin(); i != saved.rend(); ++i)
			if (!MoveFileExW((*i + suffix).c_str(), i->c_str(), MOVEFILE_WRITE_THROUGH)) restored = false;
	}
	for (const auto& path : staged) DeleteFileW(path.c_str());
	if (!ok) return restored ? 12 : 13;
	bool clean = true;
	for (const auto& path : saved) if (!DeleteFileW((path + suffix).c_str())) clean = false;
	for (const auto& path : plan.obsolete) {
		const auto dir = Parent(path);
		if (dir.substr(dir.find_last_of(L'\\') + 1) != L"Plug-ins") RemoveDirectoryW(dir.c_str()); // only succeeds when empty
	}
	return clean ? 0 : 14;
}

static std::wstring JsonStr(const std::wstring& s)
{
	std::wstring q = L"\"";
	for (wchar_t c : s) {
		if (c == L'"' || c == L'\\') q += L'\\';
		if (c >= 0x20) q += c;
	}
	return q + L"\"";
}

// What the page shows before installing: which apps are here, and whether one is still open.
static std::wstring StateJson()
{
	auto app = [](const wchar_t* prefix, const wchar_t* exe) {
		return std::wstring(L"{\"found\":") + (AppFolders(prefix).empty() ? L"false" : L"true") +
			L",\"running\":" + (Running(exe) ? L"true" : L"false") + L"}";
	};
	return L"{\"type\":\"state\",\"version\":\"" LG_VERSION L"\",\"cc\":" + std::wstring(Upia().empty() ? L"false" : L"true") +
		L",\"ps\":" + app(L"Adobe Photoshop ", L"Photoshop.exe") + L",\"ai\":" + app(L"Adobe Illustrator ", L"Illustrator.exe") +
		L",\"ae\":{\"found\":" + (AEFolders().empty() ? L"false" : L"true") + L",\"running\":" +
		((Running(L"AfterFX.exe") || Running(L"aerender.exe")) ? L"true" : L"false") + L"},\"blocked\":" + (AppsOpen() ? L"true" : L"false") + L"}";
}

// Sends one JSON message to the page (from any thread), or remembers it for the message-box fallback.
static std::vector<std::wstring>* gLog;
static void Tell(const std::wstring& json)
{
	if (gLog) gLog->push_back(json);
	else if (gWnd) PostMessageW(gWnd, WM_PAGE, 0, LPARAM(new std::wstring(json)));
}
static void Step(const wchar_t* app, const wchar_t* status, const std::wstring& text)
{
	Tell(L"{\"type\":\"step\",\"app\":\"" + std::wstring(app) + L"\",\"status\":\"" + status + L"\",\"text\":" + JsonStr(text) + L"}");
}

static bool PhotoshopCopiesRemain()
{
	const auto base = Env(L"APPDATA") + L"\\Adobe\\UXP\\Plugins\\External\\";
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW((base + kPluginId + L"_*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND;
	bool found = false;
	do { if (Exists(base + fd.cFileName + L"\\manifest.json")) found = true; } while (!found && FindNextFileW(h, &fd));
	FindClose(h);
	return found;
}

static void Install()
{
	const auto ais = AppFolders(L"Adobe Illustrator "), aes = AEFolders();
	const bool ps = !AppFolders(L"Adobe Photoshop ").empty();
	auto stop = [&](const std::wstring& why) {
		if (ps) Step(L"ps", L"fail", why);
		if (!ais.empty()) Step(L"ai", L"fail", why);
		if (!aes.empty()) Step(L"ae", L"fail", why);
		Tell(L"{\"type\":\"finished\"}");
	};
	if (AppsOpen()) return stop(L"Close Photoshop, Illustrator and After Effects, including background renders. Close Premiere Pro if an older shared plug-in is installed. Nothing was changed.");
	if (ps && Upia().empty()) return stop(L"Creative Cloud desktop wasn't found. Install it and run Setup again. Nothing was changed.");
	// Native preflight and replacement finish before UPIA removes anything from Photoshop.
	const auto plan = setup::NativePlan(ais, aes, Env(L"ProgramFiles"));
	bool native = !plan.copies.empty();
	for (const auto& path : plan.obsolete) native = native || Exists(path);
	if (native) {
		if (!ais.empty()) Step(L"ai", L"work", L"Checking files and waiting for Windows permission…");
		if (!aes.empty()) Step(L"ae", L"work", L"Checking files and waiting for Windows permission…");
		wchar_t self[MAX_PATH];
		GetModuleFileNameW(nullptr, self, MAX_PATH);
		const std::wstring args = L"--copy-native \"" + gDir + L"\"";
		SHELLEXECUTEINFOW sei{ sizeof sei };
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		sei.lpVerb = L"runas"; sei.lpFile = self; sei.lpParameters = args.c_str(); sei.hwnd = gWnd; sei.nShow = SW_HIDE;
		if (!ShellExecuteExW(&sei)) return stop(L"Windows permission was declined. Nothing was changed.");
		WaitForSingleObject(sei.hProcess, INFINITE);
		DWORD code = 1;
		GetExitCodeProcess(sei.hProcess, &code); CloseHandle(sei.hProcess);
		if (code) {
			if (code == 10 || code == 11) return stop(L"A plug-in is locked, read-only or in a redirected folder. Close Adobe apps and background renders, check folder permissions, then retry. Nothing was changed.");
			if (code == 12) return stop(L"Couldn't replace the plug-ins. Original binaries were restored; Photoshop was not changed. Close Adobe apps and retry.");
			if (code == 13) return stop(L"Replacement and rollback could not finish. Recovery files with .lg-setup- names remain beside the plug-ins. Close Adobe apps and restore these files before retrying. Photoshop was not changed.");
			if (code == 14) return stop(L"New native plug-ins were copied, but old backup files could not be removed. Close Adobe apps and retry; Photoshop was not changed.");
			return stop(L"The copy process failed. Close Adobe apps and run Setup again.");
		}
		auto verified = [&](const std::wstring& file) {
			const auto want = ReadAll(gDir + L"\\" + file);
			for (const auto& c : plan.copies) if (c.file == file && (want.empty() || ReadAll(c.destination) != want)) return false;
			return true;
		};
		if (!ais.empty()) {
			const bool ok = verified(L"LiquidGlass.aip");
			Step(L"ai", ok ? L"done" : L"fail", ok ? L"Installed. Effect › Stylize › Liquid Glass…" : L"File verification failed. Close Illustrator and retry.");
		}
		if (!aes.empty()) {
			bool ok = verified(L"LiquidGlass.aex") && verified(L"LiquidGlassBrightness.aex");
			Step(L"ae", ok ? L"done" : L"fail", ok ? L"Both effects installed. Effects › CGSudo." : L"File verification failed. Close After Effects and retry.");
		}
	}
	// Photoshop: per user, through Creative Cloud's installer. Old copies go first (also the pre-rename "Sudo Liquid Glass"),
	// because the installer silently keeps an installed plugin with the same version.
	if (ps) {
		Step(L"ps", L"work", L"Installing…");
		std::wstring u = Upia(), ccx = gDir + L"\\LiquidGlass-for-Adobe.ccx";
		Run(u, L"/remove \"Sudo Liquid Glass\"");
		Run(u, L"/remove \"Liquid Glass for Adobe\"");
		std::wstring dir = Env(L"APPDATA") + L"\\Adobe\\UXP\\Plugins\\External\\" + kPluginId + L"_" LG_VERSION;
		if (PhotoshopCopiesRemain()) {
			Step(L"ps", L"fail", L"Creative Cloud couldn't remove the previous copy. Close Photoshop and restart Creative Cloud, then retry.");
			Tell(L"{\"type\":\"finished\"}"); return;
		}
		DWORD code = Run(u, L"/install \"" + ccx + L"\"");
		if (code == 0 && Exists(dir + L"\\manifest.json") && ReadAll(dir + L"\\index.js") == ReadAll(gDir + L"\\photoshop-index.js"))
			Step(L"ps", L"done", L"Installed. In Photoshop: Plugins › Liquid Glass for Adobe.");
		else Step(L"ps", L"fail", L"Adobe's installer stopped with code " + std::to_wstring(int(code)) + L". Restart the Creative Cloud app and try again.");
	}
	Tell(L"{\"type\":\"finished\"}");
}

// Unpacks the payload into a fresh temp folder: RCDATA 101, 102, … in this order (setup\build.ps1 uses the same list).
static bool Extract()
{
	static const wchar_t* files[] = { L"setup.html", L"glass-worker.js", L"stage.js", L"looks.js", L"engine.js", L"glass_core.wasm", L"logo.png",
		L"LiquidGlass-for-Adobe.ccx", L"LiquidGlass.aip", L"LiquidGlass.aex", L"LiquidGlassBrightness.aex", L"photoshop-index.js" };
	const auto temp = Env(L"TEMP");
	if (temp.empty()) return false;
	gDir = temp + L"\\LiquidGlassSetup-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
	if (!CreateDirectoryW(gDir.c_str(), nullptr)) return false;
	int id = 101;
	for (auto name : files) {
		HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id++), RT_RCDATA);
		if (!r) return false;
		DWORD size = SizeofResource(nullptr, r);
		const void* data = LockResource(LoadResource(nullptr, r));
		HANDLE f = CreateFileW((gDir + L"\\" + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		DWORD n = 0;
		bool ok = f != INVALID_HANDLE_VALUE && WriteFile(f, data, size, &n, nullptr) && n == size;
		if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
		if (!ok) return false;
	}
	return true;
}

static void Cleanup()
{
	if (gDir.empty() || Parent(gDir) != Env(L"TEMP") || (GetFileAttributesW(gDir.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)) return;
	std::wstring from = gDir + L'\0'; // double-NUL terminated for SHFileOperation
	SHFILEOPSTRUCTW op{ nullptr, FO_DELETE, from.c_str(), nullptr, FOF_NO_UI };
	SHFileOperationW(&op);
}

static void OnMessage(const std::wstring& m)
{
	if (m == L"state") Tell(StateJson());
	else if (m == L"install" && !gBusy) {
		gBusy = true;
		std::thread([] { Install(); gBusy = false; }).detach();
	} else if (m == L"close") PostMessageW(gWnd, WM_CLOSE, 0, 0);
	else if (m.rfind(L"open:", 0) == 0) {
		std::wstring url = m.substr(5); // only our own pages
		if (url.rfind(L"https://github.com/CGSudo/", 0) == 0 || url.rfind(L"https://www.linkedin.com/in/cgsudo", 0) == 0)
			ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}
}

static void CreateView(const std::wstring& dataDir, HRESULT& failed)
{
	failed = CreateCoreWebView2EnvironmentWithOptions(nullptr, dataDir.c_str(), nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
			if (FAILED(hr) || !env) return PostMessageW(gWnd, WM_CLOSE, 1, 0), S_OK;
			return env->CreateCoreWebView2Controller(gWnd,
				Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([](HRESULT hr, ICoreWebView2Controller* c) -> HRESULT {
					if (FAILED(hr) || !c) return PostMessageW(gWnd, WM_CLOSE, 1, 0), S_OK;
					gController = c;
					c->get_CoreWebView2(&gView);
					ComPtr<ICoreWebView2Settings> s;
					gView->get_Settings(&s);
					s->put_AreDevToolsEnabled(FALSE);
					s->put_AreDefaultContextMenusEnabled(FALSE);
					s->put_IsStatusBarEnabled(FALSE);
					s->put_IsZoomControlEnabled(FALSE);
					ComPtr<ICoreWebView2Controller2> c2;
					if (SUCCEEDED(c->QueryInterface(IID_PPV_ARGS(&c2)))) c2->put_DefaultBackgroundColor({ 0, 0, 0, 0 }); // see-through: the acrylic shows
					RECT rc;
					GetClientRect(gWnd, &rc);
					c->put_Bounds(rc);
					gView->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
						[](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT {
							LPWSTR m = nullptr;
							if (SUCCEEDED(a->TryGetWebMessageAsString(&m)) && m) OnMessage(m);
							CoTaskMemFree(m);
							return S_OK;
						}).Get(), nullptr);
					gView->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
						[](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* a) -> HRESULT { return a->put_Handled(TRUE); }).Get(), nullptr);
					ComPtr<ICoreWebView2_3> v3;
					if (FAILED(gView.As(&v3))) return PostMessageW(gWnd, WM_CLOSE, 1, 0), S_OK;
					v3->SetVirtualHostNameToFolderMapping(L"setup.liquidglass", gDir.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
					gView->Navigate(L"https://setup.liquidglass/setup.html");
					return S_OK;
				}).Get());
		}).Get());
}

static bool gNoView; // WebView2 missing or failed: fall back to message boxes
static LRESULT CALLBACK Proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_SIZE:
		if (gController) {
			RECT rc;
			GetClientRect(w, &rc);
			gController->put_Bounds(rc);
		}
		return 0;
	case WM_PAGE: {
		std::wstring* json = (std::wstring*)lp;
		if (gView) gView->PostWebMessageAsJson(json->c_str());
		delete json;
		return 0;
	}
	case WM_CLOSE:
		if (gBusy) return 0; // never stop half way through an install
		if (wp == 1 && !gView) gNoView = true;
		DestroyWindow(w);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(w, msg, wp, lp);
}

static std::wstring Summary(const std::vector<std::wstring>& log)
{
	std::wstring s;
	for (auto& j : log) {
		size_t t = j.find(L"\"text\":\"");
		if (t == std::wstring::npos || j.find(L"\"status\":\"work\"") != std::wstring::npos) continue;
		s += (j.find(L"\"app\":\"ps\"") != std::wstring::npos ? L"Photoshop: " :
			j.find(L"\"app\":\"ae\"") != std::wstring::npos ? L"After Effects: " : L"Illustrator: ") + j.substr(t + 8, j.size() - t - 10) + L"\n\n";
	}
	return s;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc == 3 && !wcscmp(argv[1], L"--copy-native")) return CopyNative(argv[2]);

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (!Extract()) {
		MessageBoxW(nullptr, L"Setup is damaged. Download it again from GitHub.", kName, MB_ICONERROR);
		return 1;
	}
	// --silent: no window, for IT deployment and tests. Exit code 0 = nothing failed, 2 = something failed.
	if (argc == 2 && !wcscmp(argv[1], L"--silent")) {
		std::vector<std::wstring> log;
		gLog = &log;
		Install();
		Cleanup();
		for (auto& j : log)
			if (j.find(L"\"status\":\"fail\"") != std::wstring::npos) return 2;
		return 0;
	}

	WNDCLASSW wc{};
	wc.lpfnWndProc = Proc;
	wc.hInstance = inst;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
	wc.hbrBackground = HBRUSH(GetStockObject(BLACK_BRUSH)); // black = fully see-through where the frame is extended
	wc.lpszClassName = L"LiquidGlassForAdobeSetup";
	RegisterClassW(&wc);
	UINT dpi = GetDpiForSystem();
	int cw = MulDiv(1040, dpi, 96), ch = MulDiv(740, dpi, 96);
	RECT wa;
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
	if (cw > wa.right - wa.left - 24) cw = wa.right - wa.left - 24;
	if (ch > wa.bottom - wa.top - 24) ch = wa.bottom - wa.top - 24;
	gWnd = CreateWindowExW(0, wc.lpszClassName, (std::wstring(kName) + L" " LG_VERSION L" Setup").c_str(),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, wa.left + (wa.right - wa.left - cw) / 2,
		wa.top + (wa.bottom - wa.top - ch) / 2, cw, ch, nullptr, nullptr, inst, nullptr);
	// Windows 11 glass: dark title bar, acrylic backdrop behind the whole window (Windows 10 shows plain dark).
	BOOL dark = TRUE;
	DwmSetWindowAttribute(gWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
	int backdrop = 3; // DWMSBT_TRANSIENTWINDOW (acrylic)
	DwmSetWindowAttribute(gWnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &backdrop, sizeof backdrop);
	MARGINS m{ -1 };
	DwmExtendFrameIntoClientArea(gWnd, &m);

	HRESULT failed = S_OK;
	CreateView(Env(L"TEMP") + L"\\LiquidGlassSetup-WebView2", failed);
	if (SUCCEEDED(failed)) {
		ShowWindow(gWnd, show);
		MSG msg;
		while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	} else {
		DestroyWindow(gWnd);
		gNoView = true;
	}
	if (gController) gController->Close();
	gView.Reset();
	gController.Reset();
	gWnd = nullptr;

	if (gNoView) {
		// No WebView2 runtime (rare: it ships with Windows 11 and current Windows 10): plain dialogs, same install.
		if (MessageBoxW(nullptr, L"Install Liquid Glass for Adobe into detected Photoshop, Illustrator and After Effects 2024+?\n\nClose Adobe apps and background renders first. Presets and saved looks are kept.", kName,
			MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
			std::vector<std::wstring> log;
			gLog = &log;
			Install();
			MessageBoxW(nullptr, Summary(log).c_str(), kName, MB_ICONINFORMATION);
		}
	}
	Sleep(300); // let WebView2 release the payload files
	Cleanup();
	return 0;
}
