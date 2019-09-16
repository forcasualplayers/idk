#pragma once
#include <editor/windows/IGE_IWindow.h>
#include <imgui/imnodes/ImNodes.h>
#include <res/ResourceHandle.h>

namespace idk
{
    namespace shadergraph
    {
        struct Node;
        struct Value;
        class Graph;
    }

    class IGE_MaterialEditor : public IGE_IWindow
    {
    public:
        IGE_MaterialEditor();

        virtual void Initialize() override;
        virtual void BeginWindow() override;
        virtual void Update() override;

    private:
        ImNodes::CanvasState canvas;
        RscHandle<shadergraph::Graph> graph;
        vector<Guid> to_delete;

        void show_params_window();

        void drawValue(shadergraph::Value& value);
        void addDefaultSlotValue(const Guid& guid, int slot_in);

        void drawNode(shadergraph::Node& node);
        void addNode(const string& name, vec2 pos);
        void removeNode(const shadergraph::Node& node);
        void disconnectNode(const shadergraph::Node& node);

        void addParamNode(int param_index, vec2 pos);
        void removeParam(int param_index);
    };

}