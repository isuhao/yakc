#pragma once
//------------------------------------------------------------------------------
/**
    @class UI
    @brief imgui-based debugger UI
*/
#include "yakc/KC85Oryol.h"
#include "ui/WindowBase.h"
#include "ui/FileLoader.h"
#include "ui/SnapshotStorage.h"
#include "Time/TimePoint.h"
#include "Core/Containers/Array.h"
#include "IMUI/IMUI.h"

class UI {
public:
    /// setup the UI
    void Setup(yakc::kc85& kc);
    /// discard the UI
    void Discard();
    /// do one frame
    void OnFrame(yakc::kc85& kc);
    /// open a window
    void OpenWindow(yakc::kc85& kc, const Oryol::Ptr<WindowBase>& window);
    /// toggle the UI on/off
    void Toggle();

    static const ImVec4 ColorText;
    static const ImVec4 ColorDetail;
    static const ImVec4 ColorDetailBright;
    static const ImVec4 ColorDetailDark;
    static const ImVec4 ColorBackground;
    static const ImVec4 ColorBackgroundLight;

    struct settings {
        bool crtEffect = false;
        bool colorTV = true;
        float crtWarp = 1.0f/64.0f;
        int cpuSpeed = 1;
    } Settings;

private:
    FileLoader fileLoader;
    SnapshotStorage snapshotStorage;
    Oryol::TimePoint curTime;
    Oryol::Array<Oryol::Ptr<WindowBase>> windows;
    bool uiEnabled = false;
    bool helpOpen = true;
};
