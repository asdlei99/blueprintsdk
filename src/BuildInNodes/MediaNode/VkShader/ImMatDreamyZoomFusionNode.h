#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <DreamyZoom_vulkan.h>

namespace BluePrint
{
struct DreamyZoomFusionNode final : Node
{
    BP_NODE_WITH_NAME(DreamyZoomFusionNode, "DreamyZoom Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    DreamyZoomFusionNode(BP* blueprint): Node(blueprint) { m_Name = "DreamyZoom Transform"; }

    ~DreamyZoomFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
        if (m_logo) { ImGui::ImDestroyTexture(m_logo); m_logo = nullptr; }
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
                m_fusion = new ImGui::DreamyZoom_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_rotation, m_scale);
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
        float _rotation = m_rotation;
        float _scale = m_scale;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Rotation##DreamyZoom", &_rotation, 0.f, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_rotation##DreamyZoom")) { _rotation = 6.f; changed = true; }
        ImGui::SliderFloat("Scale##DreamyZoom", &_scale, 1.f, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_scale##DreamyZoom")) { _scale = 1.2f; changed = true; }
        ImGui::PopItemWidth();
        if (_rotation != m_rotation) { m_rotation = _rotation; changed = true; }
        if (_scale != m_scale) { m_scale = _scale; changed = true; }
        return m_Enabled ? changed : false;
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
        if (value.contains("rotation"))
        {
            auto& val = value["rotation"];
            if (val.is_number()) 
                m_rotation = val.get<imgui_json::number>();
        }
        if (value.contains("scale"))
        {
            auto& val = value["scale"];
            if (val.is_number()) 
                m_scale = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["rotation"] = imgui_json::number(m_rotation);
        value["scale"] = imgui_json::number(m_scale);
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
        // if show icon then we using u8"\uf3e1"
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

    FlowPin   m_Enter       = { this, "Enter" };
    FlowPin   m_Exit        = { this, "Exit" };
    MatPin    m_MatInFirst  = { this, "In 1" };
    MatPin    m_MatInSecond = { this, "In 2" };
    FloatPin  m_Pos         = { this, "Pos" };
    MatPin    m_MatOut      = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_rotation    {6.f};
    float m_scale       {1.2f};
    ImGui::DreamyZoom_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4375;
    const unsigned int logo_data[4376/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xa8a22820, 0xd3881aeb, 0xe0b8e734, 
    0x118cddbf, 0x69b65b90, 0x2f706537, 0x87f65c51, 0xfb9b2be2, 0xc5deb2ef, 0x74d3451c, 0x7bae547b, 0xd0953ef7, 0x95948cd2, 0x511405d0, 0x14dd0654, 
    0x14054051, 0x75885756, 0xcc69a731, 0x81592201, 0x54702a44, 0x49e1afe3, 0x35b02bbb, 0xc35fae68, 0x71a1defa, 0xf76b1ba8, 0x1d46e62f, 0x7e5c553e, 
    0x7f0ea643, 0x4629ea0a, 0x02e84a4a, 0x03aa288a, 0xa0288a72, 0x132b8a02, 0xd98a1ac5, 0xbb10a669, 0x1c14e49e, 0xd2e418f4, 0x1bb8b293, 0xa00b5774, 
    0x6a90176a, 0x2c4ff0b6, 0x55a813ed, 0x0ce09c8d, 0x1e0f7065, 0xc2d4dc95, 0x16e3ca7c, 0x11ab288a, 0x501445b7, 0x83154501, 0x2d4dade2, 0x95aa0d6c, 
    0x4a056769, 0x5c5f95e7, 0x7c32957e, 0x236ee0aa, 0x8cae88ac, 0x30645819, 0xd37a0439, 0x3ceccdab, 0xd64a0deb, 0xfc1e0d56, 0x61766c15, 0xe8b5fe8c, 
    0xc641e5e0, 0x13a6da33, 0x8a161be6, 0x7904ad28, 0x4051146d, 0x303e7205, 0x05770dbe, 0xa6e52d91, 0xe73b401e, 0xfcad00ff, 0xf8acaeeb, 0xd4207834, 
    0xbc0bd4e5, 0xf1ddc993, 0x5171fa85, 0x86ace434, 0x00ff468e, 0x4a7db557, 0x8a4ff3d6, 0x2b000c48, 0x0e640029, 0x15a70f9d, 0x4f2ac3dd, 0xca194b0a, 
    0xd407863a, 0xd352831a, 0x0dedd4e1, 0x480ad9b4, 0x4750af20, 0xd1c63aa5, 0xb6236d6c, 0xc6586657, 0x525cb714, 0x025b5c8c, 0x5a5114c5, 0x8aa2db08, 
    0x2bae0028, 0x6f6dd45b, 0xc902433c, 0xe4b63620, 0xf3a49d2b, 0xfd8e9b9f, 0xaa9c5d31, 0x6cac13ed, 0xac90ca15, 0x6b7d7046, 0xbcf04c13, 0x97bc5216, 
    0xbfd92e45, 0x0363a4e7, 0x7a9e63a7, 0x761375d6, 0xd4c56848, 0xd6343bb5, 0xf9b42fad, 0x2bd4888b, 0x0e60a48d, 0x5d83fa3b, 0x23803bb8, 0x5b2daca1, 
    0x509716c3, 0x1c41593b, 0x08820417, 0x981e5725, 0x56c00f1c, 0xc415979e, 0xdc30741a, 0x768d6590, 0x4090a492, 0x94dad7e9, 0xc09d9414, 0x561445b5, 
    0xa22837a2, 0x6046008a, 0x03f458aa, 0xfd2bb826, 0x5f471d4e, 0x36932649, 0x54c62049, 0xf18cb791, 0x75ec8aef, 0x2c2e6f58, 0x45361b9a, 0x73b1ca67, 
    0xd383948f, 0x46565c8f, 0x682be199, 0x28da3fa2, 0xba7f48b3, 0xf4f502ec, 0x5163b5c7, 0x34244d4a, 0x5b8e7865, 0xae6dd465, 0xca116eac, 0x03231fa0, 
    0xa5e3a9b4, 0x274ab076, 0xa3573982, 0xd7f861a8, 0x5a78ad2d, 0xe7d1dade, 0x9bbc13b1, 0xc3fb22b2, 0x671cf404, 0x86b7d6f9, 0x85b49165, 0x96756586, 
    0xe3ca2806, 0xa11f7704, 0x3675a114, 0xe835509f, 0x46dc8aa2, 0x405114dd, 0x1d4fdc11, 0xad3c0fac, 0x8e6510b5, 0x175c8133, 0xb5d523db, 0x68bcb8e4, 
    0xe1708fe2, 0xa10715ba, 0xfe74b5cf, 0xbdbfb425, 0x86ac58b3, 0x1bea7246, 0xa7a78705, 0x52b527bf, 0xbc7df0b2, 0x0c6f6896, 0xf9d4cdb1, 0x07073d6e, 
    0xe9393882, 0x525458eb, 0x331ab293, 0x332154bc, 0x226d4ddb, 0x141e2dbc, 0x706404a1, 0xafabfc79, 0x1637e6d3, 0xb9714a10, 0xafc76e90, 0xedb8d67f, 
    0xd97fc36b, 0xa7881b70, 0x42775379, 0xe3196d80, 0x479e3c27, 0x3fbaca6f, 0xd1264e0c, 0xca95e362, 0xfe837b9e, 0x75213a7f, 0xbd667037, 0x885b5114, 
    0x8500ffc2, 0x5100ff75, 0xff97fc5f, 0xffa8ec00, 0xff758500, 0xfc5f5100, 0xec00ff97, 0xb9a2b5ab, 0x1c3be4b9, 0xaf2bfc57, 0x00ff8afa, 0x00ffbfe4, 
    0x2bfc4765, 0xff8afaaf, 0xffbfe400, 0xad5d6500, 0x8ebcbf57, 0x90e7d5c6, 0xa83fb8f0, 0x798ed61f, 0xfe4b8e05, 0x45fdd715, 0xff5ff27f, 0xfea3b200, 
    0x45fdd715, 0xff5ff27f, 0x2da9b200, 0xac78637c, 0x146f71ab, 0x3bc818c3, 0xf2a43e09, 0x825d413f, 0xd7159138, 0x254500a3, 0xeca84f52, 0xaef07f71, 
    0xff2beabf, 0x00ff9200, 0xf01f95fd, 0x2beabfae, 0xff9200ff, 0x7695fd00, 0x90e753b4, 0xf0bf73ac, 0xd400ff82, 0x2000ff4b, 0x7f54f67f, 0x00ff0bc2, 
    0x81fc2f51, 0x57d900ff, 0xed67455d, 0x3976dc67, 0x7f41f81f, 0x00ff25ea, 0x2afb3f90, 0xff05e13f, 0xfe97a800, 0xec00ff40, 0x2fa8aeab, 0xd9d2522e, 
    0x5c0072e6, 0xfef83e75, 0xf7597bb4, 0xff98230b, 0xfe178400, 0x03f95fa2, 0xa3b200ff, 0xfa5f10fe, 0x0fe47f89, 0x0f92cafe, 0x3145df11, 0x64242b67, 
    0xa6236392, 0x7e3d784e, 0x5675cd99, 0xdaa27dd3, 0xf506b039, 0x1fe70007, 0xac3d7a8f, 0xcb9185fb, 0xff0bc27f, 0xfc2f5100, 0xd900ff81, 0x0800ff51, 
    0xbf44fd2f, 0x00ff07f2, 0x14755d65, 0x0bf7597b, 0xfc00ff1c, 0xfbf4df22, 0xbf9000ff, 0xc27ff4fa, 0x4f00ff2d, 0xff0bf9bf, 0x055daf00, 0xf7597b14, 
    0x00ff1c0b, 0xf4df22fc, 0x9000fffb, 0x7ff4fabf, 0x00ff2dc2, 0x0bf9bf4f, 0x5daf00ff, 0xc93c4705, 0xce91116f, 0x47ef4107, 0xb1709fb5, 0x0800ff87, 
    0xff3efdb7, 0xfe2fe400, 0xa0c203bd, 0xfdc7bc74, 0xeb00ffb2, 0xe7469dd2, 0x425669cf, 0xf2703f01, 0x565be931, 0xe9d634d2, 0x6156a823, 0x6b2f0592, 
    0x1363e13e, 0xfa6f11fe, 0xc800ff7d, 0x3f7afd5f, 0x00ff16e1, 0x85fcdfa7, 0xaed700ff, 0xd67e8a82, 0x6fc6c27d, 0x37fd47f6, 0x3fc700ff, 0x647ff4fa, 
    0xfc7fd37f, 0xaf00ff73, 0xed51545a, 0x662cdc67, 0x7f6400ff, 0x73fc7fd3, 0x47af00ff, 0x37fd47f6, 0x3fc700ff, 0x4da5f5fa, 0x4d320a66, 0xc27dd61e, 
    0x1ffe54c6, 0x2e3fea82, 0x1c544976, 0x0182f180, 0x2cd3e4f5, 0x606f35fc, 0xe5cf968e, 0x0272b9ab, 0x8acf9ff1, 0x9031d2be, 0x547a24b0, 0x653a33f1, 
    0x5e00ff87, 0xe196b497, 0x47f68f62, 0x00ff37fd, 0xf4fa3fc7, 0xd37f647f, 0xff73fc7f, 0x545aaf00, 0xeeb3f653, 0x45d12916, 0x0a09c014, 0x71053d09, 
    0xbadced1a, 0x222dacae, 0x605f956f, 0x241bf05c, 0xad15f567, 0xdb509de2, 0x27967ddb, 0xb27a7965, 0x7a3046f0, 0x56730dfd, 0x52695e92, 0x65dbea2d, 
    0x22afca4f, 0x1ffb8412, 0xf449295e, 0x4b6b7d02, 0x205b304d, 0xcca74b2c, 0x9e7a1dc7, 0xbd5d113f, 0x84358d84, 0x99213032, 0x07f52001, 0x5923aeb8, 
    0x115657d6, 0xa7fcd937, 0x9c73824c, 0xeaf1dc8e, 0xe15b577b, 0x2dba04e9, 0x2e600cbb, 0x70e41838, 0xe34f507f, 0x80ddb542, 0x298aa2d2, 0x288a7281, 
    0x993546ac, 0x00314b51, 0x0db9e60e, 0x8db5e942, 0x85011245, 0x1dcf4538, 0xadce9ffa, 0x9ea7faeb, 0x8eddcec2, 0x38300bce, 0x8efa910d, 0xf659e547, 
    0x37e97217, 0xceb64722, 0x0a0ec8e5, 0x7de83992, 0x1398c23f, 0xe96ada6a, 0x5ceaf6d6, 0x7172b633, 0x5cfefa8c, 0x92a54b57, 0xf67ccb74, 0xa6d3018c, 
    0x5c31fd38, 0xc1aab1d6, 0x225b24a9, 0xb8eb7575, 0xff403df0, 0x0a3a1e00, 0x4e3234e8, 0x9c240893, 0x678c640c, 0xd3d500ff, 0x0a0da0f0, 0x0590a228, 
    0x95595314, 0x5030b314, 0x008a933b, 0x46112449, 0x1314ddce, 0x244f62c5, 0xd8de859a, 0x50c0e9fe, 0x00ff7b7b, 0xafeb5b3a, 0x83aa4ab6, 0xbfc7992a, 
    0x595200f9, 0xd9897dde, 0xfce42c5e, 0x9e7f98dd, 0xeb4b0268, 0x906ded48, 0x39163702, 0xe2f9cf3d, 0xa2aae9af, 0xaea33dc6, 0x73ea3849, 0xc81b3554, 
    0xcb8fa26e, 0x39277007, 0xa6bf8a1c, 0x9329ec67, 0x7f3ac6eb, 0x308dbf9e, 0x1445d12d, 0x9aa22880, 0x407310ce, 0x93513003, 0x9ce9ce55, 0x8c1407ed, 
    0x0324565a, 0x88522cc5, 0xf7e008f2, 0x23f90014, 0xef198b54, 0xc2053ac5, 0xcca6b967, 0x8231c8e8, 0x1c3e957f, 0x14034379, 0x51142501, 0xabe81440, 
    0x88f35594, 0xecd0bec3, 0x342f0635, 0xef23bc3f, 0xfae9c1a9, 0xd1ab39fe, 0x1146b1c6, 0x013b0a00, 0x9ca3a857, 0x3ac5430e, 0x4fba3d5a, 0x62ba676b, 
    0x9c9477e7, 0x8193f39c, 0xa7d64fcf, 0xa66199f0, 0x7a5b6e3c, 0xba95554a, 0xf9e318a9, 0x1c455157, 0x45a78cda, 0x718ea25c, 0x50511405, 0x269a8432, 
    0xc22cf39e, 0x1f188e00, 0x070e9090, 0xf1b84aa7, 0x5b7e2447, 0xf887acc6, 0x547e6458, 0x970328fa, 0x7babf4d7, 0xd99aa328, 0x583b290a, 0xae67246e, 
    0x3d00ff79, 0x8ec34b2b, 0x4a0a4b5b, 0x0770acfc, 0xe6f8ef61, 0x02a0a8b5, 0x02a0288a, 0xa5aeca9a, 0xd8034359, 0xa0a8d38c, 0xc1d8f00a, 0x4064de04, 
    0x678c2ba9, 0xbc2d9f22, 0xc6de8f33, 0xe738dbad, 0xa068a9f3, 0x15fb2b0c, 0xe4c82ab4, 0x8c0d74ab, 0xebf500ff, 0xd96c4c47, 0x4682072f, 0xcf7fc648, 
    0x51aec24f, 0x51140540, 0x50210540, 0x546404c3, 0x11f35454, 0x54233dcc, 0x3cd23bce, 0x3ef4fc4a, 0x0950dab4, 0x0ee628e8, 0x31231962, 0x6215c1f5, 
    0x20c79823, 0x1a0229fd, 0xb8683042, 0xa868c9ee, 0x5c98a368, 0x4551d4c4, 0x14456151, 0x14450150, 0x14450150, 0x14450150, 0x14450150, 0x14450150, 
    0x14450150, 0x14450150, 0x14450150, 0x14450150, 0x14450150, 0x51340450, 0x2a646645, 0x9c90eafd, 0xa9a18f0a, 0x445ca26a, 0xa6d89e04, 0xe25d5abf, 
    0xc73dad97, 0x94a222ba, 0x069534f5, 0x5f55d464, 0x3fd37fb6, 0x7fb68fd6, 0x9fd63fd3, 0x23ed473c, 0x5545b5dc, 0x33fd67fb, 0x67fb68fd, 0x68fd33fd, 
    0x487b88e7, 0x55512df7, 0x00ffd97e, 0x5a00ff4c, 0x00ffd93e, 0x5a00ff4c, 0xd21ee239, 0x5554cb3d, 0xd37fb65f, 0xb68fd63f, 0xd63fd37f, 0xb487788e, 
    0x15d5728f, 0xffbdfd91, 0xf8df4e00, 0x00ff00ff, 0xefed8f5a, 0x00ff76fa, 0xfa00ffc7, 0x3dc473d4, 0xf49a7ba4, 0xf7f64756, 0xe37f3bfd, 0x6afd00ff, 
    0xe9bfb73f, 0x1f00ffdb, 0x51eb00ff, 0x91f610cf, 0x59d16bee, 0xf4dfdb1f, 0x8f00ffed, 0xa8f500ff, 0x00ffdefe, 0x7ffc6fa7, 0x47ad00ff, 0x47da433c, 
    0x6445afb9, 0xd37f6f7f, 0xff3ffeb7, 0xfba3d600, 0xbf9dfe7b, 0xfe00fff1, 0x0ff11cb5, 0xbde61e69, 0xc27fcd15, 0x4e00ff5f, 0xff1bf91f, 0xfea3b100, 
    0xfa00ff12, 0xc800ff70, 0x1c8dfddf, 0x1e690ff1, 0x73454be7, 0xff97f05f, 0xfe87d300, 0xec00ff46, 0x8400ff68, 0x3f9cfebf, 0x00ff37f2, 0x433c4763, 
    0xd2b947da, 0xfcd75cd1, 0xf400ff25, 0x9100ffe1, 0x3f1afbbf, 0x00ff2fe1, 0x8dfc0fa7, 0xd1d800ff, 0x91f610cf, 0x57b474ee, 0x0900ff35, 0x7f38fd7f, 
    0xc6fe6fe4, 0xff4bf88f, 0xffc3e900, 0xf67f2300, 0x3dc47334, 0xf49b7ba4, 0x00ff2157, 0x43fdf709, 0xff3ff23f, 0xfc476300, 0x0cf5df27, 0xffc800ff, 
    0x478dfd00, 0x89e73332, 0xcaabd4d7, 0x00ffe28a, 0x00ff3ee1, 0x47fe67a8, 0x69ec00ff, 0x0c0cc447, 0xe47f667f, 0x9ac6fe7f, 0x912a459a, 0x67b46547, 
    0x57603334, 0x4f7e7c1a, 0xf27fc3fc, 0x6300ff3f, 0x0900ff49, 0x3f43fdf7, 0x00ff3ff2, 0x219e4f63, 0xecd023ed, 0xff902b29, 0xfefb8400, 0x1ff99fa1, 
    0xa3b100ff, 0xfaef13fe, 0x7fe47f86, 0x99a7c6fe, 0xa213cf13, 0x132b8aa2, 0x606e5401, 0xab38a9a3, 0x73d3670b, 0x581540fd, 0xa8230812, 0x521926ad, 
    0x5a47c601, 0x734f11d6, 0xbd52716a, 0x43ac85c8, 0x9afac939, 0x5c7e3cad, 0x220f1d98, 0x9717c5ad, 0x3db15892, 0xa36ea849, 0x40f0f363, 0x8f5255fe, 
    0x8f53b1bb, 0x512ba2bb, 0xce811545, 0x14455162, 0xd0905600, 0xd20d37ee, 0x20790644, 0x362bfd93, 0xcd8b78ba, 0x1d768764, 0x3b361fe3, 0x84b4d691, 
    0x9cd6dc53, 0x3b73af54, 0x9efe07fb, 0x1fb3ed33, 0x206eb3d6, 0x27da796b, 0x38e3b220, 0x674157fa, 0x7fb02d6b, 0xdda6e132, 0x7e3c678c, 0x43ab93b5, 
    0x6419dd2a, 0x0a9d0450, 0xff388e9c, 0x8a720a00, 0xe01cf64a, 0x46916e94, 0x8cac288a, 0xa2283a4c, 0x8bd00a80, 0x89e6d442, 0x28db8e25, 0xf54e19ea, 
    0xb3c61fe4, 0x1ad2b6eb, 0xc40e8f6b, 0x31649b04, 0xeee323b2, 0x2c3f90e0, 0x3645b80a, 0x3ba90869, 0x2bbc1133, 0x1e922aa8, 0x63b02306, 0xc68af993, 
    0x91dd6874, 0xc129abd4, 0xeaaeb107, 0xd6f6dec2, 0x74d73e39, 0x40e5896e, 0xfbf83150, 0xa3aa1fd7, 0x9231a5a6, 0x8b9d8517, 0x5e0f72ec, 0x39a38e7a, 
    0xaea438a7, 0x7425c139, 0x2b8aa253, 0x8a122333, 0xac02a028, 0x5cdfa6c3, 0xca8224c6, 0x193d54e2, 0xf1076622, 0x7957ab02, 0x8f4e40e0, 0x3c5f0772, 
    0x15f9fee3, 0x2e765751, 0x3876e611, 0x7dec00ff, 0x36cc7f50, 0x37fcfeef, 0x083255f8, 0x57042338, 0xfa36e9a7, 0xff53a4b5, 0x1dc96800, 0x883140ce, 
    0xa88f999d, 0x2b3db0f9, 0x461075ce, 0x4c9873a5, 0x968f9539, 0x39410557, 0x36ca29c6, 0x55399c43, 0x4551d4ca, 0x54ea9941, 0x15004551, 0x140609a1, 
    0xeab3d223, 0x1c0937cc, 0x07395b41, 0xa44d69b5, 0x24a535f5, 0xcf12a19e, 0x6db3e0e6, 0xd47b9207, 0xce58b0f2, 0xf7d0e948, 0xaef31aa8, 0x6a12fad4, 
    0x1b692409, 0xd50e7f2c, 0x222bc95c, 0x6515e3b9, 0xa228daa8, 0x283130b1, 0x2b008aa2, 0xc99cd3a4, 0x19c7c2a7, 0x638fb70b, 0xdd5ce98f, 0x64d5b65e, 
    0x8810b6b6, 0xd6ceedd4, 0x84abf3cf, 0x539a7a92, 0xbbd48b92, 0x977ec1a7, 0x7024ae1b, 0x0d667480, 0x358ad79f, 0xac5b10b9, 0xc809197c, 0x4faac01d, 
    0xe33addab, 0x39eabe2a, 0x244b72aa, 0x6747baad, 0x35c7323d, 0x45d64a4e, 0x215bae39, 0x644551b4, 0x45d16162, 0x6f570014, 0x01f9dfe1, 0xc000ffdb, 
    0x5c23f4bf, 0x53875a45, 0x4588b7bd, 0x2fa2cb0d, 0x57810745, 0x97bb7209, 0x9dbb7209, 0xa961935e, 0x79edca5a, 0x1821e77b, 0x3eb7b809, 0x0f2b3db9, 
    0x4f31f7c4, 0xe1c4aaa8, 0xc4dab4fc, 0x54f1e47a, 0x2eafd426, 0x0ed76413, 0xd3aadee3, 0x87acae94, 0x82aca629, 0xccac288a, 0xa2284acc, 0x3cee0a80, 
    0x1abc3517, 0xcc2dcb3c, 0x9c946448, 0x008e6033, 0x5cedf4ef, 0x23b32a3d, 0x472a5606, 0x17557170, 0x2ea37267, 0xd1bb7357, 0x403fcdb4, 0xd6a4ee9d, 
    0x231594e3, 0xe2472168, 0xaf5c7332, 0xbf636f88, 0x212eeed6, 0x078584fb, 0x9e31708e, 0x4696812a, 0x418c3404, 0xd33c96eb, 0x595db929, 0xb2ba520e, 
    0xa0a2280a, 0x00d9ff83, 
};
};
} // namespace BluePrint
