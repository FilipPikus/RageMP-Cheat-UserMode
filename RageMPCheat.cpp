#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <d3d9.h>
#include <dwmapi.h>
#include "driver.hpp"
#include "offsets.h"
#include "overlay.h"
#include "esp.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dwmapi.lib")

namespace Cache {
    std::vector<PlayerData> players;
    Matrix4x4 viewMatrix;
    uintptr_t gameBase = 0;
    float fps = 0;
    std::mutex mtx;
    uintptr_t cameraPtr = 0;
    bool running = true;
}

static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) return false;
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0) return false;
    return true;
}

void slow_thread() {
    while (Cache::running) {
        if (!Cache::gameBase) {
            Cache::gameBase = driver::vulnerable.exported_functions().get_module_dll(L"GTA5.exe");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        uintptr_t world = driver::vulnerable.get().read_physical_memory<uintptr_t>(Cache::gameBase + Offsets::g_world);
        uintptr_t replay_interface = driver::vulnerable.get().read_physical_memory<uintptr_t>(Cache::gameBase + Offsets::g_replayinterface);
        
        if (!world || !replay_interface) {
            std::cout << "\rWaiting for world/replay interface...       " << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        uintptr_t local_player = driver::vulnerable.get().read_physical_memory<uintptr_t>(world + 0x8);
        uintptr_t ped_interface = driver::vulnerable.get().read_physical_memory<uintptr_t>(replay_interface + Offsets::Replay::PedInterface);
        
        if (!ped_interface) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        uintptr_t ped_list = driver::vulnerable.get().read_physical_memory<uintptr_t>(ped_interface + Offsets::Replay::PedList);
        int ped_count = driver::vulnerable.get().read_physical_memory<int>(ped_interface + Offsets::Replay::PedCount);

        std::vector<PlayerData> tempPlayers;
        int validPlayersCount = 0;

        for (int i = 0; i < ped_count; i++) {
            uintptr_t ped = driver::vulnerable.get().read_physical_memory<uintptr_t>(ped_list + (i * 0x10));
            if (!ped || ped == local_player) continue;

            // In GTA V, Ped Type defines if it's a real player or an NPC.
            // (pedType << 11) >> 15 gives the exact type. Player is usually 1, 2, or 4.
            // An even better check is to see if the CPlayerInfo pointer exists.
            uintptr_t player_info = driver::vulnerable.get().read_physical_memory<uintptr_t>(ped + Offsets::Ped::PlayerInfo);
            if (!player_info) continue; // If no CPlayerInfo, it's an NPC/animal

            float health = driver::vulnerable.get().read_physical_memory<float>(ped + Offsets::Ped::Health);
            float max_health = driver::vulnerable.get().read_physical_memory<float>(ped + Offsets::Ped::MaxHealth);
            
            // In GTA 5, health below 100.0f is typically dead/invisible.
            // Also add an upper bound to filter out garbage memory reads (which cause buffer overflows)
            if (health <= 100.0f || health > 100000.0f || max_health > 100000.0f) continue;

            // Check if player is invisible (Bit 0 of 0x2C)
            BYTE invis = driver::vulnerable.get().read_physical_memory<BYTE>(ped + Offsets::Ped::Invisible);
            bool isVisible = (invis & 0x01) != 0; // In GTA V, if bit 0 is set (1), the ped is VISIBLE. If 0, it's INVISIBLE.

            PlayerData data;
            data.address = ped;
            data.health = health;
            data.maxHealth = max_health;
            data.isVisible = isVisible;
            data.boneManager = driver::vulnerable.get().read_physical_memory<uintptr_t>(ped + 0x430); // BoneManager
            
            uintptr_t navigation = driver::vulnerable.get().read_physical_memory<uintptr_t>(ped + 0x30);
            if (navigation) {
                data.position = driver::vulnerable.get().read_physical_memory<Vector3>(navigation + 0x50);
            } else {
                data.position = driver::vulnerable.get().read_physical_memory<Vector3>(ped + Offsets::Ped::Position);
            }
            
            char buf[128]; // Increased buffer size to prevent crashes on large floats
            snprintf(buf, sizeof(buf), "Enemy [%.0f/%.0f HP]", health, max_health);
            data.name = buf;
            
            data.isValid = true;

            tempPlayers.push_back(data);
            validPlayersCount++;
        }

        std::cout << "\r[+] Total Entities: " << ped_count 
                  << " | Valid Players found: " << validPlayersCount 
                  << "                  " << std::flush;

        {
            std::lock_guard<std::mutex> lock(Cache::mtx);
            Cache::players = std::move(tempPlayers);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); 
    }
}

void fast_thread() {
    while (Cache::running) {
        if (!Cache::gameBase) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        uintptr_t viewport_ptr = driver::vulnerable.get().read_physical_memory<uintptr_t>(Cache::gameBase + Offsets::g_viewport);
        if (viewport_ptr) {
            Matrix4x4 vMatrix = driver::vulnerable.get().read_physical_memory<Matrix4x4>(viewport_ptr + Offsets::Viewport::ViewMatrix);
            std::lock_guard<std::mutex> lock(Cache::mtx);
            Cache::viewMatrix = vMatrix;
        }

        uintptr_t camera_ptr = driver::vulnerable.get().read_physical_memory<uintptr_t>(Cache::gameBase + Offsets::g_camera);

        static Vector3 freecamPos = {0, 0, 0};
        static bool freecamInitialized = false;

        if (Config::freecam && camera_ptr) {
            if (!freecamInitialized) {
                freecamPos = driver::vulnerable.get().read_physical_memory<Vector3>(camera_ptr + 0x60);
                freecamInitialized = true;
            }

            Vector3 camRot = driver::vulnerable.get().read_physical_memory<Vector3>(camera_ptr + 0x3D0);

            float radPitch = camRot.x * (3.14159265358979323846f / 180.0f);
            float radYaw = camRot.z * (3.14159265358979323846f / 180.0f);

            Vector3 forward;
            forward.x = -sin(radYaw) * cos(radPitch);
            forward.y = cos(radYaw) * cos(radPitch);
            forward.z = sin(radPitch);

            Vector3 right;
            right.x = cos(radYaw);
            right.y = sin(radYaw);
            right.z = 0.0f;

            float speed = Config::freecam_speed * 2.0f;

            if (GetAsyncKeyState('W') & 0x8000) {
                freecamPos.x += forward.x * speed;
                freecamPos.y += forward.y * speed;
                freecamPos.z += forward.z * speed;
            }
            if (GetAsyncKeyState('S') & 0x8000) {
                freecamPos.x -= forward.x * speed;
                freecamPos.y -= forward.y * speed;
                freecamPos.z -= forward.z * speed;
            }
            if (GetAsyncKeyState('A') & 0x8000) {
                freecamPos.x -= right.x * speed;
                freecamPos.y -= right.y * speed;
                freecamPos.z -= right.z * speed;
            }
            if (GetAsyncKeyState('D') & 0x8000) {
                freecamPos.x += right.x * speed;
                freecamPos.y += right.y * speed;
                freecamPos.z += right.z * speed;
            }

            driver::vulnerable.get().write_physical_memory<Vector3>(camera_ptr + 0x60, freecamPos);
        } else {
            freecamInitialized = false;
        }

        if (Config::fov_changer && camera_ptr) {
            // Write FOV to the camera. Note: Offset may vary, commonly 0x10, 0x3C, or 0x44 depending on the exact camera struct.
            // Try common FOV offset in GTA V
            driver::vulnerable.get().write_physical_memory<float>(camera_ptr + 0x3C, Config::fov_value);
        }

        {
            std::lock_guard<std::mutex> lock(Cache::mtx);
            Cache::cameraPtr = camera_ptr;
            for (auto& player : Cache::players) {
                if (!player.isValid) continue;
                
                uintptr_t navigation = driver::vulnerable.get().read_physical_memory<uintptr_t>(player.address + 0x30);
                if (navigation) {
                    player.position = driver::vulnerable.get().read_physical_memory<Vector3>(navigation + 0x50);
                } else {
                    player.position = driver::vulnerable.get().read_physical_memory<Vector3>(player.address + Offsets::Ped::Position);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); 
    }
}

int main() {
    if (HWND console_window = GetConsoleWindow()) {
        ShowWindow(console_window, SW_SHOW); 
    }

    std::cout << "[~] Initializing Cheat..." << std::endl;

    if (!driver::vulnerable.s_dgx().get_export()) {
        std::cout << "[-] Failed to connect to Driver!" << std::endl;
        system("pause");
        return 1;
    }
    std::cout << "[+] Connected to Driver." << std::endl;

    std::cout << "[~] Waiting for GTA5.exe..." << std::endl;
    driver::vulnerable.proc_id = driver::vulnerable.exported_functions().get_process_id("GTA5.exe");
    while (!driver::vulnerable.proc_id) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        driver::vulnerable.proc_id = driver::vulnerable.exported_functions().get_process_id("GTA5.exe");
    }
    std::cout << "[+] Found GTA5.exe! PID: " << driver::vulnerable.proc_id << std::endl;

    std::cout << "[~] Retrieving GTA5 base address..." << std::endl;
    driver::vulnerable.base_address = driver::vulnerable.exported_functions().get_module_dll(L"GTA5.exe");
    
    while (!driver::vulnerable.base_address) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        driver::vulnerable.base_address = driver::vulnerable.exported_functions().get_module_dll(L"GTA5.exe");
    }
    
    std::cout << "[+] GTA5 Base: 0x" << std::hex << std::uppercase << driver::vulnerable.base_address << std::dec << std::endl;

    std::cout << "[~] Initializing Overlay..." << std::endl;
    if (!Overlay::Init()) {
        std::cout << "[-] Failed to create the overlay window!" << std::endl;
        MessageBoxA(0, "failed to create the overlay", "Error", MB_OKCANCEL);
        return 1;
    }

    if (!CreateDeviceD3D(Overlay::hwnd)) {
        std::cout << "[-] Failed to initialize DirectX 9!" << std::endl;
        return 1;
    }
    std::cout << "[+] Overlay and DirectX initialized successfully." << std::endl;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(Overlay::hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    std::cout << "[+] Starting Memory Threads..." << std::endl;
    std::thread(slow_thread).detach();
    std::thread(fast_thread).detach();
    
    std::cout << "\n[+] Cheat is fully loaded and running!\n\n" << std::endl;

    const float targetFPS = 120.0f;
    const std::chrono::duration<float, std::milli> targetFrameTime(1000.0f / targetFPS);

    while (true) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        HWND gameHwnd = FindWindowA("grcWindow", "RAGE Multiplayer");
        if (!gameHwnd) gameHwnd = FindWindowA("grcWindow", "Grand Theft Auto V");
        if (!gameHwnd) gameHwnd = FindWindowA(NULL, "RAGE Multiplayer");
        
        if (gameHwnd) {
            Overlay::UpdatePosition(gameHwnd);
        }

        Overlay::PollMessages();

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            std::vector<PlayerData> localPlayers;
            Matrix4x4 localMatrix;
            float localFps;
            uintptr_t localCamera;
            {
                std::lock_guard<std::mutex> lock(Cache::mtx);
                localPlayers = Cache::players;
                localMatrix = Cache::viewMatrix;
                localFps = ImGui::GetIO().Framerate;
                localCamera = Cache::cameraPtr;
            }

            RECT rect;
            GetClientRect(Overlay::hwnd, &rect);
            ESP::Render(localPlayers, localMatrix, rect.right - rect.left, rect.bottom - rect.top, localFps, localCamera);
        }

        ImGui::Render();

        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        
        // When menu is open, use alpha=1 to capture all mouse inputs across the screen
        D3DCOLOR clearColor = Config::showMenu ? D3DCOLOR_ARGB(1, 0, 0, 0) : D3DCOLOR_ARGB(0, 0, 0, 0);
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0);

        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        
        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            g_pd3dDevice->Reset(&g_d3dpp);
            ImGui_ImplDX9_CreateDeviceObjects();
        }

        if (GetAsyncKeyState(VK_END) & 1) break;
        
        static bool insert_pressed = false;
        static bool last_menu_state = false;

        // VK_INSERT (Insert) or VK_F4 as alternative menu key
        if ((GetAsyncKeyState(VK_INSERT) & 0x8000) || (GetAsyncKeyState(VK_F4) & 0x8000) || (GetAsyncKeyState(VK_HOME) & 0x8000)) {
            if (!insert_pressed) {
                Config::showMenu = !Config::showMenu;
                insert_pressed = true;
            }
        } else {
            insert_pressed = false;
        }

        // Handle menu state changes (e.g. if closed via ImGui 'X' or hotkey)
        if (Config::showMenu != last_menu_state) {
            if (Config::showMenu) {
                // Remove WS_EX_TRANSPARENT and WS_EX_NOACTIVATE so we can click
                SetWindowLong(Overlay::hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED);
                SetWindowPos(Overlay::hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
                
                SetForegroundWindow(Overlay::hwnd);
                SetFocus(Overlay::hwnd);
                SetActiveWindow(Overlay::hwnd);

                ImGui::GetIO().MouseDrawCursor = true;
            } else {
                // Restore WS_EX_TRANSPARENT and WS_EX_NOACTIVATE
                SetWindowLong(Overlay::hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE);
                SetWindowPos(Overlay::hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
                
                if (gameHwnd) {
                    SetForegroundWindow(gameHwnd);
                    SetFocus(gameHwnd);
                }
                ImGui::GetIO().MouseDrawCursor = false;
            }
            last_menu_state = Config::showMenu;
        }

        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> timeElapsed = frameEnd - frameStart;
        while (timeElapsed < targetFrameTime) {
            if (targetFrameTime - timeElapsed > std::chrono::milliseconds(2)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                std::this_thread::yield();
            }
            frameEnd = std::chrono::high_resolution_clock::now();
            timeElapsed = frameEnd - frameStart;
        }
    }

    Cache::running = false;
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
