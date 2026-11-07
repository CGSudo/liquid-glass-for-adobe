#pragma once
#include <string>
#include <vector>

namespace setup {
struct Copy { std::wstring file, destination; };
struct Plan { std::vector<Copy> copies; std::vector<std::wstring> obsolete; };

inline bool SupportedAE(const std::wstring& version)
{
	size_t end = version.find_first_not_of(L"0123456789");
	if (end == 0 || version.empty()) return false;
	if (end != std::wstring::npos && version[end] != L'.' && version[end] != L' ') return false;
	const auto number = version.substr(0, end);
	if (number.size() > 4) return false;
	int major = std::stoi(number);
	return (major >= 24 && major < 100) || (major >= 2024 && major < 2100);
}

inline Plan NativePlan(const std::vector<std::wstring>& illustrator,
	const std::vector<std::wstring>& ae, const std::wstring& programFiles)
{
	Plan p;
	for (const auto& app : illustrator) {
		p.copies.push_back({ L"LiquidGlass.aip", app + L"\\Plug-ins\\Liquid Glass\\LiquidGlass.aip" });
		for (const auto* old : { L"\\Plug-ins\\LiquidGlass.aip", L"\\Plug-ins\\SudoLiquidGlass.aip",
			L"\\Plug-ins\\Sudo Liquid Glass\\LiquidGlass.aip", L"\\Plug-ins\\Sudo Liquid Glass\\SudoLiquidGlass.aip",
			L"\\Plug-ins\\CGSudo\\LiquidGlass.aip", L"\\Plug-ins\\Liquid Glass\\SudoLiquidGlass.aip" })
			p.obsolete.push_back(app + old);
	}
	for (const auto& app : ae)
		for (const auto* file : { L"LiquidGlass.aex", L"LiquidGlassBrightness.aex" })
			p.copies.push_back({ file, app + L"\\Support Files\\Plug-ins\\CGSudo\\" + file });
	if (!programFiles.empty()) p.obsolete.push_back(programFiles + L"\\Adobe\\Common\\Plug-ins\\7.0\\MediaCore\\CGSudo\\LiquidGlass.aex");
	return p;
}
}
