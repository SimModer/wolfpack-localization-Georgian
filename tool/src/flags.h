#pragma once
#include <string>

#include "imgui.h"

struct ID3D11Device;

// Country flags for the language packs. PNGs are embedded in the exe (resource FLAG_<CODE>).
void flags_init(ID3D11Device* device);
// Draws the flag of a language code ("de", "pt-BR", ...) with its top-left corner at p and the given height.
// Unknown codes get a neutral box with the code. Returns the width that was used.
// With slot > 0 the flag is centred in a slot of that width, so wide and narrow flags line up in a list.
// Returns the width that was used (the slot width, or the flag width).
float flag_draw(ImDrawList* dl, const std::string& code, ImVec2 p, float height, float slot = 0);
