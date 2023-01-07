#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Rolls_vulkan.h>

namespace BluePrint
{
struct RollsFusionNode final : Node
{
    BP_NODE_WITH_NAME(RollsFusionNode, "Rolls Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    RollsFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Rolls Transform"; }

    ~RollsFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
        if (m_logo) { ImGui::ImDestroyTexture(m_logo); m_logo = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        int x = 0, y = 0;
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float progress = context.GetPinValue<float>(m_Pos);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_fusion || m_device != gpu)
            {
                if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
                m_fusion = new ImGui::Rolls_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_RotDown, m_roll_type);
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

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        int _type = m_roll_type;
        int _down = m_RotDown ? 1 : 0;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(100, 8));
        ImGui::PushItemWidth(100);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderInt("Type", &_type, 0, 4);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_type##Rolls")) { _type = 0; changed = true; }
        ImGui::RadioButton("Roll Up", &_down, 0); ImGui::SameLine();
        ImGui::RadioButton("Roll Down", &_down, 1);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_updown##Rolls")) { _down = 0; changed = true; }
        if (_type != m_roll_type) { m_roll_type = _type; changed = true; }
        if ((m_RotDown && _down != 1) || (!m_RotDown && _down != 0)) { m_RotDown = _down == 1; changed = true; };
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        return changed;
    }

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
        if (value.contains("RotDown"))
        { 
            auto& val = value["RotDown"];
            if (val.is_boolean())
                m_RotDown = val.get<imgui_json::boolean>();
        }
        if (value.contains("roll_type"))
        { 
            auto& val = value["roll_type"];
            if (val.is_number())
                m_roll_type = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["RotDown"] = imgui_json::boolean(m_RotDown);
        value["roll_type"] = imgui_json::number(m_roll_type);
    }

    void load_logo() const
    {
        int width = 0, height = 0, component = 0;
        if (auto data = stbi_load_from_memory((stbi_uc const *)logo_data, logo_size, &width, &height, &component, 4))
        {
            m_logo = ImGui::ImCreateTexture(data, width, height);
        }
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        // if show icon then we using u8"\ue882"
        if (!m_logo)
        {
            load_logo();
        }
        if (m_logo)
        {
            int logo_col = (m_logo_index / 4) % 4;
            int logo_row = (m_logo_index / 4) / 4;
            float logo_start_x = logo_col * 0.25;
            float logo_start_y = logo_row * 0.25;
            ImGui::Image(m_logo, size, ImVec2(logo_start_x, logo_start_y),  ImVec2(logo_start_x + 0.25f, logo_start_y + 0.25f));
            m_logo_index++; if (m_logo_index >= 64) m_logo_index = 0;
        }
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
    FloatPin  m_Pos = { this, "Pos" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    int m_roll_type     {0};
    bool m_RotDown      {false};
    ImGui::Rolls_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 5546;
    const unsigned int logo_data[5548/4] =
{
    0xe0ffd8ff, 0x464a1000, 0x01004649, 0x01000001, 0x00000100, 0x8400dbff, 0x07070a00, 0x0a060708, 0x0b080808, 0x0e0b0a0a, 0x0d0e1018, 0x151d0e0d, 
    0x23181116, 0x2224251f, 0x2621221f, 0x262f372b, 0x21293429, 0x31413022, 0x3e3b3934, 0x2e253e3e, 0x3c434944, 0x3e3d3748, 0x0b0a013b, 0x0e0d0e0b, 
    0x1c10101c, 0x2822283b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 
    0x3b3b3b3b, 0x3b3b3b3b, 0xc0ff3b3b, 0x00081100, 0x03000190, 0x02002201, 0x11030111, 0x01c4ff01, 0x010000a2, 0x01010105, 0x00010101, 0x00000000, 
    0x01000000, 0x05040302, 0x09080706, 0x00100b0a, 0x03030102, 0x05030402, 0x00040405, 0x017d0100, 0x04000302, 0x21120511, 0x13064131, 0x22076151, 
    0x81321471, 0x2308a191, 0x15c1b142, 0x24f0d152, 0x82726233, 0x17160a09, 0x251a1918, 0x29282726, 0x3635342a, 0x3a393837, 0x46454443, 0x4a494847, 
    0x56555453, 0x5a595857, 0x66656463, 0x6a696867, 0x76757473, 0x7a797877, 0x86858483, 0x8a898887, 0x95949392, 0x99989796, 0xa4a3a29a, 0xa8a7a6a5, 
    0xb3b2aaa9, 0xb7b6b5b4, 0xc2bab9b8, 0xc6c5c4c3, 0xcac9c8c7, 0xd5d4d3d2, 0xd9d8d7d6, 0xe3e2e1da, 0xe7e6e5e4, 0xf1eae9e8, 0xf5f4f3f2, 0xf9f8f7f6, 
    0x030001fa, 0x01010101, 0x01010101, 0x00000001, 0x01000000, 0x05040302, 0x09080706, 0x00110b0a, 0x04020102, 0x07040304, 0x00040405, 0x00770201, 
    0x11030201, 0x31210504, 0x51411206, 0x13716107, 0x08813222, 0xa1914214, 0x2309c1b1, 0x15f05233, 0x0ad17262, 0xe1342416, 0x1817f125, 0x27261a19, 
    0x352a2928, 0x39383736, 0x4544433a, 0x49484746, 0x5554534a, 0x59585756, 0x6564635a, 0x69686766, 0x7574736a, 0x79787776, 0x8483827a, 0x88878685, 
    0x93928a89, 0x97969594, 0xa29a9998, 0xa6a5a4a3, 0xaaa9a8a7, 0xb5b4b3b2, 0xb9b8b7b6, 0xc4c3c2ba, 0xc8c7c6c5, 0xd3d2cac9, 0xd7d6d5d4, 0xe2dad9d8, 
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xa9a22820, 0x00fff76a, 0xb76cbad9, 
    0x18756923, 0x760bdd40, 0xc095dda4, 0xda7345bb, 0x6fae881f, 0x7bcbbeef, 0x4d177114, 0xb952edd1, 0x57fadcef, 0x52324a43, 0x51144057, 0x5d135045, 
    0xee1fad7f, 0xc92d35d4, 0x0c7ac4cc, 0x15955554, 0xfc98686a, 0xa8a22828, 0xe99a1a6b, 0x9b722656, 0xb9aa9dd8, 0xdd684dc6, 0x8545d995, 0xfc79b3be, 
    0xa585ba85, 0x90581ec9, 0x31eac816, 0xad9fd753, 0x1a37f6ac, 0xb5cffc7d, 0x021265ee, 0xd491017a, 0x7a953ffe, 0xe5244405, 0x288ac2b8, 0xba5b04ad, 
    0x7a06446c, 0x2e56a99a, 0xe8abcccf, 0x9feb5533, 0x44531b0d, 0x51e85e53, 0x75d65045, 0xf734d325, 0x4ff3709b, 0x933ca911, 0x7665b75b, 0xf1564759, 
    0xd96d5a4c, 0x3b734bb6, 0xf6f9dfaf, 0x3547fee0, 0x71cb04b3, 0x08ca1c6f, 0x07301459, 0xb8d20f1d, 0x9fb4746b, 0x9ebcbd48, 0x97be36ea, 0x078c6423, 
    0xd7034970, 0x0b6fbad2, 0x5068e7cd, 0x128afb82, 0xf09c8ea7, 0x13ca2a3f, 0x9a8d5a6e, 0x6c4551f4, 0xb6cdf422, 0xaa18f726, 0x33e46e35, 0x8c5c852a, 
    0x5ca98af3, 0x2e944498, 0xa2604f67, 0x5faca38a, 0xc9a79f3e, 0xaf34713c, 0xa7a3008c, 0xba02f4b9, 0xa1b9b29b, 0x5e2de299, 0xcb749154, 0x42eae202, 
    0x43c795a0, 0xa5c71ed8, 0x6e2f8527, 0x6fd53eee, 0x98923475, 0x9e5b0a08, 0x679ce7a4, 0xa79535f3, 0x2265aba6, 0xedc7a66a, 0xf29c71c2, 0xdce33ace, 
    0x8cb11d74, 0x29e1b70a, 0xf4a5facc, 0xfce62bb8, 0x323072e5, 0x5a00ffc4, 0xa6b949c1, 0x519475c6, 0x28087445, 0x2b008aa2, 0xf085f191, 0x892cb86b, 
    0xf2302d6f, 0xfd3fdf01, 0x755de76f, 0x41a24367, 0x91faac16, 0x7c265996, 0xcfc0d8f0, 0x549c1ea7, 0x212b394d, 0xd5bf92a3, 0xb5525fed, 0x92e2d3bc, 
    0xca0a0003, 0xa7031940, 0x77c5e943, 0xc293ca70, 0x8e72c692, 0x06f581a1, 0xf874d4a0, 0x6d433b75, 0x82a46039, 0x7a04f50a, 0x166dac53, 0x643bcec6, 
    0x1863dd91, 0x7e9cf105, 0x1617a314, 0x1445b1c0, 0x98248256, 0xd72366e6, 0x7b591d15, 0xac2c6666, 0xa3e63939, 0x800ed236, 0x7aaea11f, 0xa5f2a975, 
    0x6da31472, 0xf195abc8, 0xa69a172c, 0xbc9d05a9, 0x73d01812, 0x71b35321, 0x0a3b4eee, 0x01328ceb, 0xe6f08792, 0x64945198, 0x5a511fa9, 0x26ab1937, 
    0xf5e2685a, 0x8f5a338d, 0xae2ff34c, 0x0c82e5ed, 0x1dc1ab10, 0xad47f9b8, 0xef1d7a74, 0x6598f4db, 0x6303e727, 0x761547fd, 0x239e6258, 0x88acd114, 
    0x418655dd, 0xb72218a2, 0x6b0c458c, 0xa38a0e1a, 0x72185100, 0x451fe3ca, 0x2c825614, 0x268f14cf, 0x7b3b32e0, 0xa38c7255, 0x8afa4825, 0xc8bca2d0, 
    0x9514a562, 0xad51758e, 0xd5cbea0c, 0x69b538b4, 0xdd499262, 0x8300ac7c, 0x7d9e711b, 0x34d115f9, 0x1455b751, 0xc3166dd3, 0xe0c9e18d, 0x63f881e7, 
    0x15ebd6fa, 0x34d24a4e, 0x5ecf5155, 0xb94c77f8, 0x24daca8d, 0x8c2aec72, 0x630f5099, 0x7ca8c181, 0x6175672f, 0xdcbd2d67, 0x9b79531e, 0x8c1ce494, 
    0x74a54f0f, 0xeee36a2d, 0x89366a90, 0xc74339d7, 0x53196db5, 0x6a59bc93, 0x8aa28d49, 0x349a612b, 0xef1f8dd6, 0x2bea3428, 0x51ad4dc2, 0x5f6b57c6, 
    0x69e037ee, 0xecebc6a6, 0x79f671ef, 0x6938041a, 0xea831242, 0xf3a30e7a, 0xa2b555ab, 0x45e396f8, 0x822178b6, 0xf2be0339, 0x03f49c5c, 0x3c5557fa, 
    0xa9c5d754, 0xa96b6aac, 0x964d56ca, 0x89b52c86, 0xe262d6ef, 0x07567e50, 0x394f9e2b, 0xebaef2e7, 0xb56dd553, 0x224dc640, 0x715b39d2, 0xad0c63f3, 
    0x119cd493, 0x5873fdb8, 0xdd476a6d, 0xe91afb6c, 0x7a528a58, 0x95487b9a, 0x3232a7e8, 0x36c5301c, 0x5d4d53ba, 0xb8365816, 0x957e3219, 0xb4e2f55c, 
    0x5c21771d, 0xc9a922e3, 0xc838a703, 0x87c715f5, 0x3775e5ac, 0x7d2794a9, 0x53141dd9, 0x13459065, 0x46214148, 0xfd24a370, 0x919a7605, 0x63415e5e, 
    0x7db8676e, 0x3bce3888, 0x8bbf0dd6, 0x61987bda, 0x3341713a, 0xeb65ac00, 0x5f3e8e93, 0x6fd5c95a, 0xe469b56f, 0xf64d5b9c, 0xf10a764b, 0x93314600, 
    0xbc5e0ff3, 0xbe24a9f6, 0xf4d6beb6, 0x1aaf62c3, 0x59beca46, 0xdcfc7601, 0x9e2bfde0, 0x0e7a1b55, 0x14456fc7, 0x1a8d4057, 0x335e498a, 0xb91e79cc, 
    0x232368a9, 0xb438bc06, 0x22e7aa9e, 0xed5fa96b, 0xb47eec7f, 0x8ffdaf7d, 0xb36d89d6, 0x2a7d70ca, 0x415229bb, 0x0abd2218, 0x5a54e874, 0x0fc66823, 
    0xedd5bd60, 0xdd32a0ac, 0x00ff4415, 0x07fdba69, 0xc58bc37a, 0x3c6f71cd, 0x228fa3e9, 0x29734910, 0xcb87bd1b, 0xf8aa9bd6, 0x51577d7e, 0x22bc2549, 
    0x35cb7fd8, 0x9cd3af1c, 0x4e3fcf73, 0xc3a98bb5, 0x63bae8a8, 0x72acd2e9, 0x93724ada, 0xe00ef222, 0xfdf87d70, 0xa944f17a, 0x9115bd28, 0xc34e84a2, 
    0x5253d44b, 0x69a34bb0, 0x14ceefe4, 0x8e1f5b48, 0xfb53ce2a, 0x7d00ffad, 0x3fabf57f, 0xd1c88b44, 0x1f8c50ad, 0x3ac6132c, 0xf52ae8f3, 0xdf0a4674, 
    0xd15858f0, 0xc52b8aa2, 0x32bb0639, 0xfc6eaa8c, 0xffb51f6a, 0xd5fab100, 0xe16bb38a, 0x855643cd, 0xf3b6b0e7, 0x3bd6b602, 0x4dfc79d5, 0xdb745174, 
    0xb370d1b4, 0xff12a3d1, 0xd3b65c00, 0x0a6ef360, 0xbbda41ae, 0x31ec63b9, 0x8c777a59, 0xfbbd531f, 0x029cbe3d, 0x3eef9d83, 0x6d5cdf0e, 0xb53e00ff, 
    0x2369aaa9, 0x52c392cc, 0x6ce3f9b5, 0x4ee53832, 0x3cf4833a, 0x61c5eb11, 0xb31f1e6b, 0x6c73536b, 0xba35cb3e, 0x013e1a65, 0xe33f23c7, 0xa9aef4f9, 
    0x5abaa161, 0xf638fb1b, 0x447c6d34, 0xf6b4dab7, 0x3288c722, 0x87fe5d1a, 0xfcd71fd3, 0x64b4406b, 0x1a7d84f0, 0xc5066fb9, 0xf3dd25e6, 0x2649d290, 
    0xa73e70d2, 0x5de9cff3, 0x9c4a6b35, 0x36bf6e54, 0x34b6543e, 0xf28aa268, 0x48a6400e, 0x5d7017e5, 0xfa34bfc3, 0xeaa161bf, 0x2d2da43a, 0x8a9ab79d, 
    0xabde49db, 0x7a9ff883, 0xc6b5efa8, 0xf19e31b7, 0x83483182, 0xd8021c30, 0x5871fd04, 0xf2c2f812, 0xf4491edd, 0x378e2329, 0x74e7ed0a, 0x7ac700ff, 
    0xaef7f47b, 0x9ef050d3, 0xf76e65b1, 0x89b05852, 0xab6b5f06, 0x04b81e6d, 0x09abfcf1, 0x474664d1, 0x0cc3ca50, 0x5e1164a4, 0x6a4aa182, 0x143a56f1, 
    0xbeced823, 0xd0367cf1, 0xcb163c5b, 0x989ef673, 0xb91e6897, 0xf89e7f24, 0x1d315fad, 0x31766841, 0xfe6f851c, 0x3a1671b5, 0x97f05f64, 0xe2c610bc, 
    0xcfdf760b, 0x7564e0f0, 0xfad7e9f7, 0x4a5769d7, 0xdfde5594, 0x56245736, 0x51148dc6, 0x47c8515e, 0xe3fc0824, 0x3450d407, 0x7bb3f77b, 0x5bdeadd8, 
    0xbaabd878, 0x3ce3ed0c, 0xbf63a08e, 0x7fbb924a, 0xc55bdc04, 0xd7a45ca4, 0x635ac611, 0xfb01ee80, 0xd6a8baa2, 0x1aba5d9c, 0x8fbd93d3, 0x80010037, 
    0xfa175831, 0x16c4c72c, 0xa8d1f6d7, 0x44f3708b, 0x4ece5678, 0xe7f5fa58, 0xf8c37abd, 0xa4484537, 0x37719d7b, 0x36593b6d, 0x8cd407a8, 0xae38fd9c, 
    0xc9dbe636, 0xfc89459a, 0x598895d8, 0xec70e336, 0x15bbda71, 0xe8c94e48, 0x82dcafcd, 0x81ae288a, 0x4551349a, 0x57182778, 0x169de15b, 0x27d05ac7, 
    0xe51df286, 0xc12d6f67, 0x062821c3, 0x389dfe41, 0x5d25571c, 0x2d6e82bf, 0x522ed2e2, 0x2de3886b, 0x0077c031, 0x535ad1fd, 0x29cd9576, 0xac29477c, 
    0xd2553f78, 0xf6c59401, 0x6532709b, 0x046f6784, 0x18a38e9c, 0x35a7d3eb, 0x54faeac9, 0x85a0bd7a, 0x6eab90dd, 0x088e1d56, 0xf59adbe7, 0xd34c1bcd, 
    0xeade09f4, 0x41396e4d, 0x14823652, 0x3527237e, 0xcea26bc5, 0x6d9e55f3, 0x1282153e, 0xda554540, 0xc01903b8, 0x8eee9ae9, 0x866a3521, 0x616ec9dc, 
    0xde8210c1, 0x28a29738, 0x9454f851, 0x184ea5ac, 0x5a57d260, 0xd128b4b5, 0xc22b8aa2, 0xcfbbc238, 0x7b740202, 0x9c090e90, 0x15f9fee3, 0xe06fd7c1, 
    0xb4788b9b, 0xe29a948b, 0x704ccb38, 0x743fc01d, 0x23fe9456, 0x5c105f4a, 0x426d36f8, 0x6fd557da, 0x17e5e25e, 0x1b100ff7, 0x7a0e5e87, 0xc771908c, 
    0xe73519b1, 0x41729657, 0x70b79624, 0xe82a2395, 0x567aade3, 0xfa69a68d, 0x2675ef04, 0xa9a01cb7, 0x3f0a411b, 0xe49a9311, 0x0e7943fc, 0x71cfabab, 
    0x88299f12, 0x38c1480a, 0xc274cd18, 0x7b92a6bb, 0xa572b91a, 0xb2b29673, 0xc8834f83, 0xe4b152b7, 0x8f130b90, 0x9254acce, 0x723c63c2, 0x771db5be, 
    0xbc9251c2, 0xea3b2976, 0x1545d168, 0x5d819ce1, 0x3a0181e7, 0x0407c83d, 0x00ff71ce, 0xebe08a7c, 0xc54df0b7, 0xca455abc, 0x651c714d, 0xe00e38a6, 
    0x4a2bba1f, 0x2fa5117f, 0x6cd2d888, 0x293e3df5, 0xdef4fada, 0x1d440192, 0xadcf3d46, 0x6ead7679, 0xdf707fc9, 0xef3ffb67, 0x8f719818, 0x0338272f, 
    0x1bedbbd2, 0x09f4d34c, 0x6e4deade, 0x36524139, 0x237e1482, 0x6bca3527, 0x5a6af1f7, 0x8250f7cc, 0x92026223, 0x19034830, 0x4e4755ad, 0x564a1acd, 
    0x88bce648, 0x3f00ffbc, 0x6e375fcb, 0x1de7f8dd, 0x587d9a71, 0x74b90d9e, 0x951e771c, 0x732abd5e, 0x69d1d58c, 0x1a8daea6, 0x43bca228, 0x8aa22890, 
    0xe6a82800, 0x815ccc8b, 0x9d950ef3, 0x8053bc35, 0x14aec4b3, 0xc11906e4, 0x15549aae, 0x23cd7445, 0x8ca62e05, 0x8cf32390, 0xde55511f, 0xdcc04307, 
    0x7a71453d, 0x24d1a1fd, 0x3c098b6b, 0x8e01afa8, 0xd3713048, 0x6f8feda8, 0x7c3690e9, 0x742c6037, 0xf27e2494, 0x3e31de6f, 0xea4a3ffd, 0x97911aa5, 
    0x5a94cd2d, 0x51f474ea, 0x28877945, 0x00144551, 0xb8544551, 0x1d6f6b8f, 0x6bad5f0f, 0xe5720a4a, 0x5d45c56e, 0xaecab4d8, 0xaa22c330, 0xcacb6ec9, 
    0xacf5c3fc, 0x5bdb106f, 0xfdd368dc, 0x2911eda0, 0x0ce36c74, 0x9ce7c701, 0xe187357e, 0x0b75397b, 0xdcaed4d6, 0x29c222ca, 0x01580e70, 0x1583be8e, 
    0xa5730ad9, 0xe3cd542e, 0x819eba06, 0x9c571445, 0x51140573, 0x53140540, 0x0a314f64, 0x4dd254fe, 0x197108ea, 0x5b563518, 0x6432bf7d, 0x8c804a8f, 
    0xaee11a1c, 0xbcb43f2d, 0x666b734d, 0x9bbcb0ce, 0x649c00a6, 0xff7ae27d, 0x3a7f1600, 0x51298def, 0x9d5bc669, 0x4f5d8311, 0x2b8aa24a, 0x8ac239cf, 
    0x8a02a028, 0xab02a028, 0xe40e064d, 0x9b02771c, 0xc8649b72, 0xbe72351c, 0xed289e32, 0x79a3d0ad, 0x22c5cbfb, 0x19633690, 0xd0dbeff7, 0x6e2a7d57, 
    0x9191f611, 0x33d521bc, 0x2b8aa2b5, 0xad2bc080, 0xa68b9ef0, 0xf36c5aea, 0xae79dbde, 0x7b079592, 0xd8030c0c, 0xbbabe48a, 0x8dc629f0, 0x4c9c7172, 
    0x8000ff78, 0xb32bb88a, 0x6a39694a, 0xbcd24f33, 0xab24ae2f, 0x6d2c6f65, 0xb04b321e, 0x923a3d23, 0xaa0b573b, 0xc97b8b59, 0xe5dd56e3, 0xf74fc848, 
    0xf49a71b0, 0x875e073d, 0x485b9658, 0xa56ac3f4, 0x71132f09, 0x6b3080f9, 0x23acd78f, 0xed8975d3, 0xa4c872a2, 0x8ce4b811, 0xa95af38c, 0x2e515af2, 
    0x1c49c94e, 0x41a4a776, 0xbc4c18aa, 0x7a902f34, 0x3f823b82, 0xafd2f30f, 0xec10d7d4, 0xa77c943b, 0x5e43b5af, 0x6b463995, 0x75271a99, 0x288a4673, 
    0x0ae414af, 0xa2277ceb, 0x9b96bae9, 0xdeb6f73c, 0x41a5a46b, 0x0003c3de, 0x2ab922f6, 0x710afcee, 0x679c5ca3, 0xe03f1e13, 0xec0aae22, 0x5a4e9ad2, 
    0xaff4d38c, 0x2a89eb0b, 0x1bcb5bd9, 0xec928c47, 0xa44ecf08, 0xdfc5d58e, 0x53372c5b, 0x23de96da, 0xef709391, 0x7bd78c83, 0xebd0eba0, 0x1e69cb12, 
    0xa1546d98, 0x3f6ee225, 0x710d0630, 0x7a84f5fa, 0xb43db16e, 0x8214594e, 0x91911c37, 0xa2a56a9e, 0x593b154d, 0x36777334, 0x5b722ceb, 0x5247b9cc, 
    0x1ad433ac, 0x2bd2ce8a, 0xa1ed481b, 0x03632404, 0xadb94f3d, 0xaeb7883b, 0xeb87f7e1, 0xa3d3eb54, 0xfa8d5451, 0xc85c469a, 0x2b8aa2d1, 0xba4239c7, 
    0xbae809df, 0xcfa6a56e, 0x9ab7ed3d, 0x775029e9, 0x3dc0c0b0, 0xbb4aae88, 0x689c02bf, 0xc41927d7, 0x08f88fc7, 0x34bb82ab, 0xa39693a6, 0xc22bfd34, 
    0xb64ae2fa, 0xd1c6f256, 0x02bb24e3, 0x23a9d333, 0x70b771b5, 0xedc96b0b, 0x48116ec3, 0x07d7bbc9, 0x41efae19, 0x25d6a1d7, 0x303dd296, 0x4b42a9da, 
    0x607edcc4, 0xf5e31a0c, 0xddf408eb, 0x9c687b62, 0x6e0429b2, 0x3c232339, 0x5d2b4dd5, 0x53b93415, 0x7bcc2546, 0x7ae8785b, 0xe0ec28d5, 0x945be68a, 
    0xc666098c, 0x3070eef6, 0x3ad4592b, 0x81ea3d15, 0x510f1204, 0x9c6af85d, 0x15ddb3f1, 0x3456574e, 0xf28aa228, 0xb7ae70ce, 0x9b2e7ac2, 0xcfb369a9, 
    0xbae66d7b, 0xec1d544a, 0x620f3030, 0xefae922b, 0x351aa7c0, 0x3171c6c9, 0x2a02fee3, 0x29cdaee0, 0xcda8e5a4, 0xbef04a3f, 0x95ad92b8, 0x78b4b1bc, 
    0x8cc02ec9, 0xed48eaf4, 0x02dc6d5c, 0x707bf2da, 0x325284db, 0xc6c1f56e, 0x75d0bb6b, 0x658975e8, 0x364c8fb4, 0xf19250aa, 0x03981f37, 0x7afdb806, 
    0x58373dc2, 0x2c27da9e, 0x8e1b418a, 0x35cfc848, 0x45d74a53, 0xd1542e4d, 0x886e3a95, 0xb35973fb, 0xd170435b, 0x963f5099, 0x991e94db, 0xb5d61fc1, 
    0xb2a98caa, 0x6ac73142, 0xe94ec2ea, 0xd02785c1, 0x571445bd, 0xd6568801, 0x8d5be28b, 0x86e0d916, 0xfb0ee408, 0xd07372c9, 0xb458e90f, 0x86ad4d53, 
    0xa8a35a9b, 0x839ac793, 0x026d0921, 0x9cdce831, 0xcfcdb57e, 0xbc33b73c, 0x1c7939f3, 0x35f798e5, 0x3739141d, 0xdc9bdcb8, 0x7a100408, 0x7e2ca51a, 
    0x3da8e353, 0xfc915b0d, 0xe38c2db4, 0xc0bdabde, 0xf1d1da75, 0x37bcaef5, 0x5de94eb4, 0xa19b4f17, 0xae288a6a, 0xadad2033, 0x1ab7c417, 0x0cc1b32d, 
    0xf71dc811, 0xa0e7e492, 0x68b1d21f, 0x0d5b9ba6, 0x5147b536, 0x06358f27, 0x04da1242, 0x38b9d163, 0x9e9b6bfd, 0x79676e79, 0x39f272e6, 0x6aee31cb, 
    0x6f72283a, 0xb837b971, 0xb6884b55, 0x4ff7e136, 0xb4ea56e9, 0x0ab983b3, 0x6b1d6764, 0xa7e60ca3, 0xa7cafcee, 0xa259e87b, 0xcc982b8a, 0xf1456b2b, 
    0x6c8bc62d, 0x720443f0, 0xb9e47d07, 0xf407e839, 0xa6295aac, 0xad4dc3d6, 0xe349d451, 0x849041cd, 0xf41881b6, 0x5a3f4e6e, 0x5b9ee7e6, 0x9c79de99, 
    0xcc728ebc, 0x8a8e9a7b, 0x6edc9b1c, 0x5a15ee4d, 0xc7c130e9, 0xa8590d7e, 0x852a1cae, 0xf8de902b, 0xaaedb0ad, 0x6a83c38a, 0x45d1125a, 0x5b018115, 
    0x6e892f5a, 0x82675b34, 0x3b902318, 0xcfc925ef, 0x62a53f40, 0xb6364dd1, 0x8e6a6d1a, 0x6a1e4fa2, 0xb425840c, 0x72a3c708, 0x37d7fa71, 0xcedcf23c, 
    0xe4e5ccf3, 0xdc639673, 0xe45074d4, 0x6f72e3de, 0x49f7aa70, 0xe0761c90, 0x986c8ad5, 0x2323dbf2, 0xe3a47415, 0xd945d034, 0x2b8aa28e, 0xd8af4232, 
    0xa98e7a68, 0x6d674b0b, 0xd2b6a2e6, 0xfee0aa77, 0x57a8de27, 0x4e40e079, 0xc101728f, 0xdf7f9c33, 0xee2aaa22, 0x4e2ac2c5, 0xf0a5e6cc, 0x170ab7b6, 
    0xebc0066b, 0xe48fd5b5, 0x4110640d, 0xbd2218c1, 0xd4b3493f, 0x6ba7f8f4, 0x487ad3eb, 0x18751005, 0xe7b53ef7, 0xea90893a, 0x462c2d57, 0x8c596917, 
    0x7372f867, 0x908d728a, 0x721505e7, 0x2af4aeab, 0xd4c68a7b, 0x367d92a1, 0x797728e2, 0xb5d919ad, 0xb9dd9db6, 0x52d5d6fa, 0x7dca769d, 0x24acaef9, 
    0x932a71b5, 0x8aa25de8, 0x2ac4882b, 0xa88786fd, 0xb6b490ea, 0x2b6ade76, 0xae7a276d, 0xea7de20f, 0x049e7785, 0x20f7e804, 0xc739131c, 0xa22af2fd, 
    0x225cecae, 0x6aceeca4, 0x706b0b5f, 0x6cb076a1, 0x585dbb0e, 0x41d640fe, 0x82111c04, 0x9bf4d32b, 0x8a4f4f3d, 0x37bdbe76, 0x075180a4, 0xeb738f51, 
    0x99a8735e, 0xd272a50e, 0x957661c4, 0x877fc698, 0x28a73827, 0x51700ed9, 0x3b552b57, 0xf5898991, 0x5a95ab19, 0x7dca7cec, 0x2b6c2b46, 0xfda640b5, 
    0x5114cde2, 0x5761c65c, 0x473d34ec, 0xb3a58554, 0x5b51f3b6, 0x70d53b69, 0x54ef137f, 0x20f0bc2b, 0x00b94727, 0x3fce99e0, 0x155591ef, 0x15e16277, 
    0x52736627, 0x855b5bf8, 0x6083b50b, 0xc7eada75, 0x08b206f2, 0x118ce020, 0xd9a49f5e, 0x537c7aea, 0xbde9f5b5, 0x3a880224, 0x5a9f7b8c, 0xc8449df3, 
    0x96962b75, 0xacb40b23, 0x39fc33c6, 0x4639c539, 0x8a8273c8, 0xbaa15ab9, 0x3f5400ff, 0x2baaa9de, 0xda270c80, 0x44b58baa, 0x4b247e4c, 0x44561445, 
    0xd0b05f85, 0x16521df5, 0xcddbce96, 0xefa46d45, 0x4ffcc155, 0xf3ae50bd, 0x1e9d80c0, 0x678203e4, 0xbe00ff38, 0xdd555445, 0x9d54848b, 0xe14bcd99, 
    0x2e146e6d, 0xd7810dd6, 0xc81fab6b, 0x8320c81a, 0x7a453082, 0xa967937e, 0xd74ef1e9, 0x90f4a6d7, 0x31ea200a, 0xce6b7dee, 0xd4211375, 0x8c585aae, 
    0x18b3d22e, 0xe7e4f0cf, 0x211be514, 0xe52a0ace, 0xea9f6c6a, 0x4ed3fd9f, 0xa9fec9a6, 0x34dd00ff, 0x1622f1a3, 0x8aa2a8e3, 0xfe760591, 0x8bb7b809, 
    0xae49b948, 0xc7b48c23, 0xf703dc01, 0x55147145, 0xa8dcd945, 0xefdc95cb, 0x4f336d34, 0xa97b27d0, 0x05e5b835, 0x5108da48, 0xd79c8cf8, 0xc5dfaf29, 
    0xdd336ba9, 0x888d0842, 0x20c1480a, 0xa8b3660c, 0xc8eacaa1, 0x4856576e, 0xf0d2bd2a, 0x8a55f8af, 0x26becb64, 0xba8afa1d, 0x629ae532, 0x451fb38b, 
    0x85445614, 0xb809fe76, 0xb9488bb7, 0x8c23ae49, 0xdc01c7b4, 0x7145f703, 0xd9455514, 0x95cba8dc, 0x6d34efdc, 0x27d04f33, 0xb835a97b, 0xda4805e5, 
    0x8cf85108, 0xaf29d79c, 0x6ba9c5df, 0x0842dd33, 0x480a888d, 0x660c20c1, 0xcaa1a8b3, 0x576ec8ea, 0x2b2a4856, 0xe8b39891, 0x65d35273, 0xc630d15d, 
    0xf9a6aa78, 0x76319966, 0x8aa2a863, 0xbf5d41cc, 0xe22d6e82, 0x6b522ed2, 0x312de388, 0xfd0077c0, 0x15455cd1, 0x2a777651, 0x3b77e532, 0xd34c1bcd, 
    0xeade09f4, 0x41396e4d, 0x14823652, 0x3527237e, 0xf1f76bca, 0xf7cc5a6a, 0x62238250, 0x48309202, 0xeaac1903, 0xb2ba7228, 0x92d5951b, 0x30c3640a, 
    0xa7991eb0, 0x8d1964d3, 0x4134f480, 0xb79249da, 0x5414451d, 0xf0b72b88, 0x5abcc54d, 0x714dca45, 0x38a6651c, 0xba1fe00e, 0xaaa2882b, 0x46e5ce2e, 
    0x79e7ae5c, 0x7e9a69a3, 0x49dd3b81, 0x2a28c7ad, 0x8f42d046, 0xb9e664c4, 0x2dfe7e4d, 0xea9e594b, 0x406c4410, 0x00094652, 0x459d3563, 0x4356570e, 
    0x41b2ba72, 0x53fd934d, 0x69ba00ff, 0x6306d9d4, 0x110d3d60, 0x732b91f8, 0x0000d9ff, 
};
};
} // namespace BluePrint
