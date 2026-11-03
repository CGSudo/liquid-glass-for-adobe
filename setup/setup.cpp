// Liquid Glass for Adobe: one-click Windows installer.
// The window is a Windows 11 acrylic (glass) window showing setup.html in WebView2: a live tour of the looks and one
// Install button. This file does the installing:
//   Photoshop:   Adobe's plugin installer (UPIA, part of Creative Cloud) installs the .ccx for the current user.
//   Illustrator: the .aip is copied into each Illustrator's Plug-ins folder by an elevated copy of this exe
//                (--copy-aip <file>); Program Files needs admin rights, so Windows asks once.
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
static bool gBusy;

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

// Elevated part: copy the Illustrator plug-in into every Illustrator. Exit code 0 = all copied.
static int CopyAip(const std::wstring& src)
{
	int failed = 0;
	for (auto& ai : AppFolders(L"Adobe Illustrator ")) {
		std::wstring dir = ai + L"\\Plug-ins\\Liquid Glass";
		CreateDirectoryW(dir.c_str(), nullptr);
		if (!CopyFileW(src.c_str(), (dir + L"\\LiquidGlass.aip").c_str(), FALSE)) failed++;
	}
	return failed;
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
		L",\"ps\":" + app(L"Adobe Photoshop ", L"Photoshop.exe") + L",\"ai\":" + app(L"Adobe Illustrator ", L"Illustrator.exe") + L"}";
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

static void Install()
{
	// Photoshop: per user, through Creative Cloud's installer. Old copies go first (also the pre-rename "Sudo Liquid Glass"),
	// because the installer silently keeps an installed plugin with the same version.
	if (AppFolders(L"Adobe Photoshop ").empty()) Step(L"ps", L"skip", L"Photoshop isn't installed on this PC.");
	else if (Running(L"Photoshop.exe")) Step(L"ps", L"fail", L"Photoshop is open. Close it, then click Install again.");
	else if (Upia().empty()) Step(L"ps", L"fail", L"The Creative Cloud desktop app wasn't found. Install it, then run Setup again.");
	else {
		Step(L"ps", L"work", L"Installing…");
		std::wstring u = Upia(), ccx = gDir + L"\\LiquidGlass-for-Adobe.ccx";
		Run(u, L"/remove \"Sudo Liquid Glass\"");
		Run(u, L"/remove \"Liquid Glass for Adobe\"");
		DWORD code = Run(u, L"/install \"" + ccx + L"\"");
		std::wstring dir = Env(L"APPDATA") + L"\\Adobe\\UXP\\Plugins\\External\\" + kPluginId + L"_" LG_VERSION;
		if (GetFileAttributesW((dir + L"\\manifest.json").c_str()) != INVALID_FILE_ATTRIBUTES)
			Step(L"ps", L"done", L"Installed. In Photoshop: Plugins › Liquid Glass for Adobe.");
		else Step(L"ps", L"fail", L"Adobe's installer stopped with code " + std::to_wstring(int(code)) + L". Restart the Creative Cloud app and try again.");
	}

	// Illustrator: an elevated copy of this exe copies the plug-in; this side checks the bytes afterwards.
	auto ais = AppFolders(L"Adobe Illustrator ");
	if (ais.empty()) Step(L"ai", L"skip", L"Illustrator isn't installed on this PC.");
	else if (Running(L"Illustrator.exe")) Step(L"ai", L"fail", L"Illustrator is open. Close it, then click Install again.");
	else {
		Step(L"ai", L"work", L"Waiting for permission…");
		wchar_t self[MAX_PATH];
		GetModuleFileNameW(nullptr, self, MAX_PATH);
		std::wstring aip = gDir + L"\\LiquidGlass.aip", args = L"--copy-aip \"" + aip + L"\"";
		SHELLEXECUTEINFOW sei{ sizeof sei };
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		sei.lpVerb = L"runas";
		sei.lpFile = self;
		sei.lpParameters = args.c_str();
		sei.hwnd = gWnd;
		sei.nShow = SW_HIDE;
		if (!ShellExecuteExW(&sei)) Step(L"ai", L"fail", L"Admin permission was declined, so Illustrator wasn't changed.");
		else {
			Step(L"ai", L"work", L"Installing…");
			WaitForSingleObject(sei.hProcess, INFINITE);
			CloseHandle(sei.hProcess);
			std::string want = ReadAll(aip);
			int ok = 0;
			for (auto& a : ais) ok += !want.empty() && ReadAll(a + L"\\Plug-ins\\Liquid Glass\\LiquidGlass.aip") == want;
			if (ok == int(ais.size())) Step(L"ai", L"done", L"Installed. In Illustrator: Effect › Stylize › Liquid Glass…");
			else Step(L"ai", L"fail", L"Couldn't copy the plug-in into Illustrator. Close Illustrator and try again.");
		}
	}
	Tell(L"{\"type\":\"finished\"}");
}

// Unpacks the payload into a fresh temp folder: RCDATA 101, 102, … in this order (setup\build.ps1 uses the same list).
static bool Extract()
{
	static const wchar_t* files[] = { L"setup.html", L"setup-worker.js", L"looks.js", L"engine.js", L"glass_core.wasm", L"logo.png",
		L"LiquidGlass-for-Adobe.ccx", L"LiquidGlass.aip" };
	gDir = Env(L"TEMP") + L"\\LiquidGlassSetup-" + std::to_wstring(GetCurrentProcessId());
	CreateDirectoryW(gDir.c_str(), nullptr);
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
		s += (j.find(L"\"app\":\"ps\"") != std::wstring::npos ? L"Photoshop: " : L"Illustrator: ") + j.substr(t + 8, j.size() - t - 10) + L"\n\n";
	}
	return s;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc == 3 && !wcscmp(argv[1], L"--copy-aip")) return CopyAip(argv[2]);

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
	int cw = MulDiv(920, dpi, 96), ch = MulDiv(600, dpi, 96);
	RECT wa;
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
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
		if (MessageBoxW(nullptr, L"Install Liquid Glass for Adobe into Photoshop and Illustrator?\n\nClose both apps first.", kName,
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
