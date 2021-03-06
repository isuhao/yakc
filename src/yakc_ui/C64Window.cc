//------------------------------------------------------------------------------
//  C64Window.cc
//------------------------------------------------------------------------------
#include "C64Window.h"
#include "IMUI/IMUI.h"
#include "yakc/util/breadboard.h"

using namespace Oryol;

namespace YAKC {

//------------------------------------------------------------------------------
void
C64Window::Setup(yakc& emu) {
    this->setName("C64 Misc State");
}

//------------------------------------------------------------------------------
bool
C64Window::Draw(yakc& emu) {
    ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_Once);
    if (ImGui::Begin(this->title.AsCStr(), &this->Visible)) {
        ImGui::Text("CPU IO DDR: %02X PORT: %02X", board.m6502.io_ddr, board.m6502.io_port);
    }
    ImGui::End();
    return this->Visible;
}

} // namespace YAKC
