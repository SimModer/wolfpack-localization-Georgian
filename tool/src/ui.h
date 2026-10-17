#pragma once
#include <windows.h>

struct ImFont;

// Called once after the ImGui context and fonts exist.
void ui_init(ImFont* semibold, ImFont* black, HWND hwnd);
// Draws the whole window (call between ImGui::NewFrame and ImGui::Render).
bool ui_frame(float dpi_scale);
// Custom title bar: height of the draggable strip and width of the button area at its right (client pixels).
float ui_caption_height();
float ui_caption_buttons_width();
