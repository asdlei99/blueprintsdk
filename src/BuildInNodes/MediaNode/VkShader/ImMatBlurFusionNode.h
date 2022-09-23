#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Box.h>
#include <AlphaBlending_vulkan.h>
#include <Brightness_vulkan.h>

namespace BluePrint
{
struct BlurFusionNode final : Node
{
    BP_NODE_WITH_NAME(BlurFusionNode, "Blur Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video")
    BlurFusionNode(BP& blueprint): Node(blueprint) { m_Name = "Blur Transform"; }

    ~BlurFusionNode()
    {
        if (m_blur) { delete m_blur; m_blur = nullptr; }
        if (m_alpha) { delete m_alpha; m_alpha = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
    }

    void OnStop(Context& context) override
    {
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        auto percentage = context.GetPinValue<float>(m_Blur);
        float alpha = 1.0f - percentage;
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_blur || m_device != gpu)
            {
                if (m_blur) { delete m_blur; m_blur = nullptr; }
                m_blur = new ImGui::BoxBlur_vulkan(gpu);
            }
            if (!m_alpha || m_device != gpu)
            {
                if (m_alpha) { delete m_alpha; m_alpha = nullptr; }
                m_alpha = new ImGui::AlphaBlending_vulkan(gpu);
            }
            if (!m_blur || !m_alpha)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            ImGui::VkMat im_First_Blur; im_First_Blur.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            ImGui::VkMat im_Second_Blur; im_Second_Blur.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_second.type : m_mat_data_type;

            int size_first = percentage * 50 + 1;
            int size_second = (1.0 - percentage) * 50 + 1;
            
            double node_time = 0;
            m_blur->SetParam(size_first, size_first);
            node_time += m_blur->filter(mat_first, im_First_Blur);
            m_blur->SetParam(size_second, size_second);
            node_time += m_blur->filter(mat_second, im_Second_Blur);
            node_time += m_alpha->blend(im_First_Blur, im_Second_Blur, im_RGB, alpha);
            m_NodeTimeMs = node_time;

            im_RGB.time_stamp = mat_first.time_stamp;
            im_RGB.rate = mat_first.rate;
            im_RGB.flags = mat_first.flags;
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    bool CustomLayout() const override { return false; }
    bool Skippable() const override { return true; }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatInFirst   = { this, "In 1" };
    MatPin    m_MatInSecond   = { this, "In 2" };
    FloatPin  m_Blur = { this, "Blur" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Blur };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    ImGui::BoxBlur_vulkan * m_blur   {nullptr};
    ImGui::AlphaBlending_vulkan * m_alpha   {nullptr};
};
} // namespace BluePrint
