#include "Direct3DDevice9.h"

#include "../SDK/SDK.h"
#include "../Features/ImGui/Render.h"
#include "../Features/ImGui/Menu/Menu.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

MAKE_HOOK(Direct3DDevice9_Present, U::Memory.GetVirtual(I::DirectXDevice, 17), HRESULT,
	IDirect3DDevice9* pDevice, const RECT* pSource, const RECT* pDestination, const RGNDATA* pDirtyRegion)
{
	DEBUG_RETURN(Direct3DDevice9_Present, pDevice, pSource, pDestination, pDirtyRegion);

	if (!G::Unload)
		F::Render.Render(pDevice);

	return CALL_ORIGINAL(pDevice, pSource, pDestination, pDirtyRegion);
}

MAKE_HOOK(Direct3DDevice9_Reset, U::Memory.GetVirtual(I::DirectXDevice, 16), HRESULT,
	LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	DEBUG_RETURN(Direct3DDevice9_Reset, pDevice, pPresentationParameters);

	ImGui_ImplDX9_InvalidateDeviceObjects();
	const HRESULT Original = CALL_ORIGINAL(pDevice, pPresentationParameters);
	ImGui_ImplDX9_CreateDeviceObjects();
	return Original;
}

LONG __stdcall WndProc::Func(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (F::Menu.m_bIsOpen)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

		if ((ImGui::GetIO().WantTextInput || F::Menu.m_bInKeybind) && WM_KEYFIRST <= uMsg && uMsg <= WM_KEYLAST)
		{
			// FIXED: Null check prevents crash when InputSystem is invalid
			if (I::InputSystem)
				I::InputSystem->ResetInputState();
			return 1;
		}

		if (WM_MOUSEFIRST <= uMsg && uMsg <= WM_MOUSELAST)
			return 1;
	}

	return CallWindowProc(Original, hWnd, uMsg, wParam, lParam);
}

MAKE_HOOK(VGuiSurface_LockCursor, U::Memory.GetVirtual(I::MatSystemSurface, 62), void,
	void* rcx)
{
<<<<<<< HEAD
	// FIXED: Null check prevents crash when MatSystemSurface is invalid
	if (F::Menu.m_bIsOpen && I::MatSystemSurface)
=======
	DEBUG_RETURN(VGuiSurface_LockCursor, rcx);

	if (F::Menu.m_bIsOpen)
>>>>>>> upstream/master
		return I::MatSystemSurface->UnlockCursor();

	CALL_ORIGINAL(rcx);
}

MAKE_HOOK(VGuiSurface_SetCursor, U::Memory.GetVirtual(I::MatSystemSurface, 51), void,
	void* rcx, HCursor cursor)
{
	DEBUG_RETURN(VGuiSurface_SetCursor, rcx, cursor);

	if (F::Menu.m_bIsOpen)
	{
		// OPTIMIZED: Lookup table avoids switch statement overhead
		static constexpr HCursor cursorMap[9] = { 2, 3, 12, 11, 10, 9, 8, 14, 13 };
		// SAFETY: Bounds check prevents out-of-range access
		if (F::Render.Cursor >= 0 && F::Render.Cursor < 9)
			cursor = cursorMap[F::Render.Cursor];
	}

	CALL_ORIGINAL(rcx, cursor);
}

void WndProc::Initialize()
{
	hwWindow = SDK::GetTeamFortressWindow();

	// FIXED: Validate window handle before setting window procedure
	if (hwWindow)
		Original = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Func)));
}

void WndProc::Unload()
{
	// FIXED: Validate handles before restoring original window procedure
	if (hwWindow && Original)
		SetWindowLongPtr(hwWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Original));
}