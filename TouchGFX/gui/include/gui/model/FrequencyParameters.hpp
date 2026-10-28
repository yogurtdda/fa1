#ifndef FREQUENCY_PARAMETERS_HPP
#define FREQUENCY_PARAMETERS_HPP
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Parameter storage only: no peripheral writes, interrupts, or allocation.
class FrequencyParameters
{
public:
    static const uint32_t MIN_HZ = 100, MAX_HZ = 10000;
    static const uint8_t CHANNELS = 3, MAX_DIGITS = 5;
    FrequencyParameters() : selected_(0), replaceOnDigit_(true)
    {
        for (unsigned i = 0; i < CHANNELS; ++i) values_[i] = 1000;
        load();
    }
    uint32_t get(unsigned channel) const { return channel < CHANNELS ? values_[channel] : 0; }
    uint8_t selected() const { return selected_; }
    const char* draft() const { return draft_; }
    bool select(unsigned channel)
    {
        if (channel >= CHANNELS) return false;
        selected_ = static_cast<uint8_t>(channel);
        load(); // Discard unconfirmed input on channel switch.
        return true;
    }
    bool digit(unsigned value)
    {
        if (value > 9) return false;
        if (replaceOnDigit_) draft_[0] = 0;
        replaceOnDigit_ = false;
        unsigned n = static_cast<unsigned>(strlen(draft_));
        if (n == 1 && draft_[0] == '0') n = 0;
        if (n >= MAX_DIGITS) return false;
        draft_[n] = static_cast<char>('0' + value);
        draft_[n + 1] = 0;
        return true;
    }
    void clear() { draft_[0] = 0; replaceOnDigit_ = false; }
    void backspace()
    {
        unsigned n = static_cast<unsigned>(strlen(draft_));
        if (n) draft_[n - 1] = 0;
        replaceOnDigit_ = false;
    }
    void cancel() { load(); }
    bool commit()
    {
        if (draft_[0] == 0) return false;
        uint32_t value = 0;
        for (const char* p = draft_; *p; ++p) value = value * 10 + static_cast<unsigned>(*p - '0');
        if (value < MIN_HZ || value > MAX_HZ) return false;
        values_[selected_] = value;
        load();
        return true;
    }
private:
    uint32_t values_[CHANNELS];
    uint8_t selected_;
    bool replaceOnDigit_;
    char draft_[MAX_DIGITS + 1];
    void load()
    {
        snprintf(draft_, sizeof draft_, "%lu", static_cast<unsigned long>(values_[selected_]));
        replaceOnDigit_ = true;
    }
};
#endif
