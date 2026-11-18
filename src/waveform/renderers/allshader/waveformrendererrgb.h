#pragma once

#include <QColor>
#include <memory>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

namespace allshader {
class WaveformRendererRGB;
} // namespace allshader

class ControlProxy;

class allshader::WaveformRendererRGB final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRendererRGB(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play,
            ::WaveformRendererSignalBase::Options options =
                    ::WaveformRendererSignalBase::Option::None);

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    bool supportsSlip() const override {
        return true;
    }

    bool onInit() override;

    // Virtuals for rendergraph::Node
    void preprocess() override;

    /// Colors for the signal inside an enabled loop; the loop is not highlighted while any is invalid.
    void setLoopColors(const QColor& low, const QColor& mid, const QColor& high);

  private:
    bool m_isSlipRenderer;
    ::WaveformRendererSignalBase::Options m_options;

    bool m_loopColorsEnabled{false};
    float m_loopLowColor[3]{};
    float m_loopMidColor[3]{};
    float m_loopHighColor[3]{};
    std::unique_ptr<ControlProxy> m_pLoopStart;
    std::unique_ptr<ControlProxy> m_pLoopEnd;
    std::unique_ptr<ControlProxy> m_pLoopEnabled;

    bool preprocessInner();

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererRGB);
};
