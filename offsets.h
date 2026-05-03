#pragma once
#include <windows.h>
#include "driver.hpp"
namespace Offsets
{
    // --- Base Pointers (Add to ModuleBase) ---
    // These pointers lead to the main game structures
    inline DWORD64 g_world = 0x25B14B0;           // Entry for LocalPlayer and Peds
    inline DWORD64 GameBase = driver::vulnerable()->exported_functions().get_module_dll(nullptr);
    inline DWORD64 world_ptr = GameBase + 0x25b14b0;
    inline DWORD64 g_replayinterface = 0x1FBD4F0; // Entry for Entity List (Finding enemies)
    inline DWORD64 g_viewport = 0x201DBA0;        // ViewMatrix for WorldToScreen
    inline DWORD64 g_camera_manager = 0x20025B8;  // Camera angles and FOV

    // --- Ped / Entity Offsets (Relative to Ped Pointer) ---
    // Used for both LocalPlayer and Enemies
    namespace Ped
    {
        inline DWORD Health = 0x280;              // float (100.0 = Dead, 200.0 = Full)
        inline DWORD MaxHealth = 0x284;           // float
        inline DWORD Armor = 0x150C;              // float
        inline DWORD Position = 0x90;             // Vector3 (X, Y, Z)
        inline DWORD Rotation = 0xD4;             // Vector3 (Roll, Pitch, Yaw)
        inline DWORD EntityType = 0x2B;           // byte (Identify if it's a player/NPC)
        inline DWORD Invisible = 0x2C;            // byte (Bit 0 = Invisible)
        inline DWORD PlayerInfo = 0x10A8;         // Pointer (Contains Stamina, Wanted Level)
        inline DWORD WeaponManager = 0x10D8;      // Pointer to current weapon list
        inline DWORD BoneManager = 0x430;         // Pointer to BoneManager
    }

    // --- Weapon Offsets (Relative to CurrentWeapon) ---
    // Path: LocalPlayer -> WeaponManager -> 0x20 (CurrentWeapon)
    namespace Weapon
    {
        inline DWORD CurrentWeaponPtr = 0x20;     // Offset inside WeaponManager
        inline DWORD Recoil = 0x2F4;              // float (Set to 0.0f for No Recoil)
        inline DWORD Spread = 0x2F8;              // float (Set to 0.0f for No Spread)
        inline DWORD MuzzleVelocity = 0x114;      // float (Bullet Speed)
    }

    // --- Camera Offsets (Relative to CameraManager) ---
    namespace Camera
    {
        inline DWORD FOV = 0x10;                  // float
        inline DWORD ViewAnglesPitch = 0x3D0;     // float (Up/Down)
        inline DWORD ViewAnglesYaw = 0x3D4;       // float (Left/Right)
    }

    // --- ReplayInterface Chain (For Entity List) ---
    namespace Replay
    {
        inline DWORD PedInterface = 0x18;         // Offset from ReplayInterface
        inline DWORD PedList = 0x100;             // Offset from PedInterface
        inline DWORD PedCount = 0x108;            // Offset from PedInterface
    }

    // --- Viewport / Matrix ---
    namespace Viewport
    {
        inline DWORD ViewMatrix = 0x24C;          // Offset from Viewport Pointer
    }
    
    // --- Camera (For Memory Aimbot) ---
    inline DWORD g_camera = 0x20025B8; // 3095 Camera offset
}