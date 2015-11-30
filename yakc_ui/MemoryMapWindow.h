#pragma once
//------------------------------------------------------------------------------
/**
    @class MemoryMapWindow
    @brief visualize the current memory map configuration
*/
#include "yakc_ui/WindowBase.h"

class MemoryMapWindow : public WindowBase {
    OryolClassDecl(MemoryMapWindow);
public:
    /// setup the window
    virtual void Setup(yakc::kc85& kc) override;
    /// draw method
    virtual bool Draw(yakc::kc85& kc) override;

    struct pageInfo {
        const char* name = nullptr;
        unsigned int addr = 0;
        unsigned int size = 0;
    };

    /// get name for a memory layer and page
    pageInfo getPageInfo(yakc::kc85& kc, int layer_index, int page_index) const;
};
