#pragma once
#include "Common.h"

namespace Aqua {

    class TaskbarController {
    public:
        static TaskbarController& Instance();

        void SuppressTaskbar();
        void RestoreTaskbar();
        void ToggleTaskbar();
        bool IsSuppressed() const { return m_suppressed; }

        void AdjustWorkArea(int topBarHeight);
        void RestoreWorkArea();

    private:
        TaskbarController();
        ~TaskbarController();

        bool m_suppressed = false;
        RECT m_originalWorkArea = {};
        bool m_hasOriginalWorkArea = false;
    };

}

