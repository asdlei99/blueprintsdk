#include <Node.h>
#include <Debug.h>
#include <imgui_node_editor_internal.h>
#include <imgui_helper.h>
#include <BuildInNodes.h> // Which is generated by cmake

namespace BluePrint
{
ID_TYPE NodeTypeIDFromName(string type, string catalog)
{
    return fnv1a_hash_32(type + string("*") + catalog);
}

string NodeTypeToString(NodeType type)
{
    switch (type)
    {
        default:                    return "UnKnown";
        case NodeType::Internal:    return "Internal";
        case NodeType::EntryPoint:  return "Entry";
        case NodeType::ExitPoint:   return "Exit";
        case NodeType::External:    return "External";
        case NodeType::Dummy:       return "Dummy";
    }
}

bool NodeTypeFromString(string str, NodeType& type)
{
    if (str.compare("Internal") == 0)
        type = NodeType::Internal;
    else if (str.compare("Entry") == 0)
        type = NodeType::EntryPoint;
    else if (str.compare("Exit") == 0)
        type = NodeType::ExitPoint;
    else if (str.compare("External") == 0)
        type = NodeType::External;
    else if (str.compare("Dummy") == 0)
        type = NodeType::Dummy;
    else
        return false;
    return true;
}

string NodeStyleToString(NodeStyle style)
{
    switch (style)
    {
        default:                    return "UnKnown Style";
        case NodeStyle::Dummy:      return "Dummy Style";
        case NodeStyle::Default:    return "Default Style";
        case NodeStyle::Simple:     return "Simple Style";
        case NodeStyle::Comment:    return "Comment Style";
        case NodeStyle::Group:      return "Group Style";
        case NodeStyle::Custom:     return "Custom Style";
    }
}

bool NodeStyleFromString(string str, NodeStyle& style)
{
    if (str.compare("Dummy Style") == 0)
        style = NodeStyle::Dummy;
    else if (str.compare("Default Style") == 0)
        style = NodeStyle::Default;
    else if (str.compare("Simple Style") == 0)
        style = NodeStyle::Simple;
    else if (str.compare("Comment Style") == 0)
        style = NodeStyle::Comment;
    else if (str.compare("Group Style") == 0)
        style = NodeStyle::Group;
    else if (str.compare("Custom Style") == 0)
        style = NodeStyle::Custom;
    else
        return false;
    return true;
}

string NodeVersionToString(VERSION_TYPE version)
{
    return  std::to_string(VERSION_MAJOR(version)) + "." +
            std::to_string(VERSION_MINOR(version)) + "." +
            std::to_string(VERSION_PATCH(version));
            /* + "." +
            std::to_string(VERSION_BUILT(version)); */
}

bool NodeVersionFromString(string str, VERSION_TYPE& version)
{
    uint32_t version_major = 0, version_minor = 0, version_patch = 0, version_built = 0;
    sscanf(str.c_str(), "%d.%d.%d.%d", &version_major, &version_minor, &version_patch, &version_built);
    version = (version_major << 24) | (version_minor << 16) | (version_patch << 8) | version_built;
    if (version == 0 || version == 0xFFFFFFFF)
        return false;
    return true;
}

// ------------------------------
// -------[ NodeRegistry ]-------
// ------------------------------

NodeRegistry::NodeRegistry()
    : m_BuildInNodes({
        DummyNode::GetStaticTypeInfo(),
        SystemEntryPointNode::GetStaticTypeInfo(),
        SystemExitPointNode::GetStaticTypeInfo(),
        FilterEntryPointNode::GetStaticTypeInfo(),
        TransitionEntryPointNode::GetStaticTypeInfo(),
        MatExitPointNode::GetStaticTypeInfo(),
        CommentNode::GetStaticTypeInfo(),
        GroupNode::GetStaticTypeInfo(),
        DateTimeNode::GetStaticTypeInfo(),
        ConstValueNode::GetStaticTypeInfo(),
        LoopNode::GetStaticTypeInfo(),
        FloatCountNode::GetStaticTypeInfo(),
        CountNode::GetStaticTypeInfo(),
        ToStringNode::GetStaticTypeInfo(),
        TimerNode::GetStaticTypeInfo(),
        FileSelectNode::GetStaticTypeInfo(),
        AddNode::GetStaticTypeInfo(),
        SubNode::GetStaticTypeInfo(),
        MulNode::GetStaticTypeInfo(),
        DivNode::GetStaticTypeInfo(),
        CompareNode::GetStaticTypeInfo(),
        ComparatorNode::GetStaticTypeInfo(),
        SwitchNode::GetStaticTypeInfo(),
        BranchNode::GetStaticTypeInfo(),
        FlipFlopNode::GetStaticTypeInfo(),
        PrintNode::GetStaticTypeInfo(),
    })
{
    RebuildTypes();
}

NodeRegistry::~NodeRegistry()
{
    for (auto node : m_Nodes)
    {
        delete node;
    }
    m_Nodes.clear();
    for (auto obj : m_ExternalObject)
    {
        delete obj;
    }
    m_ExternalObject.clear();
}

ID_TYPE NodeRegistry::RegisterNodeType(shared_ptr<NodeTypeInfo> info)
{
    // regiester static node which has NodeTypeInfo
    auto id = info->m_ID;

    auto it = std::find_if(m_CustomNodes.begin(), m_CustomNodes.end(), [id](const NodeTypeInfo& typeInfo)
    {
        return typeInfo.m_ID == id;
    });

    if (it != m_CustomNodes.end())
        m_CustomNodes.erase(it);

    NodeTypeInfo typeInfo;
    typeInfo.m_ID               = id;
    typeInfo.m_Name             = info->m_Name;
    typeInfo.m_NodeTypeName     = info->m_NodeTypeName;
    typeInfo.m_Version          = info->m_Version;
    typeInfo.m_SDK_Version      = info->m_SDK_Version;
    typeInfo.m_API_Version      = info->m_API_Version;
    typeInfo.m_Author           = info->m_Author;
    typeInfo.m_Type             = info->m_Type;
    typeInfo.m_Style            = info->m_Style;
    typeInfo.m_Catalog          = info->m_Catalog;
    typeInfo.m_Factory          = info->m_Factory;
    typeInfo.m_Url              = info->m_Url;

    m_CustomNodes.push_back(std::move(typeInfo));

    RebuildTypes();

    return id;
}

ID_TYPE NodeRegistry::RegisterNodeType(std::string Path)
{
    auto dlobject = new DLClass<NodeTypeInfo>(Path.c_str());
    if (!dlobject)
    {
        return 0;
    }
    auto info = dlobject->make_obj();
    if (!info)
    {
        delete dlobject;
        return 0;
    }

    int32_t version = dlobject->get_version();
    if (version < VERSION_BLUEPRINT)
    {
        LOGW("[RegisterNodeType] Warning Node BluePrint Version(%d.%d.%d.%d) less then App BluePrint Version(%d.%d.%d.%d)\n", 
                VERSION_MAJOR(version), VERSION_MINOR(version), VERSION_PATCH(version), VERSION_BUILT(version),
                VERSION_MAJOR(VERSION_BLUEPRINT), VERSION_MINOR(VERSION_BLUEPRINT), VERSION_PATCH(VERSION_BLUEPRINT), VERSION_BUILT(VERSION_BLUEPRINT));
    }
    int32_t api_version = dlobject->get_api_version();
    if (api_version < VERSION_BLUEPRINT_API)
    {
        LOGW("[RegisterNodeType] Warning Node BluePrint API Version(%d.%d.%d) less then App BluePrint API Version(%d.%d.%d)\n", 
                VERSION_MAJOR(api_version), VERSION_MINOR(api_version), VERSION_PATCH(api_version),
                VERSION_MAJOR(VERSION_BLUEPRINT_API), VERSION_MINOR(VERSION_BLUEPRINT_API), VERSION_PATCH(VERSION_BLUEPRINT_API));
    }

    m_ExternalObject.push_back(dlobject);
    info->m_Url = ImGuiHelper::path_url(Path);
    return RegisterNodeType(info);
}

void NodeRegistry::UnregisterNodeType(std::string name)
{
    auto it = std::find_if(m_CustomNodes.begin(), m_CustomNodes.end(), [name](const NodeTypeInfo& typeInfo)
    {
        return typeInfo.m_Name == name;
    });

    if (it == m_CustomNodes.end())
        return;

    m_CustomNodes.erase(it);

    RebuildTypes();
}

void NodeRegistry::RebuildTypes()
{
    m_Types.resize(0);
    m_Types.reserve(m_CustomNodes.size() + std::distance(std::begin(m_BuildInNodes), std::end(m_BuildInNodes)));

    for (auto& typeInfo : m_CustomNodes)
        m_Types.push_back(&typeInfo);

    for (auto& typeInfo : m_BuildInNodes)
        m_Types.push_back(&typeInfo);

    std::sort(m_Types.begin(), m_Types.end(), [](const NodeTypeInfo* lhs, const NodeTypeInfo* rhs) { return lhs->m_ID < rhs->m_ID; });
    m_Types.erase(std::unique(m_Types.begin(), m_Types.end()), m_Types.end());

    // rebuild catalog
    for (auto type : m_Types)
    {
        bool alread_in_list = false;
        for (auto catalog : m_Catalogs)
        {
            if (catalog.compare(type->m_Catalog) == 0)
            {
                alread_in_list = true;
                break;
            }
        }
        if (!alread_in_list)
        {
            m_Catalogs.push_back(type->m_Catalog);
        }
        bool alread_in_nodes = false;
        for (auto node : m_Nodes)
        {
            if (node->GetTypeID() == type->m_ID)
            {
                alread_in_nodes = true;
                break;
            }
        }
        if (!alread_in_nodes)
        {
            auto node = Create(type->m_ID, nullptr);
            if (node) m_Nodes.push_back(node);
        }
    }
}

Node* NodeRegistry::Create(ID_TYPE typeId, BP* blueprint)
{
    for (auto& nodeInfo : m_Types)
    {
        if (nodeInfo->m_ID != typeId)
            continue;

        return nodeInfo->m_Factory(blueprint);
    }

    return nullptr;
}

Node* NodeRegistry::Create(std::string typeName, BP* blueprint)
{
    for (auto& nodeInfo : m_Types)
    {
        if (nodeInfo->m_Name != typeName)
            continue;

        return nodeInfo->m_Factory(blueprint);
    }

    return nullptr;
}

span<const NodeTypeInfo* const> NodeRegistry::GetTypes() const
{
    const NodeTypeInfo* const* begin = m_Types.data();
    const NodeTypeInfo* const* end   = m_Types.data() + m_Types.size();
    return make_span(begin, end);
}

span<const std::string> NodeRegistry::GetCatalogs() const
{
    const std::string *begin = m_Catalogs.data();
    const std::string *end   = m_Catalogs.data() + m_Catalogs.size();
    return make_span(begin, end);
}

span<const Node * const> NodeRegistry::GetNodes() const
{
    const Node* const* begin = m_Nodes.data();
    const Node* const* end   = m_Nodes.data() + m_Nodes.size();
    return make_span(begin, end);
}

const NodeTypeInfo* NodeRegistry::GetTypeInfo(ID_TYPE typeId) const
{
    for (auto& nodeInfo : m_Types)
    {
        if (nodeInfo->m_ID == typeId)
            return nodeInfo;
    }
    return nullptr;
}

// ----------------------
// -------[ Node ]-------
// ----------------------

Node::Node(BP* blueprint)
    : m_Blueprint(blueprint)
{
    if (blueprint) m_ID = blueprint->MakeNodeID(this);
}

unique_ptr<Pin> Node::CreatePin(PinType pinType, std::string name)
{
    switch (pinType)
    {
        default:
        case PinType::Void:     return nullptr;
        case PinType::Any:      return make_unique<AnyPin>(this, name);
        case PinType::Flow:     return make_unique<FlowPin>(this, name);
        case PinType::Bool:     return make_unique<BoolPin>(this, name);
        case PinType::Int32:    return make_unique<Int32Pin>(this, name);
        case PinType::Int64:    return make_unique<Int64Pin>(this, name);
        case PinType::Float:    return make_unique<FloatPin>(this, name);
        case PinType::Double:   return make_unique<DoublePin>(this, name);
        case PinType::String:   return make_unique<StringPin>(this, name, "");
        case PinType::Point:    return make_unique<PointPin>(this, name);
        case PinType::Vec2:     return make_unique<Vec2Pin>(this, name);
        case PinType::Vec4:     return make_unique<Vec4Pin>(this, name);
    }

    return nullptr;
}

Pin * Node::NewPin(PinType pinType, std::string name)
{
    Pin * pin = nullptr;
    switch (pinType)
    {
        case PinType::Any :     pin = new AnyPin(this, name); break;
        case PinType::Flow :    pin = new FlowPin(this, name); break;
        case PinType::Bool :    pin = new BoolPin(this, name); break;
        case PinType::Int32 :   pin = new Int32Pin(this, name); break;
        case PinType::Int64 :   pin = new Int64Pin(this, name); break;
        case PinType::Float :   pin = new FloatPin(this, name); break;
        case PinType::Double :  pin = new DoublePin(this, name); break;
        case PinType::String :  pin = new StringPin(this, name, ""); break;
        case PinType::Point :   pin = new PointPin(this, name); break;
        case PinType::Vec2 :    pin = new Vec2Pin(this, name); break;
        case PinType::Vec4 :    pin = new Vec4Pin(this, name); break;
        case PinType::Mat :     pin = new MatPin(this, name); break;
        case PinType::Array :   pin = new ArrayPin(this, name); break;
        default: break;
    }
    return pin;
}

std::string Node::GetName() const
{
    return m_Name;
}

ID_TYPE Node::GetTypeID() const
{
    return GetTypeInfo().m_ID;
}

NodeType Node::GetType() const
{
    return GetTypeInfo().m_Type;
}

VERSION_TYPE Node::GetVersion() const
{
    return GetTypeInfo().m_Version;
}

VERSION_TYPE Node::GetSDKVersion() const
{
    return GetTypeInfo().m_SDK_Version;
}

VERSION_TYPE Node::GetAPIVersion() const
{
    return GetTypeInfo().m_API_Version;
}

std::string Node::GetAuthor() const
{
    return GetTypeInfo().m_Author;
}

NodeStyle Node::GetStyle() const
{
    return GetTypeInfo().m_Style;
}

std::string Node::GetCatalog() const
{
    return GetTypeInfo().m_Catalog;
}

std::string Node::GetURL() const
{
    ID_TYPE id = GetTypeInfo().m_ID;
    auto nodeRegistry = m_Blueprint->GetNodeRegistry();
    auto type_info = nodeRegistry->GetTypeInfo(id);
    return type_info->m_Url;
}

void Node::SetName(std::string name)
{
    m_Name = name;
}

void Node::SetBreakPoint(bool breaken)
{
    m_BreakPoint = breaken;
}

bool Node::IsSelected()
{
    return ed::IsNodeSelected(m_ID);
}

void Node::DrawSettingLayout(ImGuiContext * ctx)
{
    // Draw Setting
    if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
    
    ImGui::TextUnformatted("Node Name:"); ImGui::SameLine(0.f, 50.f);
    string value = m_Name;
    if (ImGui::InputText("##node_name_string_value", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            auto& stringValue = *static_cast<string*>(data->UserData);
            ImVector<char>* my_str = (ImVector<char>*)data->UserData;
            //IM_ASSERT(stringValue.data() == data->Buf);
            stringValue.resize(data->BufSize);
            data->Buf = (char*)stringValue.data();
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
        {
            auto& stringValue = *static_cast<string*>(data->UserData);
            stringValue = std::string(data->Buf);
        }
        return 0;
    }, &value))
    {
        value.resize(strlen(value.c_str()));
        if (m_Name.compare(value) != 0)
        {
            m_Name = value;
            ed::SetNodeChanged(m_ID);
        }
    }
}

