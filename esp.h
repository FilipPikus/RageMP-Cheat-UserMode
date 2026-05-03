#pragma once
#include "imgui/imgui.h"
#include <vector>
#include <string>

struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };
struct Matrix4x4 { float m[4][4]; };

struct PlayerData {
    uintptr_t address;
    Vector3 position;
    float health;
    float maxHealth;
    std::string name;
    bool isVisible;
    bool isValid;
    uintptr_t boneManager;
};

#include <chrono>

namespace Config {
    inline bool showMenu = false;
    inline bool esp_box = true;
    inline bool esp_name = true;
    inline bool esp_health = true;
    inline bool esp_skeleton = true;
    inline bool fov_changer = false;
    inline float fov_value = 90.0f;
    
    // Aimbot Settings
    inline bool aimbot = true;
    inline float aimbot_fov = 100.0f;
    inline float aimbot_smooth = 5.0f;

    // Misc Settings
    inline bool anti_afk = false;
    inline bool freecam = false;
    inline float freecam_speed = 1.0f;
}

namespace ESP {
    inline bool WorldToScreen(const Vector3& world_pos, Vector2& screen_pos, const Matrix4x4& view_matrix, int screenWidth, int screenHeight)
    {
        float w = view_matrix.m[0][3] * world_pos.x + view_matrix.m[1][3] * world_pos.y + view_matrix.m[2][3] * world_pos.z + view_matrix.m[3][3];
        if (w < 0.001f) return false;

        float inv_w = 1.0f / w;
        
        // Fix: GTA V uses column 1 for X and column 2 for Y
        float x = (view_matrix.m[0][1] * world_pos.x + view_matrix.m[1][1] * world_pos.y + view_matrix.m[2][1] * world_pos.z + view_matrix.m[3][1]) * inv_w;
        float y = (view_matrix.m[0][2] * world_pos.x + view_matrix.m[1][2] * world_pos.y + view_matrix.m[2][2] * world_pos.z + view_matrix.m[3][2]) * inv_w;

        screen_pos.x = (screenWidth / 2.0f) + (x * screenWidth / 2.0f);
        screen_pos.y = (screenHeight / 2.0f) - (y * screenHeight / 2.0f);

        return true;
    }

    inline void DrawBox(Vector2 screen_top, Vector2 screen_bottom, ImU32 color, float thickness = 1.5f) {
        float height = screen_bottom.y - screen_top.y;
        float width = height / 2.0f;

        ImGui::GetOverlayDrawList()->AddRect(
            ImVec2(screen_top.x - width / 2.0f, screen_top.y),
            ImVec2(screen_top.x + width / 2.0f, screen_bottom.y),
            color, 0.0f, 0, thickness
        );
    }

    inline Vector3 GetBonePos(uintptr_t boneManager, int boneId) {
        if (!boneManager) return { 0, 0, 0 };
        Matrix4x4 boneMatrix = driver::vulnerable.get().read_physical_memory<Matrix4x4>(boneManager + (boneId * 0x40));
        return { boneMatrix.m[3][0], boneMatrix.m[3][1], boneMatrix.m[3][2] };
    }

