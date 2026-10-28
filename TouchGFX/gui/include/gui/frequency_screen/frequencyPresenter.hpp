#ifndef FREQUENCYPRESENTER_HPP
#define FREQUENCYPRESENTER_HPP
#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>
class frequencyView;
class frequencyPresenter : public touchgfx::Presenter, public ModelListener {
public:
    explicit frequencyPresenter(frequencyView& v) : view(v) {}
    void activate() override {}
    void deactivate() override {}
    FrequencyParameters& parameters() { return model->frequency(); }
private:
    frequencyView& view;
};
#endif
