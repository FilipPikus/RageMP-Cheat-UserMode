#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {
    inline HWND hwnd = nullptr;
    inline WNDCLASSEXA wc = { 0 };

    inline LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
            return true;

        switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcA(hWnd, message, wParam, lParam);
    }

    inline bool Init() {
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "external-gta-base";

        if (!RegisterClassExA(&wc)) return false;

        hwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            wc.lpszClassName,
            "RageMP-External",
            WS_POPUP,
            0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            NULL, NULL, wc.hInstance, NULL
        );

        if (!hwnd) return false;

        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
        
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        return true;
    }

    inline void UpdatePosition(HWND gameHwnd) {
        if (!hwnd || !gameHwnd) return;

        static RECT lastRect = {0, 0, 0, 0};
        static POINT lastPt = {0, 0};

        RECT clientRect;
        if (GetClientRect(gameHwnd, &clientRect)) {
            POINT pt = { 0, 0 };
            ClientToScreen(gameHwnd, &pt);
            
            if (clientRect.right != lastRect.right || clientRect.bottom != lastRect.bottom ||
                pt.x != lastPt.x || pt.y != lastPt.y) {
                
                SetWindowPos(hwnd, HWND_TOPMOST, pt.x, pt.y, clientRect.right, clientRect.bottom, SWP_SHOWWINDOW);
                
                lastRect = clientRect;
                lastPt = pt;
            }
        }
    }

    inline void PollMessages() {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
}
