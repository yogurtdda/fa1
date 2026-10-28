#ifndef FREQUENCYVIEW_HPP
#define FREQUENCYVIEW_HPP
#include <gui_generated/frequency_screen/frequencyViewBase.hpp>
#include <gui/frequency_screen/frequencyPresenter.hpp>
class frequencyView : public frequencyViewBase {
public:
    frequencyView();
    void setupScreen() override;
    void tearDownScreen() override {}
    void selectD1() override { selectChannel(0); }
    void selectD2() override { selectChannel(1); }
    void selectD3() override { selectChannel(2); }
    void digit0() override { enterDigit(0); }
    void digit1() override { enterDigit(1); }
    void digit2() override { enterDigit(2); }
    void digit3() override { enterDigit(3); }
    void digit4() override { enterDigit(4); }
    void digit5() override { enterDigit(5); }
    void digit6() override { enterDigit(6); }
    void digit7() override { enterDigit(7); }
    void digit8() override { enterDigit(8); }
    void digit9() override { enterDigit(9); }
    void clearInput() override;
    void backspaceInput() override;
    void cancelInput() override;
    void confirmInput() override;
private:
    touchgfx::Unicode::UnicodeChar savedBuffers[3][8];
    touchgfx::Unicode::UnicodeChar draftBuffer[8];
    void selectChannel(unsigned channel);
    void enterDigit(unsigned digit);
    void refresh(uint16_t statusText);
};
#endif
