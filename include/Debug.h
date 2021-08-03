#pragma once
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_helper.h>
#include <BluePrint.h>
#include <Node.h>

namespace BluePrint
{
struct BluePrintUI;
struct DebugOverlay:
    private ContextMonitor
{
    DebugOverlay() {}
    DebugOverlay(BP* blueprint);
    ~DebugOverlay();

    void Init(BP* blueprint);
    void Begin();
    void End();

    void DrawNode(BluePrintUI* ui, const Node& node);
    void DrawInputPin(BluePrintUI* ui, const Pin& pin);
    void DrawOutputPin(BluePrintUI* ui, const Pin& pin);

    ContextMonitor* GetContextMonitor();
    
private:
    void OnDone(Context& context) override;
    void OnPause(Context& context) override;
    void OnResume(Context& context) override;
    void OnStepNext(Context& context) override;
    void OnStepCurrent(Context& context) override;

    BP* m_Blueprint {nullptr};
    const Node* m_CurrentNode {nullptr};
    const Node* m_NextNode {nullptr};
    FlowPin m_CurrentFlowPin;
    ImDrawList* m_DrawList {nullptr};
    ImDrawListSplitter m_Splitter;
};

}