    inline void DrawBone(uintptr_t boneManager, int bone1, int bone2, const Matrix4x4& viewMatrix, int width, int height, ImU32 color) {
        Vector3 b1 = GetBonePos(boneManager, bone1);
        Vector3 b2 = GetBonePos(boneManager, bone2);
        
        if (b1.x == 0.0f && b1.y == 0.0f) return;
        if (b2.x == 0.0f && b2.y == 0.0f) return;

        Vector2 s1, s2;
        if (WorldToScreen(b1, s1, viewMatrix, width, height) && WorldToScreen(b2, s2, viewMatrix, width, height)) {
            ImGui::GetOverlayDrawList()->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), color, 1.5f);
        }
    }

    inline void DrawSkeleton(uintptr_t boneManager, const Matrix4x4& viewMatrix, int width, int height, ImU32 color) {
        if (!boneManager) return;
        
        int Head = 13, Neck = 12, Spine = 8;
        int LShoulder = 14, LElbow = 15, LHand = 16;
        int RShoulder = 31, RElbow = 32, RHand = 33;
        int LHip = 48, LKnee = 49, LFoot = 50;
        int RHip = 55, RKnee = 56, RFoot = 57;

        // Spine/Head
        DrawBone(boneManager, Head, Neck, viewMatrix, width, height, color);
        DrawBone(boneManager, Neck, Spine, viewMatrix, width, height, color);

        // Arms
        DrawBone(boneManager, Neck, LShoulder, viewMatrix, width, height, color);
        DrawBone(boneManager, LShoulder, LElbow, viewMatrix, width, height, color);
        DrawBone(boneManager, LElbow, LHand, viewMatrix, width, height, color);

        DrawBone(boneManager, Neck, RShoulder, viewMatrix, width, height, color);
        DrawBone(boneManager, RShoulder, RElbow, viewMatrix, width, height, color);
        DrawBone(boneManager, RElbow, RHand, viewMatrix, width, height, color);

        // Legs
        DrawBone(boneManager, Spine, LHip, viewMatrix, width, height, color);
        DrawBone(boneManager, LHip, LKnee, viewMatrix, width, height, color);
        DrawBone(boneManager, LKnee, LFoot, viewMatrix, width, height, color);

        DrawBone(boneManager, Spine, RHip, viewMatrix, width, height, color);
        DrawBone(boneManager, RHip, RKnee, viewMatrix, width, height, color);
        DrawBone(boneManager, RKnee, RFoot, viewMatrix, width, height, color);
    }

    inline void DrawHealthBar(Vector2 screen_top, Vector2 screen_bottom, float health, float maxHealth) {
        float height = screen_bottom.y - screen_top.y;
        float width = height / 2.0f;

        float range = maxHealth - 100.0f;
        if (range <= 0.0f) range = 100.0f; // fallback if maxHealth is weird
        float health_pc = (health - 100.0f) / range;
        
        if (health_pc > 1.0f) health_pc = 1.0f;
        if (health_pc < 0.0f) health_pc = 0.0f;

        ImGui::GetOverlayDrawList()->AddRectFilled(
            ImVec2(screen_top.x - width / 2.0f - 6.0f, screen_top.y),
            ImVec2(screen_top.x - width / 2.0f - 2.0f, screen_bottom.y),
            ImColor(0, 0, 0, 180)
        );

        ImGui::GetOverlayDrawList()->AddRectFilled(
            ImVec2(screen_top.x - width / 2.0f - 5.0f, screen_bottom.y - (height * health_pc)),
            ImVec2(screen_top.x - width / 2.0f - 3.0f, screen_bottom.y),
            ImColor((int)(255 * (1.0f - health_pc)), (int)(255 * health_pc), 0)
        );
    }

    inline void RenderMenu() {
        if (Config::showMenu) {
            ImGui::Begin("PikusClientV2 Settings", &Config::showMenu, ImGuiWindowFlags_AlwaysAutoResize);
            
            if (ImGui::CollapsingHeader("Aimbot", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enable Aimbot (Right Click)", &Config::aimbot);
                ImGui::SliderFloat("Aimbot FOV", &Config::aimbot_fov, 10.0f, 500.0f);
                ImGui::SliderFloat("Aimbot Smooth", &Config::aimbot_smooth, 1.0f, 20.0f);
            }

            if (ImGui::CollapsingHeader("ESP", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Box ESP", &Config::esp_box);
                ImGui::Checkbox("Name ESP", &Config::esp_name);
                ImGui::Checkbox("Health Bar", &Config::esp_health);
                ImGui::Checkbox("Skeleton ESP", &Config::esp_skeleton);
            }
            
            if (ImGui::CollapsingHeader("Misc", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Anti-AFK (Memory)", &Config::anti_afk);
                ImGui::Checkbox("Freecam", &Config::freecam);
                if (Config::freecam) {
                    ImGui::SliderFloat("Freecam Speed", &Config::freecam_speed, 0.1f, 10.0f);
                }
                ImGui::Checkbox("FOV Changer", &Config::fov_changer);
                if (Config::fov_changer) {
                    ImGui::SliderFloat("FOV Value", &Config::fov_value, 60.0f, 150.0f);
                }
            }

            ImGui::End();
        }
    }

    inline void Render(const std::vector<PlayerData>& players, Matrix4x4 viewMatrix, int width, int height, float fps, uintptr_t cameraPtr) {
        char watermark[128];
        sprintf_s(watermark, "PikusClientV2 <3 - FPS: %.0f | Entities: %d | W: %.2f", fps, (int)players.size(), viewMatrix.m[3][3]);
        ImGui::GetOverlayDrawList()->AddText(ImVec2(10, 10), IM_COL32(255, 255, 255, 255), watermark);

        RenderMenu();

        float closest_dist = Config::aimbot_fov;
        Vector2 center(width / 2.0f, height / 2.0f);
        Vector3 best_target_3d(0, 0, 0);
        bool has_target = false;

        // Draw Aimbot FOV Circle
        if (Config::aimbot) {
            ImGui::GetOverlayDrawList()->AddCircle(ImVec2(center.x, center.y), Config::aimbot_fov, IM_COL32(255, 255, 255, 100), 32);
        }

        for (const auto& player : players) {
            if (!player.isValid) continue;

            Vector2 screen_top, screen_bottom;
            Vector3 head_pos = player.position;
            head_pos.z += 0.8f;
            Vector3 feet_pos = player.position;
            feet_pos.z -= 1.0f;

            if (WorldToScreen(head_pos, screen_top, viewMatrix, width, height) &&
                WorldToScreen(feet_pos, screen_bottom, viewMatrix, width, height)) {

                // Aimbot logic
                if (Config::aimbot && player.isVisible && player.health > 100.0f) {
                    float dist = sqrt(pow(screen_top.x - center.x, 2) + pow(screen_top.y - center.y, 2));
                    if (dist < closest_dist) {
                        closest_dist = dist;
                        best_target_3d = head_pos;
                        has_target = true;
                    }
                }

                // Weiß, wenn sichtbar, rot, wenn unsichtbar
                ImU32 col = player.isVisible ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);

                if (Config::esp_box) {
                    DrawBox(screen_top, screen_bottom, col);
                }

                if (Config::esp_health) {
                    DrawHealthBar(screen_top, screen_bottom, player.health, player.maxHealth);
                }

                if (Config::esp_skeleton && player.boneManager) {
                    DrawSkeleton(player.boneManager, viewMatrix, width, height, col);
                }

                if (Config::esp_name) {
                    float h = screen_bottom.y - screen_top.y;
                    float w = h / 2.0f;
                    ImGui::GetOverlayDrawList()->AddText(
                        ImVec2(screen_top.x - w / 2.0f, screen_top.y - 15.0f),
                        col,
                        player.name.c_str()
                    );
                }
            }
        }

        // Check if Right Click or Left Alt is pressed
        bool aimkey_pressed = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) || (GetAsyncKeyState(VK_LMENU) & 0x8000);

        if (Config::aimbot && has_target && aimkey_pressed && cameraPtr) {
            Vector3 camPos = driver::vulnerable.get().read_physical_memory<Vector3>(cameraPtr + 0x60);
            
            Vector3 delta;
            delta.x = best_target_3d.x - camPos.x;
            delta.y = best_target_3d.y - camPos.y;
            delta.z = best_target_3d.z - camPos.z;

            float distance = sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

            // Calculate target Pitch and Yaw in degrees
            float pitch = asin(delta.z / distance) * (180.0f / 3.14159265358979323846f);
            float yaw = atan2(-delta.x, delta.y) * (180.0f / 3.14159265358979323846f);

            // Read current angles (Pitch = x, Roll = y, Yaw = z) in DEGREES
            Vector3 current_angles = driver::vulnerable.get().read_physical_memory<Vector3>(cameraPtr + 0x3D0);
            
            Vector3 smoothed_angles = current_angles;

            // Handle Yaw wrapping (from -180 to 180)
            float yaw_diff = yaw - current_angles.z; // Yaw is .z
            if (yaw_diff > 180.0f) yaw_diff -= 360.0f;
            if (yaw_diff < -180.0f) yaw_diff += 360.0f;

            smoothed_angles.x = current_angles.x + ((pitch - current_angles.x) / Config::aimbot_smooth);
            smoothed_angles.z = current_angles.z + (yaw_diff / Config::aimbot_smooth);

            // Write the Euler angles back to TPS camera
            driver::vulnerable.get().write_physical_memory<Vector3>(cameraPtr + 0x3D0, smoothed_angles);
            
            // Try to write to FPS angles if available
            uintptr_t pMyFPSAngles = driver::vulnerable.get().read_physical_memory<uintptr_t>(cameraPtr + 0x48);
            if (pMyFPSAngles) {
                driver::vulnerable.get().write_physical_memory<Vector3>(pMyFPSAngles + 0x40, smoothed_angles);
            } else {
                driver::vulnerable.get().write_physical_memory<Vector3>(cameraPtr + 0x40, smoothed_angles);
            }
        }

        // Anti-AFK Memory Logic (Move camera slightly every 2 minutes to prevent AFK kick)
        static auto last_afk_time = std::chrono::steady_clock::now();
        if (Config::anti_afk && cameraPtr) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_afk_time).count() >= 120) {
                Vector3 current_angles = driver::vulnerable.get().read_physical_memory<Vector3>(cameraPtr + 0x3D0);
                
                // Jiggle the camera slightly (Yaw is Z)
                current_angles.z += 1.5f;

                driver::vulnerable.get().write_physical_memory<Vector3>(cameraPtr + 0x3D0, current_angles);
                driver::vulnerable.get().write_physical_memory<Vector3>(cameraPtr + 0x40, current_angles);
                last_afk_time = now;
            }
        }
    }
}