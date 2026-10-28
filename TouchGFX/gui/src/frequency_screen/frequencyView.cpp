#include <gui/frequency_screen/frequencyView.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <touchgfx/Color.hpp>
#ifndef SIMULATOR
#include "sender_pwm.h"
#endif
frequencyView::frequencyView() : savedBuffers{}, draftBuffer{} {
    frequency1.setWildcard(savedBuffers[0]);
    frequency2.setWildcard(savedBuffers[1]);
    frequency3.setWildcard(savedBuffers[2]);
    editValue.setWildcard(draftBuffer);
}
void frequencyView::setupScreen() {
    frequencyViewBase::setupScreen();
    presenter->parameters().cancel();
    refresh(T_STATUS_READY);
}
void frequencyView::refresh(uint16_t statusText) {
    const FrequencyParameters& p = presenter->parameters();
    ++frequency_ui_diag.refreshes;
    frequency_ui_diag.selected_channel = p.selected();
    for (unsigned i = 0; i < 3; ++i) frequency_ui_diag.values[i] = p.get(i);
    const auto orange = touchgfx::Color::getColorFromRGB(240, 163, 64);
    const auto ink = touchgfx::Color::getColorFromRGB(248, 246, 241);
    const auto border = touchgfx::Color::getColorFromRGB(68, 72, 76);
    const auto key = touchgfx::Color::getColorFromRGB(48, 53, 58);
    const auto dark = touchgfx::Color::getColorFromRGB(21, 23, 24);
    decltype(channel1)* buttons[] = {&channel1, &channel2, &channel3};
    for (unsigned i = 0; i < 3; ++i) {
        buttons[i]->setTextColors(i == p.selected() ? orange : ink, dark);
        buttons[i]->setBoxWithBorderColors(key, orange, i == p.selected() ? orange : border, orange);
        buttons[i]->invalidate();
    }
    for (unsigned i = 0; i < 3; ++i)
        touchgfx::Unicode::snprintf(savedBuffers[i], 8, "%u", static_cast<unsigned>(p.get(i)));
    touchgfx::Unicode::strncpy(draftBuffer, p.draft(), 8);
    static const uint16_t channelTexts[] = {T_CHANNEL_D1, T_CHANNEL_D2, T_CHANNEL_D3};
    frequency1.invalidate(); frequency2.invalidate(); frequency3.invalidate();
    editValue.invalidate();
    editChannel.setTypedText(touchgfx::TypedText(channelTexts[p.selected()]));
    editChannel.invalidate();
    selectedBar.invalidate(); selectedBar.setY(48 + p.selected() * 59); selectedBar.invalidate();
    status.setTypedText(touchgfx::TypedText(statusText));
    status.setColor(statusText == T_STATUS_SAVED ? orange :
        statusText == T_STATUS_RANGE ? touchgfx::Color::getColorFromRGB(255, 182, 101) :
        touchgfx::Color::getColorFromRGB(184, 180, 168));
    status.invalidate();
}
void frequencyView::selectChannel(unsigned channel) {
    presenter->parameters().select(channel); refresh(T_STATUS_READY);
}
void frequencyView::enterDigit(unsigned digit) {
    presenter->parameters().digit(digit); refresh(T_STATUS_DRAFT);
}
void frequencyView::clearInput() {
    presenter->parameters().clear(); refresh(T_STATUS_DRAFT);
}
void frequencyView::backspaceInput() {
    presenter->parameters().backspace(); refresh(T_STATUS_DRAFT);
}
void frequencyView::cancelInput() {
    presenter->parameters().cancel(); refresh(T_STATUS_CANCELLED);
}
void frequencyView::confirmInput() {
    const bool saved = presenter->parameters().commit();
#ifndef SIMULATOR
    // Apply only confirmed values; draft typing leaves the physical PWM unchanged.
    if (saved) SenderPwm_SetFrequency(presenter->parameters().selected(),
                                     presenter->parameters().get(presenter->parameters().selected()));
#endif
    ++frequency_ui_diag.confirm_calls;
    if (saved) ++frequency_ui_diag.saved;
    else ++frequency_ui_diag.rejected;
    refresh(saved ? T_STATUS_SAVED : T_STATUS_RANGE);
}