void Node::DrawMenuLayout(ImGuiContext * ctx)
{
}

void Node::DrawNodeLogo(ImGuiContext * ctx, ImVec2 size, std::string logo) const
{
    if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
    float font_size = ImGui::GetFontSize();
    float size_min = size.x > size.y ? size.y : size.x;
    float font_scale = (size_min - 16) / font_size;
    float font_shadow = font_scale + 2;
    ImGui::SetWindowFontScale(font_scale);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TexGlyphShadow, ImVec4(0, 0, 0, 1.0));
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(font_shadow, font_shadow));
    ImGui::Button((logo + "##" + std::to_string(m_ID)).c_str(), size); // set default logo is a icon button
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::SetWindowFontScale(1.0);
}

ImTextureID Node::LoadNodeLogo(void * data, int size) const
{
    ImTextureID logo = nullptr;
    if (!data || !size)
        return logo;
    int width = 0, height = 0, component = 0;
    if (auto _data = stbi_load_from_memory((stbi_uc const *)data, size, &width, &height, &component, 4))
    {
        logo = ImGui::ImCreateTexture(_data, width, height);
    }
    return logo;
}

void Node::DrawNodeLogo(ImTextureID logo, int& index, int cols, int rows, ImVec2 size) const
{
    if (!logo)
        return;
    int col = (index / 4) % cols;
    int row = (index / 4) / cols;
    float start_x = (float)col / (float)cols;
    float start_y = (float)row / (float)rows;
    ImGui::Image(logo, size, ImVec2(start_x, start_y),  ImVec2(start_x + 1.f / (float)cols, start_y + 1.f / (float)rows));
    index++; if (index >= cols * rows * 4) index = 0;
}

