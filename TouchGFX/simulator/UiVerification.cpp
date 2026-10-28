// Optional integration check, invoked only with SENDER_UI_VERIFY=1.
#include <gui/common/FrontendApplication.hpp>
#include <platform/hal/simulator/sdl2/HALSDL2.hpp>
#include <touchgfx/events/ClickEvent.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>

static void finish(bool passed)
{
    FILE* f = fopen("verification/ui_result.txt", "w");
    if (f) { fprintf(f, "%s: TouchGFX clicks, independent channels, range validation and cancel\n", passed ? "PASS" : "FAIL"); fclose(f); }
    exit(passed ? 0 : 1);
}
void FrontendApplication::runVerification()
{
    if (!getenv("SENDER_UI_VERIFY")) return;
    static unsigned tick = 0;
    ++tick;
    const auto click = [this](int x, int y) {
        touchgfx::ClickEvent press(touchgfx::ClickEvent::PRESSED, x, y);
        touchgfx::ClickEvent release(touchgfx::ClickEvent::RELEASED, x, y);
        handleClickEvent(press); handleClickEvent(release);
    };
    const auto screenshot = [](const char* name) {
        // Capture the actual RGB565 framebuffer; a hidden SDL window is black.
        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
            touchgfx::HAL::getInstance()->getTFTFrameBuffer(), 480, 272, 16,
            480 * 2, 0xf800, 0x07e0, 0x001f, 0);
        char filename[200];
        snprintf(filename, sizeof filename, "screenshots/%s", name);
        if (surface) { SDL_SaveBMP(surface, filename); SDL_FreeSurface(surface); }
    };
    switch(tick) {
    case 5: screenshot("01_initial.bmp"); break;
    case 10: click(35,130); break; // D2
    case 14: click(327,112); break; // 2
    case 18: click(327,151); break; // 5
    case 22: case 26: click(327,230); break; // 00
    case 30: click(440,210); break; // Confirm
    case 35:
        if (model.frequency().get(0)!=1000 || model.frequency().get(1)!=2500 || model.frequency().get(2)!=1000) finish(false);
        screenshot("02_d2_saved.bmp"); break;
    case 40: click(270,230); break; // Clear
    case 44: case 48: click(382,190); break; // 99
    case 52: click(440,210); break;
    case 58:
        if (model.frequency().get(1)!=2500) finish(false);
        screenshot("03_invalid_range.bmp"); break;
    case 62: click(440,130); break; // Cancel
    case 68:
        if (strcmp(model.frequency().draft(),"2500")!=0) finish(false);
        screenshot("04_cancelled.bmp"); break;
    case 72: click(35,190); break; // D3
    case 76: click(270,112); break; // 1
    case 80: case 84: case 88: case 92: click(327,230); break;
    case 96: click(440,210); break;
    case 100:
        if (model.frequency().get(2)!=10000 || model.frequency().get(1)!=2500) finish(false);
        screenshot("05_d3_saved.bmp"); break;
    case 104: finish(true); break;
    }
}
