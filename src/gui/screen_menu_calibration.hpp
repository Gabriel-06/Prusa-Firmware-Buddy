/**
 * @file screen_menu_calibration.hpp
 */
#pragma once

#include "screen_menu.hpp"
#include "WindowMenuItems.hpp"
#include "MItem_tools.hpp"
#include "MItem_basic_selftest.hpp"
#include "MItem_print.hpp"
#include "printers.h"

using ScreenMenuCalibration__ = ScreenMenu<EFooter::On, MI_RETURN, MI_WIZARD, MI_LIVE_ADJUST_Z, MI_AUTO_HOME, MI_MESH_BED,
    MI_SELFTEST, MI_CALIB_FIRST, MI_TEST_FANS, MI_TEST_XYZ, MI_TEST_HEAT,
#ifdef _DEBUG
    MI_ADVANCED_FAN_TEST,
#endif
    MI_SELFTEST_RESULT>;

class ScreenMenuCalibration : public ScreenMenuCalibration__ {
public:
    constexpr static const char *label = N_("CALIBRATION");
    ScreenMenuCalibration();
};