bool Node::DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::Curve * key, bool embedded)
{
    return false;
}

void Node::DrawDataTypeSetting(const char * label, ImDataType& type, bool full_type)
{
    ImGui::TextUnformatted(label); ImGui::SameLine();
    ImGui::RadioButton("AsInput", (int *)&type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
    auto pos = ImGui::GetCursorPos();
    ImGui::RadioButton("Int8",    (int *)&type, (int)IM_DT_INT8); ImGui::SameLine();
    ImGui::RadioButton("Int16",   (int *)&type, (int)IM_DT_INT16); ImGui::SameLine();
    ImGui::RadioButton("Float16", (int *)&type, (int)IM_DT_FLOAT16); ImGui::SameLine();
    ImGui::RadioButton("Float32", (int *)&type, (int)IM_DT_FLOAT32);
    if (full_type)
    {
        ImGui::SetCursorPosX(pos.x);
        ImGui::RadioButton("Int16BE", (int *)&type, (int)IM_DT_INT16_BE); ImGui::SameLine();
        ImGui::RadioButton("Int32",   (int *)&type, (int)IM_DT_INT32); ImGui::SameLine();
        ImGui::RadioButton("Int64",   (int *)&type, (int)IM_DT_INT64); ImGui::SameLine();
        ImGui::RadioButton("Float64", (int *)&type, (int)IM_DT_FLOAT64);
    }
}

LinkQueryResult Node::AcceptLink(const Pin& receiver, const Pin& provider) const
{
    if (!receiver.IsMappedPin() && !provider.IsMappedPin() && receiver.m_Node == provider.m_Node)
        return { false, "Please drag pin link to other pin"};

    const auto receiverIsFlow = receiver.GetType() == PinType::Flow;
    const auto providerIsFlow = provider.GetType() == PinType::Flow;
    if (receiverIsFlow != providerIsFlow)
        return { false, "Flow pins can be connected only to other Flow pins"};

    if (receiver.IsInput() && provider.IsInput())
        return { false, "Input pins cannot be linked together"};

    if (receiver.IsOutput() && provider.IsOutput())
        return { false, "Output pins cannot be linked together"};

    if (!receiver.IsMappedPin() && !receiver.IsReceiver())
        return { false, "Receiver pin cannot be used as provider"};

    if (!provider.IsMappedPin() && !provider.IsProvider())
        return { false, "Provider pin cannot be used as receiver"};

    if (provider.GetValueType() != receiver.GetValueType() && (provider.GetType() != PinType::Any && receiver.GetType() != PinType::Any))
        return { false, "Incompatible types"};

    return {true};
}

void Node::WasLinked(const Pin& receiver, const Pin& provider)
{
}

void Node::WasUnlinked(const Pin& receiver, const Pin& provider)
{
}

int Node::Load(const imgui_json::value& value)
{
    if (!value.is_object())
        return BP_ERR_NODE_LOAD;

    if (!imgui_json::GetTo<imgui_json::number>(value, "id", m_ID)) // required
        return BP_ERR_NODE_LOAD;

    if (!imgui_json::GetTo<imgui_json::string>(value, "name", m_Name)) // required
        return BP_ERR_NODE_LOAD;

    if (imgui_json::GetTo<imgui_json::boolean>(value, "break_point", m_BreakPoint)) // optional
    {
    }

    if (value.contains("enabled"))
    { 
        auto& val = value["enabled"];
        if (val.is_boolean())
            m_Enabled = val.get<imgui_json::boolean>();
    }

    string nodeVersion;
    if (imgui_json::GetTo<imgui_json::string>(value, "version", nodeVersion)) // optional
    {
        //TODO::Dicky need check Node version?
    }

    string nodeType;
    if (imgui_json::GetTo<imgui_json::string>(value, "type", nodeType)) // optional
    {
        //TODO::Dicky need check node typr?
    }
    
    string nodeStyle;
    if (imgui_json::GetTo<imgui_json::string>(value, "style", nodeStyle)) // optional
    {
        //TODO::Dicky need check node style?
    }

    string nodeCatalog;
    if (imgui_json::GetTo<imgui_json::string>(value, "catalog", nodeCatalog)) // optional
    {
        //TODO::Dicky need check node catalog?
    }

    if (!imgui_json::GetTo<imgui_json::number>(value, "group_id", m_GroupID)) // optional
    {
        //TODO::Dicky need check node group id?
    }

    const imgui_json::array* inputPinsArray = nullptr;
    if (imgui_json::GetPtrTo(value, "input_pins", inputPinsArray)) // optional
    {
        auto pins = GetInputPins();

        if (pins.size() != inputPinsArray->size())
            return BP_ERR_PIN_NUMPER;

        auto pin = pins.data();
        for (auto& pinValue : *inputPinsArray)
        {
            if (!(*pin)->Load(pinValue))
            {
                return BP_ERR_INPIN_LOAD;
            }
            ++pin;
        }
    }

    const imgui_json::array* outputPinsArray = nullptr;
    if (imgui_json::GetPtrTo(value, "output_pins", outputPinsArray)) // optional
    {
        auto pins = GetOutputPins();

        if (pins.size() != outputPinsArray->size())
            return BP_ERR_PIN_NUMPER;

        auto pin = pins.data();
        for (auto& pinValue : *outputPinsArray)
        {
            if (!(*pin)->Load(pinValue))
                return BP_ERR_OUTPIN_LOAD;

            ++pin;
        }
    }

    return BP_ERR_NONE;
}

void Node::Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID)
{
    bool isRemap = MapID.size() > 0;
    value["id"] = imgui_json::number(GetIDFromMap(m_ID, MapID)); // required
    value["name"] = imgui_json::string(m_Name); // required
    value["enabled"] = imgui_json::boolean(m_Enabled);
    value["break_point"] = m_BreakPoint;
    value["type"] = NodeTypeToString(GetType());
    value["style"] = NodeStyleToString(GetStyle());
    value["catalog"] = GetCatalog();
    value["version"] = NodeVersionToString(GetVersion());
    if (m_GroupID) value["group_id"] = imgui_json::number(GetIDFromMap(m_GroupID, MapID));

    auto& inputPinsValue = value["input_pins"]; // optional
    for (auto& pin : const_cast<Node*>(this)->GetInputPins())
    {
        imgui_json::value pinValue;
        pin->Save(pinValue, MapID);
        if (isRemap && (pin->m_Flags & PIN_FLAG_EXPORTED))
        {
            auto new_flags = pin->m_Flags;
            new_flags &= ~PIN_FLAG_EXPORTED;
            new_flags |= PIN_FLAG_PUBLICIZED;
            pinValue["flags"] = imgui_json::number(new_flags);
        }
        inputPinsValue.push_back(pinValue);
    }
    if (inputPinsValue.is_null())
        value.erase("input_pins");

    auto& outputPinsValue = value["output_pins"]; // optional
    for (auto& pin : const_cast<Node*>(this)->GetOutputPins())
    {
        imgui_json::value pinValue;
        pin->Save(pinValue, MapID);
        if (isRemap && (pin->m_Flags & PIN_FLAG_EXPORTED))
        {
            auto new_flags = pin->m_Flags;
            new_flags &= ~PIN_FLAG_EXPORTED;
            new_flags |= PIN_FLAG_PUBLICIZED;
            pinValue["flags"] = imgui_json::number(new_flags);
        }
        outputPinsValue.push_back(pinValue);
    }
    if (outputPinsValue.is_null())
        value.erase("output_pins");
}
} // namespace BluePrint

