#ifndef MODEL_HPP
#define MODEL_HPP
#include <gui/model/FrequencyParameters.hpp>
class ModelListener;
// Read-only SWD snapshot of UI state; never controls the output hardware.
struct FrequencyUiDiagnostics {
    uint32_t values[3];
    uint32_t selected_channel;
    uint32_t refreshes;
    uint32_t confirm_calls;
    uint32_t saved;
    uint32_t rejected;
};
extern volatile FrequencyUiDiagnostics frequency_ui_diag;
class Model {
public:
    Model() : modelListener(0) {}
    void bind(ModelListener* listener) { modelListener = listener; }
    void tick() {}
    FrequencyParameters& frequency() { return parameters; }
    const FrequencyParameters& frequency() const { return parameters; }
protected:
    ModelListener* modelListener;
private:
    FrequencyParameters parameters;
};
#endif
