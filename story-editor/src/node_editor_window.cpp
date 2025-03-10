#include "node_editor_window.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include "IconsFontAwesome5_c.h"

#include "media_node.h"
#include "gui.h"

#include <stdexcept> // for std::runtime_error
#define JSON_ASSERT(x) \
if (!(x)) { \
        throw std::runtime_error("Assertion failed: " #x); \
}
#include "json.hpp"


NodeEditorWindow::NodeEditorWindow(IStoryManager &proj)
    : WindowBase("Node editor")
    , m_story(proj)
{

    registerNode<MediaNode>("media-node");
}

NodeEditorWindow::~NodeEditorWindow()
{
    ed::DestroyEditor(m_context);
}

void NodeEditorWindow::Initialize()
{
    ed::Config config;
    config.SettingsFile = "Widgets.json";
    m_context = ed::CreateEditor(&config);

    ed::SetCurrentEditor(m_context);
}

void NodeEditorWindow::Clear()
{
    m_nodes.clear();
    m_ids.clear();
}



void NodeEditorWindow::LoadNode(const nlohmann::json &nodeJson)
{
    try
    {
        int restoredNodeId = nodeJson["id"].get<int>();
        nlohmann::json internalDataJson = nodeJson["internal-data"];
        std::string type = nodeJson["type"].get<std::string>();

        auto n = createNode(type, "", m_story);
        if (n)
        {
            n->SetType(type); // FIXME: set type in createNode factory?
            n->SetId(restoredNodeId);
            nlohmann::json posJson = nodeJson["position"];
            n->SetOutputs(nodeJson["outPortCount"].get<int>());
            n->SetPosition(posJson["x"].get<float>(), posJson["y"].get<float>());
            n->FromJson(internalDataJson);

            m_ids.insert(restoredNodeId);

            m_nodes.push_back(n);
        }
        else
        {
            throw std::logic_error(std::string("No registered model with name ") + type);
        }
    }
    catch (std::exception&  e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
    }

}

int NodeEditorWindow::GenerateNodeId()
{
    int max = 1;
    if (m_ids.size() > 0)
    {
        auto max = *m_ids.rbegin();
        max++;
        m_ids.insert(max);
    }
    return max;
}


ed::PinId NodeEditorWindow::GetInputPin(unsigned long modelNodeId, int pinIndex)
{
    ed::PinId id = 0;

    for (auto & n : m_nodes)
    {
        if (n->GetId() == modelNodeId)
        {
            id = n->GetInputPinAt(pinIndex);
        }
    }

    if (id.Get() == 0)
    {
        std::cout << "Invalid Id, input pin not found" << std::endl;
    }

    return id;
}

ed::PinId NodeEditorWindow::GetOutputPin(unsigned long modelNodeId, int pinIndex)
{
    ed::PinId id = 0;

    for (auto & n : m_nodes)
    {
        if (n->GetId() == modelNodeId)
        {
            id = n->GetOutputPinAt(pinIndex);
        }
    }

    if (id.Get() == 0)
    {
        std::cout << "Invalid Id, output pin not found" << std::endl;
    }

    return id;
}

void NodeEditorWindow::Load(const nlohmann::json &model)
{
    nlohmann::json nodesJsonArray = model["nodes"];

    BaseNode::InitId();
    m_nodes.clear();
    m_links.clear();

    for (auto& element : nodesJsonArray) {
        LoadNode(element);
    }

    std::cout << model.dump(4) << std::endl;

    nlohmann::json connectionJsonArray = model["connections"];

    for (auto& connection : connectionJsonArray)
    {
        Connection model = connection.get<Connection>();
        CreateLink(model,
                   GetInputPin(model.inNodeId, model.inPortIndex),
                   GetOutputPin(model.outNodeId, model.outPortIndex));
    }
}


void NodeEditorWindow::CreateLink(const Connection &model, ed::PinId inId, ed::PinId outId)
{
    auto conn = std::make_shared<LinkInfo>();

    *conn->model = model;

    // ImGui stuff for links
    conn->ed_link->Id = BaseNode::GetNextId();
    conn->ed_link->InputId = inId;
    conn->ed_link->OutputId = outId;

    // Since we accepted new link, lets add one to our list of links.
    m_links.push_back(conn);
}

void NodeEditorWindow::Save(nlohmann::json &model)
{
    ed::SetCurrentEditor(m_context);
    // Save nodes

    nlohmann::json nodes = nlohmann::json::array();
    for (const auto & n : m_nodes)
    {
        nlohmann::json node;
        node["id"] = n->GetId();
        node["type"] = n->GetType();
        node["outPortCount"] = n->Outputs();
        node["inPortCount"] = n->Inputs();

        nlohmann::json position;
        position["x"] = n->GetX();
        position["y"] = n->GetY();

        nlohmann::json internalData;

        n->ToJson(internalData);

        node["position"] = position;
        node["internal-data"] = internalData;
        nodes.push_back(node);
    }

    model["nodes"] = nodes;

    // Save links
    nlohmann::json connections = nlohmann::json::array();
    for (const auto& linkInfo : m_links)
    {

        nlohmann::json c;

        Connection cnx = LinkToModel(linkInfo->ed_link->InputId, linkInfo->ed_link->OutputId);

        c["outNodeId"] = cnx.outNodeId;
        c["outPortIndex"] = cnx.outPortIndex;
        c["inNodeId"] = cnx.inNodeId;
        c["inPortIndex"] = cnx.inPortIndex;

        connections.push_back(c);
    }

    model["connections"] = connections;
    ed::SetCurrentEditor(nullptr);
}

Connection NodeEditorWindow::LinkToModel(ed::PinId InputId, ed::PinId OutputId)
{
    Connection c;
    int index;
    for (const auto & n : m_nodes)
    {
        if (n->HasOnputPinId(OutputId, index))
        {
            c.outNodeId = n->GetId();
            c.outPortIndex = index;
        }

        if (n->HasInputPinId(InputId, index))
        {
            c.inNodeId = n->GetId();
            c.inPortIndex = index;
        }
    }

    return c;
}

uint32_t NodeEditorWindow::FindFirstNode() const
{
    uint32_t id = 0;

    // First node is the one without connection on its input port

    for (const auto & n : m_nodes)
    {
        bool foundConnection = false;
        for (const auto& l : m_links)
        {
            if (l->model->inNodeId == n->GetId())
            {
                foundConnection = true;
            }
        }

        if (!foundConnection)
        {
            id = n->GetId();
            m_story.Log("First node is: " + std::to_string(id));
            break;
        }
    }

    return id;
}

std::string NodeEditorWindow::Build()
{
    std::stringstream code;
    ed::SetCurrentEditor(m_context);


    std::stringstream chip32;

    uint32_t firstNode = FindFirstNode();

    code << "\tjump    " << GetNodeEntryLabel(firstNode) << "\r\n";

    // First generate all constants
    for (const auto & n : m_nodes)
    {
        code << n->GenerateConstants() << "\n";
    }

    for (const auto & n : m_nodes)
    {
        code << n->Build() << "\n";
    }

    ed::SetCurrentEditor(nullptr);
    return code.str();
}

std::list<std::shared_ptr<Connection>> NodeEditorWindow::GetNodeConnections(unsigned long nodeId)
{
    std::list<std::shared_ptr<Connection>> c;
    ed::SetCurrentEditor(m_context);

    for (const auto & l : m_links)
    {
        if (l->model->outNodeId == nodeId)
        {
            c.push_back(l->model);
        }
    }

    ed::SetCurrentEditor(nullptr);
    return c;
}

std::string NodeEditorWindow::GetNodeEntryLabel(unsigned long nodeId)
{
    std::string label;
    ed::SetCurrentEditor(m_context);

    for (const auto & n : m_nodes)
    {
        if (n->GetId() == nodeId)
        {
            label = n->GetEntryLabel();
            break;
        }
    }

    ed::SetCurrentEditor(nullptr);
    return label;
}


std::shared_ptr<BaseNode> NodeEditorWindow::GetSelectedNode()
{
    std::shared_ptr<BaseNode> selected;

    ed::SetCurrentEditor(m_context);
    if (ed::GetSelectedObjectCount() > 0)
    {
        ed::NodeId nId;
        int nodeCount = ed::GetSelectedNodes(&nId, 1);

        if (nodeCount > 0)
        {
            for (auto & n : m_nodes)
            {
                if (n->GetInternalId() == nId.Get())
                {
                    selected = n;
                }
            }
        }
    }
    ed::SetCurrentEditor(nullptr);

    return selected;
}


void NodeEditorWindow::Draw()
{
    if (WindowBase::BeginDraw())
    {

        ed::SetCurrentEditor(m_context);
        ed::Begin("My Editor", ImVec2(0.0, 0.0f));


        for (const auto & n : m_nodes)
        {
            ImGui::PushID(n->GetInternalId());
            n->Draw();
            ImGui::PopID();
        }

        for (const auto& linkInfo : m_links)
        {
            ed::Link(linkInfo->ed_link->Id, linkInfo->ed_link->OutputId, linkInfo->ed_link->InputId);
        }

        // Handle creation action, returns true if editor want to create new object (node or link)
        if (ed::BeginCreate())
        {
            ed::PinId inputPinId, outputPinId;
            if (ed::QueryNewLink(&inputPinId, &outputPinId))
            {
               // QueryNewLink returns true if editor want to create new link between pins.
               //
               // Link can be created only for two valid pins, it is up to you to
               // validate if connection make sense. Editor is happy to make any.
               //
               // Link always goes from input to output. User may choose to drag
               // link from output pin or input pin. This determine which pin ids
               // are valid and which are not:
               //   * input valid, output invalid - user started to drag new ling from input pin
               //   * input invalid, output valid - user started to drag new ling from output pin
               //   * input valid, output valid   - user dragged link over other pin, can be validated

               if (inputPinId && outputPinId) // both are valid, let's accept link
               {
                   // ed::AcceptNewItem() return true when user release mouse button.
                   if (ed::AcceptNewItem())
                   {
                       Connection model = LinkToModel(inputPinId, outputPinId);

                       CreateLink(model, inputPinId, outputPinId);

                       // Draw new link.
                       ed::Link(m_links.back()->ed_link->Id, inputPinId, outputPinId);
                   }

                   // You may choose to reject connection between these nodes
                   // by calling ed::RejectNewItem(). This will allow editor to give
                   // visual feedback by changing link thickness and color.
               }
            }
        }
        ed::EndCreate(); // Wraps up object creation action handling.


        // Handle deletion action
        if (ed::BeginDelete())
        {
            // There may be many links marked for deletion, let's loop over them.
            ed::LinkId deletedLinkId;
            while (ed::QueryDeletedLink(&deletedLinkId))
            {
               // If you agree that link can be deleted, accept deletion.
               if (ed::AcceptDeletedItem())
               {

                   m_links.erase(std::remove_if(m_links.begin(),
                                  m_links.end(),
                                [deletedLinkId](std::shared_ptr<LinkInfo> inf)
                                {
                                    return inf->ed_link->Id == deletedLinkId;
                                }));
               }

               // You may reject link deletion by calling:
               // ed::RejectDeletedItem();
            }
        }
        ed::EndDelete(); // Wrap up deletion action


        auto openPopupPosition = ImGui::GetMousePos();
        ed::Suspend();

        if (ed::ShowBackgroundContextMenu())
        {
            ImGui::OpenPopup("Create New Node");
        }

        if (ImGui::BeginPopup("Create New Node"))
        {
            auto newNodePostion = openPopupPosition;
            Node* node = nullptr;
            if (ImGui::MenuItem("Media Node"))
            {
                auto n = createNode("media-node", "", m_story);
                if (n)
                {
                    n->SetType("media-node"); // FIXME: set type in createNode factory?
                    n->SetId(GenerateNodeId());
                    n->SetPosition(newNodePostion.x, newNodePostion.y);
                    m_nodes.push_back(n);
                }
            }

            // if (node)
            // {
            //     ed::SetNodePosition(node->ID, newNodePostion);
            // }

            ImGui::EndPopup();
        }

        ed::Resume();


        ed::End();
        ed::SetCurrentEditor(nullptr);

    }

    WindowBase::EndDraw();
}

void NodeEditorWindow::ToolbarUI()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(42, Gui::GetWindowSize().h));

    ImGuiWindowFlags window_flags = 0
                                    | ImGuiWindowFlags_NoTitleBar
                                    | ImGuiWindowFlags_NoResize
                                    | ImGuiWindowFlags_NoMove
                                    | ImGuiWindowFlags_NoScrollbar
                                    | ImGuiWindowFlags_NoSavedSettings
        ;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("TOOLBAR", NULL, window_flags);
    ImGui::PopStyleVar();

    if (ImGui::Button(ICON_FA_COG))
    {
        ImGui::OpenPopup("Options");
    }

    if (ImGui::Button(ICON_FA_SIGN_OUT_ALT))
    {
       // mEvent.ExitGame();
    }


    ImGui::End();
}
