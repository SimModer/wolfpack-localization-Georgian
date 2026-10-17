#pragma once
#include <windows.h>

struct ImFont;

// Called once after the ImGui context and fonts exist. `bold` is used for headings and buttons.
void ui_init(ImFont* bold);
// Draws the whole window (call between ImGui::NewFrame and ImGui::Render).
bool ui_frame(float dpi_scale);
