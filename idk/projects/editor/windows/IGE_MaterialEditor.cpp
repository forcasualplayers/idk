#include "pch.h"

#include "IGE_MaterialEditor.h"
#include <editor/IDE.h>
#include <editor/windows/IGE_ProjectWindow.h>
#include <editor/widgets/InputResource.h>
#include <editor/widgets/EnumCombo.h>
#include <editor/DragDropTypes.h>
#include <app/Keys.h>
#include <gfx/ShaderGraph.h>
#include <gfx/ShaderGraph_helpers.h>
#include <gfx/MaterialInstance.h>
#include <gfx/GraphicsSystem.h>
#include <serialize/text.inl>
#include <reflect/reflect.inl>
#include <file/FileSystem.h>
#include <res/ResourceHandle.inl>
#include <res/ResourceManager.inl>
#include <res/ResourceUtils.inl>
#include <res/Guid.inl>

#include <regex>
namespace fs = std::filesystem;

namespace idk
{
    using namespace idk::shadergraph;

    static array<ImColor, ValueType::count + 1> type_colors
    {
        0,
        0xffEBC05B,
        0xff3DC59B,
        0xff4CE7FD,
        0xff2179FA,
        0xff0631CE
    };

    enum class TypeMatch
    {
        // Upcast = lower dimension to higher dimension
        None = 0, Downcast, Upcast, Match
    };
    // vecs can upcast to any dimension higher, but can only downcast to 1d
    static TypeMatch type_match(int t1, int t2)
    {
        if (t1 == t2) return TypeMatch::Match;
        if (t1 == ValueType::SAMPLER2D || t2 == ValueType::SAMPLER2D) return TypeMatch::None;
        if (t2 < t1) return TypeMatch::Downcast;
        if (t1 == ValueType::FLOAT) return TypeMatch::Upcast;
        return TypeMatch::None;
    }

    // similar to c++ function matching,
    // we match each param for each signature to determine the best matching function
//#pragma optimize("", off)
    static const NodeSignature* match_node_sig(const Node& node)
    {
        auto& tpl = NodeTemplate::GetTable().at(node.name);
        const size_t in_sz = node.input_slots.size();

        const NodeSignature* best_matched_sig = nullptr;
        vector<TypeMatch> best_match_values(in_sz, TypeMatch::None);
        for (auto& sig : tpl.signatures)
        {
            vector<TypeMatch> curr_match_values(in_sz, TypeMatch::Match);
            for (int i = 0; i < in_sz; ++i)
            {
                if (!node.input_slots[i].value.empty()) // disconnected
                {
                    curr_match_values[i] = TypeMatch::Match; // can freely change type
                    continue;
                }
                auto match = type_match(node.input_slots[i].type, sig.ins[i]);
                if (match < curr_match_values[i])
                    curr_match_values[i] = match;
            }

            std::sort(curr_match_values.begin(), curr_match_values.end());
            bool is_better_match = true;
            for (int i = 0; i < in_sz; ++i)
            {
                if (curr_match_values[i] == TypeMatch::None || curr_match_values[i] < best_match_values[i])
                {
                    is_better_match = false;
                    break;
                }
            }

            if (is_better_match)
            {
                std::swap(best_match_values, curr_match_values);
                best_matched_sig = &sig;
            }
        }

        return best_matched_sig;
    }

    // force input slot to be kind_in
    static ValueType match_node_sig_for_slot(const Node& node, int slot, ValueType kind_in)
    {
        auto& tpl = NodeTemplate::GetTable().at(node.name);
        const size_t in_sz = node.input_slots.size();

        const NodeSignature* best_matched_sig = nullptr;
        vector<TypeMatch> best_match_values(in_sz, TypeMatch::None);
        for (auto& sig : tpl.signatures)
        {
            vector<TypeMatch> curr_match_values(in_sz, TypeMatch::Match);
            for (int i = 0; i < in_sz; ++i)
            {
                if (slot != i && !node.input_slots[i].value.empty()) // disconnected
                {
                    curr_match_values[i] = TypeMatch::Match; // can freely change type
                    continue;
                }
                auto match = type_match(slot == i ? kind_in : node.input_slots[i].type, sig.ins[i]);
                if (match < curr_match_values[i])
                    curr_match_values[i] = match;
            }

            std::sort(curr_match_values.begin(), curr_match_values.end());
            bool is_better_match = true;
            for (int i = 0; i < in_sz; ++i)
            {
                if (curr_match_values[i] == TypeMatch::None || curr_match_values[i] < best_match_values[i])
                {
                    is_better_match = false;
                    break;
                }
            }

            if (is_better_match)
            {
                std::swap(best_match_values, curr_match_values);
                best_matched_sig = &sig;
            }
        }

        return best_matched_sig ? best_matched_sig->ins[slot] : static_cast<ValueType>(ValueType::INVALID);
    }



    static const char* slot_suffix(ValueType type)
    {
        switch (type)
        {
        case ValueType::FLOAT:      return "(1)";
        case ValueType::VEC2:       return "(2)";
        case ValueType::VEC3:       return "(3)";
        case ValueType::VEC4:       return "(4)";
        case ValueType::SAMPLER2D:  return "(T)";
        default: throw;
        }
    }

    bool draw_slot(const char* title, int kind, ValueType converted_slot_type = 0)
    {
        auto* canvas = ImNodes::GetCurrentCanvas();

        auto* storage = ImGui::GetStateStorage();
        const auto& style = ImGui::GetStyle();
        const float CIRCLE_RADIUS = 5.f * canvas->zoom;
        ImVec2 title_size = ImGui::CalcTextSize(title);

        // Pull entire slot a little bit out of the edge so that curves connect into int without visible seams
        float item_offset_x = style.ItemSpacing.x * canvas->zoom;
        if (!ImNodes::IsOutputSlotKind(kind))
            item_offset_x = -item_offset_x;
        ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2{ item_offset_x, 0 });

        if (ImNodes::IsOutputSlotKind(kind))
        {
            // Align output slots to the right edge of the node.
            ImGuiID max_width_id = ImGui::GetID("output-max-title-width");
            float offset = storage->GetFloat(max_width_id, title_size.x) - title_size.x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        }

        if (ImNodes::BeginSlot(title, kind))
        {
            auto* draw_lists = ImGui::GetWindowDrawList();

            // Slot appearance can be altered depending on curve hovering state.
            bool is_active = ImNodes::IsSlotCurveHovered() ||
                (ImNodes::IsConnectingCompatibleSlot() /*&& !IsAlreadyConnectedWithPendingConnection(title, kind)*/);

            ImColor kindColor = converted_slot_type ? type_colors[converted_slot_type] : type_colors[std::abs(kind)];
            ImColor text_color = ImGui::GetColorU32(ImGuiCol_Text);//is_active ? kindColor : canvas->colors[ImNodes::ColConnection];
            if (!is_active) text_color.Value.w = 0.7f;
            ImColor color = !is_active ? canvas->colors[ImNodes::ColConnection] :
                (converted_slot_type ? type_colors[converted_slot_type] : type_colors[std::abs(kind)]);

            string label = title;
            label += slot_suffix(converted_slot_type ? converted_slot_type : static_cast<ValueType>(abs(kind)));

            ImGui::PushStyleColor(ImGuiCol_Text, text_color.Value);

            if (ImNodes::IsOutputSlotKind(kind))
            {
                ImGui::TextUnformatted(label.c_str());
                ImGui::SameLine();
            }

            ImRect circle_rect{
                ImGui::GetCursorScreenPos(),
                ImGui::GetCursorScreenPos() + ImVec2{CIRCLE_RADIUS * 2, CIRCLE_RADIUS * 2}
            };
            // Vertical-align circle in the middle of the line.
            float circle_offset_y = title_size.y / 2.f - CIRCLE_RADIUS;
            circle_rect.Min.y += circle_offset_y;
            circle_rect.Max.y += circle_offset_y;
            draw_lists->AddCircleFilled(circle_rect.GetCenter(), CIRCLE_RADIUS, color);

            ImGui::ItemSize(circle_rect.GetSize());
            ImGui::ItemAdd(circle_rect, ImGui::GetID(title));

            if (ImNodes::IsInputSlotKind(kind))
            {
                ImGui::SameLine();
                ImGui::TextUnformatted(label.c_str());
            }

            ImGui::PopStyleColor();
            ImNodes::EndSlot();

            // A dirty trick to place output slot circle on the border.
            ImGui::GetCurrentWindow()->DC.CursorMaxPos.x -= item_offset_x;
            return true;
        }
        return false;
    }



    static bool draw_value_draw_tex_input(const char* id, vec2 pos, RscHandle<Texture>* tex)
    {
        auto canvas = ImNodes::GetCurrentCanvas();
        auto& style = ImGui::GetStyle();

        const float itemw = 80.0f + style.ItemSpacing.x;
        pos.x -= itemw + 4.0f;

        auto screen_pos = ImGui::GetWindowPos() + pos * canvas->zoom + canvas->offset;

        ImGui::SetCursorScreenPos(screen_pos + ImVec2{ 1.0f, 1.0f });

        ImRect rect = { screen_pos, screen_pos + ImVec2{ itemw * canvas->zoom, ImGui::GetTextLineHeight() + 2.0f } };

        ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max,
                                                  canvas->colors[ImNodes::ColNodeActiveBg], style.FrameRounding);
        ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max,
                                            canvas->colors[ImNodes::ColNodeActiveBg], style.FrameRounding);

        ImGui::PushItemWidth(itemw * canvas->zoom);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 1.0f, 0.0f });

        bool ret = ImGuidk::InputResource(("##" + std::string(id)).c_str(), tex);

        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        return ret;
    }
    static bool draw_value_draw_vec(const char* id, vec2 pos, int n, float* f)
    {
        auto canvas = ImNodes::GetCurrentCanvas();
        auto& style = ImGui::GetStyle();

        const float itemw = (40.0f + style.ItemSpacing.x) * n;
        pos.x -= itemw + 4.0f;

        auto screen_pos = ImGui::GetWindowPos() + pos * canvas->zoom + canvas->offset;

        ImGui::SetCursorScreenPos(screen_pos + ImVec2{ 1.0f, 1.0f });

        ImRect rect = { screen_pos, screen_pos + ImVec2{ itemw * canvas->zoom, ImGui::GetTextLineHeight() + 2.0f } };

        ImGui::GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max,
                                                  canvas->colors[ImNodes::ColNodeActiveBg], style.FrameRounding);
        ImGui::GetWindowDrawList()->AddRect(rect.Min, rect.Max,
                                            canvas->colors[ImNodes::ColNodeActiveBg], style.FrameRounding);

        ImGui::PushItemWidth(itemw * canvas->zoom);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 1.0f, 0.0f });

        bool ret = false;

        switch (n)
        {
        case 1: ret = ImGui::DragFloat (("##" + std::string(id)).c_str(), f, 0.01f); break;
        case 2: ret = ImGui::DragFloat2(("##" + std::string(id)).c_str(), f, 0.01f); break;
        case 3: ret = ImGui::DragFloat3(("##" + std::string(id)).c_str(), f, 0.01f); break;
        case 4: ret = ImGui::DragFloat4(("##" + std::string(id)).c_str(), f, 0.01f); break;
        default: throw;
        }

        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        return ret;
    }

    void IGE_MaterialEditor::drawValue(Node& node, int input_slot_index)
    {
        auto pos = node.position;
        pos.y += (ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y) / _canvas.zoom * (input_slot_index + 1);

        auto& slot = node.input_slots[input_slot_index];
        if (slot.value.empty())
            return;

        auto id = string(node.guid) + serialize_text(input_slot_index);

        switch (slot.type)
        {

        case ValueType::FLOAT:
        {
            float f = std::stof(slot.value);
            if (draw_value_draw_vec(id.c_str(), pos, 1, &f))
                slot.value = std::to_string(f);
            if (ImGui::IsItemDeactivatedAfterEdit())
                _graph->Compile();
            break;
        }
        case ValueType::VEC2:
        {
            vec2 v = helpers::parse_vec2(slot.value);
            if (draw_value_draw_vec(id.c_str(), pos, 2, v.values))
                slot.value = helpers::serialize_value(v);
            if (ImGui::IsItemDeactivatedAfterEdit())
                _graph->Compile();
            break;
        }
        case ValueType::VEC3:
        {
            vec3 v = helpers::parse_vec3(slot.value);
            if (draw_value_draw_vec(id.c_str(), pos, 3, v.values))
                slot.value = helpers::serialize_value(v);
            if (ImGui::IsItemDeactivatedAfterEdit())
                _graph->Compile();
            break;
        }
        case ValueType::VEC4:
        {
            vec4 v = helpers::parse_vec4(slot.value);
            if (draw_value_draw_vec(id.c_str(), pos, 4, v.values))
                slot.value = helpers::serialize_value(v);
            if (ImGui::IsItemDeactivatedAfterEdit())
                _graph->Compile();
            break;
        }
        case ValueType::SAMPLER2D:
        {
            auto tex = helpers::parse_sampler2d(slot.value);
            if (draw_value_draw_tex_input(id.c_str(), pos, &tex))
            {
                slot.value = helpers::serialize_value(tex);
                _graph->Compile();
            }
            break;
        }

        default:
            break;
        }
    }

    void IGE_MaterialEditor::addDefaultSlotValue(const Guid& guid, int slot_in)
    {
        auto& node_in = _graph->nodes[guid];
        auto in_type = node_in.input_slots[slot_in].type;
        node_in.input_slots[slot_in].value = helpers::default_value(in_type);
    }



    void IGE_MaterialEditor::drawNode(Node& node)
    {
        bool is_master_node = node.guid == _graph->master_node;
        bool is_param_node = node.name[0] == '$';

        _canvas.colors[ImNodes::ColNodeBorder] = node.selected ? _canvas.colors[ImNodes::ColConnectionActive] : _canvas.colors[ImNodes::ColNodeBg];

        if (ImNodes::BeginNode(&node, reinterpret_cast<ImVec2*>(&node.position), &node.selected))
        {
            const auto slash_pos = node.name.find_last_of('\\');
            
            string title = node.name.c_str() + (slash_pos == std::string::npos ? 0 : slash_pos + 1);
            if (is_master_node)
                title += " (Master)";
            if (is_param_node)
                title = "Parameter";

            auto title_size = ImGui::CalcTextSize(title.c_str());
            float input_names_width = 0;
            float output_names_width = 0;

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x * 0.5f);
            ImGui::Text(title.c_str());

            ImGui::BeginGroup();

            if (is_param_node)
            {
                auto& param = _graph->parameters[std::stoi(node.name.data() + 1)];
                const auto& slot_name = param.name;
                auto slot_w = ImGui::CalcTextSize(slot_name.c_str()).x + ImGui::CalcTextSize(slot_suffix(param.type)).x;
                title_size.x = std::max(title_size.x, slot_w);
                ImGui::GetStateStorage()->SetFloat(ImGui::GetID("output-max-title-width"), title_size.x);

                draw_slot(slot_name.c_str(), param.type);
            }
            else
            {
                auto& tpl = NodeTemplate::GetTable().at(node.name);

                int i = 0;
                size_t num_slots = node.input_slots.size() + node.output_slots.size();

                // calc width of node
                for (auto& slot_name : tpl.names)
                {
                    if (i < node.input_slots.size())
                    {
                        const auto suffix_w = ImGui::CalcTextSize(slot_suffix(node.input_slots[i].type)).x;
                        input_names_width = std::max(input_names_width, ImGui::CalcTextSize(slot_name.c_str()).x + suffix_w);
                    }
                    else if (i < num_slots)
                    {
                        const auto suffix_w = ImGui::CalcTextSize(slot_suffix(node.output_slots[i - node.input_slots.size()].type)).x;
                        output_names_width = std::max(output_names_width, ImGui::CalcTextSize(slot_name.c_str()).x + suffix_w);
                    }
                    else
                        break;
                    ++i;
                }
                title_size.x = std::max(title_size.x, input_names_width + output_names_width);
                ImGui::GetStateStorage()->SetFloat(ImGui::GetID("output-max-title-width"), title_size.x);

                i = 0;
                float cursor_y = ImGui::GetCursorPosY();
                float max_slots_cursor_y = cursor_y;
                string control_values = node.control_values;
                string final_control_values;

                for (auto& slot_name : tpl.names)
                {
                    if (i == node.input_slots.size())
                        ImGui::SetCursorPosY(cursor_y);
                    ImGui::Spacing();

                    int kind = 0;
                    if (i < node.input_slots.size())
                    {
                        Node* node_out;
                        const char* title_out;
                        int kind_out;

                        kind = -node.input_slots[i].type;

                        if (ImNodes::GetPendingConnection(reinterpret_cast<void**>(&node_out), &title_out, &kind_out)
                            && kind_out > 0 && node_out != &node)
                        {
                            ValueType converted_slot_type = match_node_sig_for_slot(node, i, kind_out);

                            auto cursorpos = ImGui::GetCursorPos();
                            drawValue(node, i); // draw value control when disconnected
                            ImGui::SetCursorPos(cursorpos);
                            if (converted_slot_type != ValueType::INVALID)
                                draw_slot(slot_name.c_str(), -kind_out, converted_slot_type);
                            else
                                draw_slot(slot_name.c_str(), kind);
                        }
                        else
                        {
                            auto cursorpos = ImGui::GetCursorPos();
                            drawValue(node, i); // draw value control when disconnected
                            ImGui::SetCursorPos(cursorpos);
                            draw_slot(slot_name.c_str(), kind);
                        }

                        max_slots_cursor_y = ImMax(max_slots_cursor_y, ImGui::GetCursorPosY());
                    }
                    else if (i < num_slots)
                    {
                        kind = node.output_slots[i - node.input_slots.size()].type;
                        draw_slot(slot_name.c_str(), kind);

                        max_slots_cursor_y = ImMax(max_slots_cursor_y, ImGui::GetCursorPosY());
                    }
                    else // custom controls
                    {
                        assert(slot_name[0] == '$'); // did we fuck up the names in the .node?

                        if (i == num_slots)
                            ImGui::SetCursorPosY(max_slots_cursor_y);

                        string next_value = "";
                        if (control_values.size())
                        {
                            auto pos = control_values.find('|');
                            next_value = control_values.substr(0, pos);
                            if (pos != string::npos)
                                control_values.erase(0, pos + 1);
                            else
                                control_values.clear();
                        }

                        auto w = ImGui::GetStyle().ItemSpacing.x * 4 * _canvas.zoom + ImGui::GetStateStorage()->GetFloat(ImGui::GetID("output-max-title-width"));

                        ImGui::PushID(i);

                        if (slot_name == "$Color")
                        {
                            if (next_value.empty())
                                final_control_values += (next_value = "0,0,0,1") + '|';

                            vec4 v = helpers::parse_vec4(next_value);
                            if (ImGui::ColorButton("", v, 0, ImVec2(w, 0)))
                            {
                                ImGui::OpenPopup("picker");
                                ImGui::GetCurrentContext()->ColorPickerRef = v;
                            }
                            if (ImGui::BeginPopup("picker"))
                            {
                                ImGuiColorEditFlags picker_flags = ImGuiColorEditFlags__DisplayMask | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf;
                                ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 12.0f); // Use 256 + bar sizes?
                                if (ImGui::ColorPicker4("##picker", v.values, picker_flags, &ImGui::GetCurrentContext()->ColorPickerRef.x))
                                    final_control_values += helpers::serialize_value(v) + "|";

                                ImGui::EndPopup();
                            }
                        }
                        else if (slot_name.find("$Bool") != string::npos)
                        {
                            string inner = slot_name.substr(sizeof("$Bool"));
                            int def_val = 0;
                            string label;
                            if (inner.size())
                            {
                                const auto colon_pos = inner.find('=');
                                if (colon_pos == string::npos)
                                    def_val = inner[0] == '1';
                                else
                                {
                                    label = inner.substr(0, colon_pos);
                                    def_val = inner[colon_pos + 1] == '1';
                                }
                            }

                            if (next_value.empty())
                                final_control_values += (next_value = serialize_text(def_val));

                            if (label.size())
                            {
                                w -= ImGui::CalcTextSize(label.c_str()).x;
                                ImGui::Text(label.c_str());
                                ImGui::SameLine();
                            }
                            
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w - ImGui::GetFrameHeight());
                            bool value = next_value.size() == 1 && next_value[0] == '1';
                            if (ImGui::Checkbox("", &value))
                            {
                                next_value.resize(1);
                                next_value[0] = value ? '1' : '0';
                            }
                        }
                        else if (slot_name[1] == '[' && slot_name.back() == ']')
                        {
                            string inner = slot_name.substr(2, slot_name.size() - 3);

                            size_t pos = inner.find(':');
                            if (pos != string::npos)
                            {
                                auto txt = inner.substr(0, pos);
                                w -= ImGui::CalcTextSize(txt.c_str()).x;
                                ImGui::Text(txt.c_str());
                                ImGui::SameLine();
                                inner = inner.substr(pos);
                            }

                            // inner = :aaaaaaa|bbbbbbb|ccccccc...
                            pos = inner.find('|');

                            if (next_value.empty())
                                final_control_values += (next_value = inner.substr(1, pos - 1));

                            ImGui::SetNextItemWidth(w);
                            if (ImGui::BeginCombo("", next_value.c_str()))
                            {
                                pos = 0;
                                while (pos != string::npos)
                                {
                                    size_t new_pos = inner.find('|', pos + 1);
                                    string label = inner.substr(pos + 1, new_pos - pos - 1);
                                    if (ImGui::Selectable(label.c_str(), label == next_value))
                                        final_control_values += label + '|';
                                    pos = new_pos;
                                }
                                ImGui::EndCombo();
                            }
                        }

                        ImGui::PopID();
                    }

                    ++i;
                }

                if (final_control_values.size())
                    node.control_values = final_control_values;
            }

            ImGui::EndGroup();

            ImNodes::EndNode();
        }

        // if context menu is already open, iswindowhovered will return false
        auto can_open = ImGui::IsPopupOpen(ImGui::GetCurrentWindow()->DC.LastItemId) ||
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        can_open = can_open && !ImGui::IsMouseDragPastThreshold(1);
        auto fntscl = ImGui::GetCurrentWindow()->FontWindowScale;
        ImGui::SetWindowFontScale(1.0f);
        if (can_open && ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Disconnect"))
                disconnectNode(node);
            if (ImGui::MenuItem("Delete", NULL, false, !is_master_node))
            {
                removeNode(node);
            }
            ImGui::EndPopup();
        }
        ImGui::SetWindowFontScale(fntscl);
    }

    Node& IGE_MaterialEditor::addNode(const string& name, vec2 pos)
    {
        Node node;
        node.name = name;
        node.guid = Guid::Make();
        node.position = pos;

        auto sig = NodeTemplate::GetTable().at(name).signatures[0];
        for (auto in : sig.ins)
            node.input_slots.push_back({ in, helpers::default_value(in) });
        for (auto out : sig.outs)
            node.output_slots.push_back({ out });

        _node_order.push_back(node.guid);
        return _graph->nodes.emplace(node.guid, node).first->second;
    }

    void IGE_MaterialEditor::removeNode(const Node& node)
    {
        disconnectNode(node);
        _nodes_to_delete.push_back(node.guid);
    }

    void IGE_MaterialEditor::disconnectNode(const Node& node, bool disconnect_inputs, bool disconnect_outputs)
    {
        auto& g = *_graph;

        for (auto& link : g.links)
        {
            if ((disconnect_inputs && link.node_in == node.guid) || (disconnect_outputs && link.node_out == node.guid))
                addDefaultSlotValue(link.node_in, link.slot_in);
        }

        g.links.erase(std::remove_if(g.links.begin(), g.links.end(),
                                     [guid = node.guid, disconnect_inputs, disconnect_outputs](const Link& link) 
                                     {
                                         return (disconnect_inputs && link.node_in == guid) || 
                                             (disconnect_outputs && link.node_out == guid); 
                                     }),
                      g.links.end());
    }



    void IGE_MaterialEditor::addParamNode(int param_index, vec2 pos)
    {
        auto& param = _graph->parameters[param_index];

        Node node;
        node.name = "$" + std::to_string(param_index);
        node.guid = Guid::Make();
        node.position = pos;
        node.output_slots.push_back(Slot{ param.type });

        _graph->nodes.emplace(node.guid, node);
        _node_order.push_back(node.guid);
    }

    void IGE_MaterialEditor::removeParam(int param_index)
    {
        // since all params are referenced by indices,
        // we have to shift all param nodes value that reference greater indices
        // as when you erase a param, all params after that are shifted back by 1.

        vector<Guid> to_del;
        for (auto& [guid, node] : _graph->nodes)
        {
            if (node.name[0] != '$')
                continue;

            auto index = std::stoi(node.name.data() + 1);
            if (index > param_index)
                node.name = '$' + std::to_string(index - 1);
            else if (index == param_index)
                to_del.push_back(guid);
        }
        for (const auto& guid : to_del)
            removeNode(_graph->nodes[guid]);

        _graph->parameters.erase(_graph->parameters.begin() + param_index);
    }



    struct node_item { vector<node_item> items; string name; };

    static bool context_menu_node_item_filter(const node_item& item, const ImGuiTextFilter& filter)
    {
        if (filter.PassFilter(item.name.c_str()))
            return true;
        if (item.items.size())
        {
            for (const auto& i : item.items)
            {
                if (context_menu_node_item_filter(i, filter))
                    return true;
            }
        }
        return false;
    }

    static node_item* context_menu_node_item(node_item& item, std::vector<node_item*>& stack, const ImGuiTextFilter& filter, bool ignore_filter = false)
    {
        if (item.items.size())
        {
            auto folder_name = item.name;
            if (folder_name == "master") // skip master nodes
                return nullptr;
            if (!ignore_filter && !context_menu_node_item_filter(item, filter))
                return nullptr;
            if (!ignore_filter && filter.PassFilter(item.name.c_str())) // if folder name filtered, pass all descendants
                ignore_filter = true;

            if (stack.empty() || ImGui::TreeNodeEx(folder_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth))
            {
                stack.push_back(&item);
                for (auto& inner_item : item.items)
                {
                    auto* res = context_menu_node_item(inner_item, stack, filter, ignore_filter);
                    if (res)
                    {
                        if (item.name.size())
                            ImGui::TreePop();
                        return res;
                    }
                }
                stack.pop_back();

                if (stack.size())
                    ImGui::TreePop();
            }
        }
        else if (ignore_filter || context_menu_node_item_filter(item, filter))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
			ImGui::Unindent();
            ImGui::TreeNodeEx(item.name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
			ImGui::Indent();
			if (ImGui::IsItemClicked())
			{
				ImGui::PopStyleColor();
				return &item;
			}
            ImGui::PopStyleColor();
        }

        return nullptr;
    }

    static node_item init_templates_hierarchy()
    {
        node_item root;
		auto get_uppercase = [](const string& str)
		{
			auto ret = str;
			for (char& c : ret)
				c = static_cast<char>(toupper(c));
			return ret;
		};

        for (auto& tpl : NodeTemplate::GetTable())
        {
            node_item* curr = &root;
            fs::path path = tpl.first.sv();

            for (auto& part : path)
            {
                auto iter = std::find_if(curr->items.begin(), curr->items.end(), [&](node_item& item) { return item.name.sv() == part; });
                if (iter != curr->items.end())
                    curr = &*iter;
                else
                {
                    curr->items.push_back({ {}, part.string() });
                    curr = &curr->items.back();
                }
            }
        }
        std::vector<node_item*> stack{ &root };
        while (stack.size())
        {
            auto* item = stack.back();
            stack.pop_back();
            std::sort(item->items.begin(), item->items.end(),
				[get_uppercase](node_item& a, node_item& b)
				{ 
					return get_uppercase(a.name) < get_uppercase(b.name);
				});
            for (auto& inner_item : item->items)
            {
                stack.push_back(&inner_item);
            }
        }

        return root;
    }

    static string draw_nodes_context_menu()
    {
        static node_item templates_hierarchy = init_templates_hierarchy();
        std::vector<node_item*> stack;

        static ImGuiTextFilter filter;
        if (ImGui::IsWindowAppearing())
        {
            filter.Clear();
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetFrameHeight() * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_Border));
        filter.Draw("", ImGui::GetWindowContentRegionWidth());
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::BeginChild("nodes");
        auto* item = context_menu_node_item(templates_hierarchy, stack, filter);
        ImGui::EndChild();
        
        if (item)
        {
            fs::path path;
            // build path
            for (auto iter = stack.begin() + 1; iter != stack.end(); ++iter)
                path /= (*iter)->name.sv();
            path /= item->name.sv();

            return path.string();
        }

        return "";
    }



    void IGE_MaterialEditor::OpenGraph(const RscHandle<shadergraph::Graph>& handle)
    {
		_graph = handle;
        _node_order.clear();
        for (auto& pair : _graph->nodes)
            _node_order.push_back(pair.first);

        ImGui::SetWindowFocus(window_name);
        is_open = true;
    }



    IGE_MaterialEditor::IGE_MaterialEditor()
        : IGE_IWindow("Material Editor", false, ImVec2{ 600,300 }, ImVec2{ 0,0 })
    {
        window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar;
    }

    void IGE_MaterialEditor::Initialize()
    {
        _canvas.colors[ImNodes::ColNodeBg] = ImGui::GetColorU32(ImGuiCol_Border);
        _canvas.colors[ImNodes::ColNodeBg].Value.w = 0.75f;
        _canvas.colors[ImNodes::ColNodeActiveBg] = ImGui::GetColorU32(ImGuiCol_Border);
        _canvas.colors[ImNodes::ColNodeActiveBg].Value.w = 0.75f;
        _canvas.style.curve_thickness = 2.5f;

        Core::GetSystem<IDE>().FindWindow<IGE_ProjectWindow>()->OnAssetDoubleClicked.Listen([&](GenericResourceHandle handle)
        {
            if (handle.resource_id() == BaseResourceID<Graph>)
                OpenGraph(handle.AsHandle<Graph>());
        });
    }

    void IGE_MaterialEditor::BeginWindow()
    {
    }

    void IGE_MaterialEditor::Update()
    {
        if (!is_open || ImGui::IsWindowCollapsed())
        {
            ImNodes::BeginCanvas(&_canvas);
            ImNodes::EndCanvas();
            return;
        }

        if (!_graph)
        {
            ImNodes::BeginCanvas(&_canvas);
            ImNodes::EndCanvas();
            return;
        }

        auto g = _graph;

        if(ImGui::IsKeyDown(static_cast<int>(Key::Delete)))
        {
            for (const auto& [guid, node] : g->nodes)
            {
                if (node.selected && guid != g->master_node)
                    removeNode(node);
            }
        }

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::Button("Compile & Save"))
            {
                g->Compile();
                Core::GetResourceManager().Save(_graph);
				g = _graph;
            }
            ImGui::EndMenuBar();
        }


        ImGui::Columns(2);
        if (ImGui::IsWindowAppearing())
            ImGui::SetColumnWidth(-1, 200);

        drawLeftColumn();

        ImGui::NextColumn();

        ImGui::BeginChild("Canvas", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        auto window_pos = ImGui::GetWindowPos();

        ImGui::SetWindowFontScale(1.0f);
        if (_released_link_for_pending_new_node.node_in || _released_link_for_pending_new_node.node_out ||
            (ImGui::IsMouseReleased(1) && !ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered()))
        {
            ImGui::OpenPopup("nodes_context_menu");
        }
        if (ImGui::IsPopupOpen("nodes_context_menu"))
            ImGui::SetNextWindowSizeConstraints(ImVec2{ 200, 320 }, ImVec2{ 200, 320 });
        if (ImGui::BeginPopup("nodes_context_menu"))
        {
            auto str = draw_nodes_context_menu();
            if (str.size())
            {
                ImGui::CloseCurrentPopup();
                auto pos = (ImGui::GetWindowPos() - window_pos - _canvas.offset) / _canvas.zoom;
                auto& node = addNode(str, pos);
                // pos = windowpos + nodepos * zoom + offset
                // nodepos = (screenpos - offset - windowpos) / zoom

                if (_released_link_for_pending_new_node.node_out)
                {
                    const auto& slot_out = g->nodes[_released_link_for_pending_new_node.node_out].output_slots[_released_link_for_pending_new_node.slot_out];
                    for (int i = 0; i < node.input_slots.size(); ++i)
                    {
                        auto converted = match_node_sig_for_slot(node, i, slot_out.type);
                        if (converted != ValueType::INVALID)
                        {
                            _released_link_for_pending_new_node.node_in = node.guid;
                            _released_link_for_pending_new_node.slot_in = i;
                            g->links.push_back(_released_link_for_pending_new_node);

                            node.input_slots[i].type = converted;
                            node.input_slots[i].value.clear();
                            if (const auto* sig = match_node_sig(node))
                            {
                                for (size_t j = 0; j < node.input_slots.size(); ++j)
                                    node.input_slots[j].type = sig->ins[j];
                                for (size_t j = 0; j < node.output_slots.size(); ++j)
                                    node.output_slots[j].type = sig->outs[j];
                            }
                            break;
                        }
                    }
                }
                else if (_released_link_for_pending_new_node.node_in) // want to connect to an output slot
                {
                    const auto& slot_in = g->nodes[_released_link_for_pending_new_node.node_in].input_slots[_released_link_for_pending_new_node.slot_in];
                    for (int i = 0; i < node.output_slots.size(); ++i)
                    {
                        const auto match = type_match(node.output_slots[i].type, slot_in.type);
                        if (match > TypeMatch::None)
                        {
                            _released_link_for_pending_new_node.node_out = node.guid;
                            _released_link_for_pending_new_node.slot_out = i;
                            g->links.push_back(_released_link_for_pending_new_node);
                            break;
                        }
                    }
                }

                _released_link_for_pending_new_node.node_in = {};
                _released_link_for_pending_new_node.node_out = {};
            }
            ImGui::EndPopup();

            if (ImGui::IsMouseClicked(0))
            {
                _released_link_for_pending_new_node.node_in = {};
                _released_link_for_pending_new_node.node_out = {};
            }
        }

        for (auto& guid : _nodes_to_delete)
        {
            _node_order.erase(std::find(_node_order.begin(), _node_order.end(), guid));
            g->nodes.erase(guid);
        }
        _nodes_to_delete.clear();

        ImNodes::BeginCanvas(&_canvas);

		drawLinks();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        auto end = _node_order.end();
        auto iter = _node_order.begin();
        while (iter != end)
        {
			auto& deref = *iter;
			auto& node = g->nodes.at(deref);
            bool was_selected = node.selected;
            drawNode(node);
            if (!was_selected && node.selected)
            {
                // rotate node onto end of _node_order (so it will be drawn at the top next frame)
                // then decrement end so it will not be drawn twice this frame, and draw the new iter-pointed node
                std::rotate(iter, iter + 1, _node_order.end());
                --end;
                continue;
            }
            ++iter;
        }

        for(auto& guid : _nodes_to_delete)
            g->nodes.erase(guid);

        ImGui::PopStyleVar();

        auto connection_col_active = _canvas.colors[ImNodes::ColConnectionActive];
        {
            Node* node;
            const char* title;
            int kind;

            if (ImNodes::GetPendingConnection(r_cast<void**>(&node), &title, &kind))
            {
                _canvas.colors[ImNodes::ColConnectionActive] = type_colors[std::abs(kind)];
                if (!ImGui::IsMouseDown(0))
                {
                    if (kind < 0)
                    {
                        _released_link_for_pending_new_node.node_in = node->guid;
                        _released_link_for_pending_new_node.node_out = {};
                        _released_link_for_pending_new_node.slot_in = (int)NodeTemplate::GetTable().at(node->name).GetSlotIndex(title);
                        _released_link_for_pending_new_node.slot_out = -1;
                    }
                    else
                    {
                        _released_link_for_pending_new_node.node_out = node->guid;
                        _released_link_for_pending_new_node.node_in = {};
                        const bool out_is_param = node->name[0] == '$';
                        _released_link_for_pending_new_node.slot_out = out_is_param ? 0 : (int)NodeTemplate::GetTable().at(node->name).GetSlotIndex(title);
                        _released_link_for_pending_new_node.slot_in = -1;
                    }



                }
                    ImGui::OpenPopup("nodes_context_menu");
            }
        }

        handleNewLink();

        ImNodes::EndCanvas();

        ImGui::EndChild();

        _canvas.colors[ImNodes::ColConnectionActive] = connection_col_active;

        if (ImGui::BeginDragDropTarget())
        {
            if (const auto* payload = ImGui::AcceptDragDropPayload(DragDrop::PARAMETER, ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
            {
                addParamNode(*reinterpret_cast<int*>(payload->Data), (ImGui::GetMousePos() - ImGui::GetItemRectMin() - _canvas.offset) / _canvas.zoom);
            }
            ImGui::EndDragDropTarget();
        }

    }

    void IGE_MaterialEditor::handleNewLink()
    {
        Graph& g = *_graph;

        Node* node_out;
        Node* node_in;
        const char* title_out;
        const char* title_in;
        if (ImNodes::GetNewConnection(r_cast<void**>(&node_in), &title_in, r_cast<void**>(&node_out), &title_out))
        {
            int slot_in = (int)NodeTemplate::GetTable().at(node_in->name).GetSlotIndex(title_in);
            int slot_out = node_out->name[0] == '$' ? 0 : (int)NodeTemplate::GetTable().at(node_out->name).GetSlotIndex(title_out);

            // check if any link inputs into input slot (cannot have multiple)
            for (auto iter = g.links.begin(); iter != g.links.end(); ++iter)
            {
                if (iter->node_in == node_in->guid && iter->slot_in == slot_in)
                {
                    g.links.erase(iter);
                    break;
                }
            }

            g.links.push_back({ node_out->guid, node_in->guid, slot_out, slot_in });

            // clear any unconnected value
            node_in->input_slots[slot_in].value.clear();
            // set new type
            node_in->input_slots[slot_in].type = node_out->output_slots[slot_out - node_out->input_slots.size()].type;

            bool out_type_changed = false;

            // resolve new node_in signature
            if (const auto* sig = match_node_sig(*node_in))
            {
                for (size_t i = 0; i < node_in->input_slots.size(); ++i)
                    node_in->input_slots[i].type = sig->ins[i];
                for (size_t i = 0; i < node_in->output_slots.size(); ++i)
                {
                    if (node_in->output_slots[i].type != sig->outs[i])
                    {
                        node_in->output_slots[i].type = sig->outs[i];
                        out_type_changed = true;
                    }
                }
            }

            if (out_type_changed)
                disconnectNode(*node_in, false, true);

            _released_link_for_pending_new_node.node_in = {};
            _released_link_for_pending_new_node.node_out = {};

            g.Compile();
        }
    }

    void IGE_MaterialEditor::drawLinks()
    {
        Graph& g = *_graph;

        auto link_to_delete = g.links.end();
        for (auto iter = g.links.begin(); iter != g.links.end(); ++iter)
        {
            auto& link = *iter;

            auto& node_out = g.nodes[link.node_out];
            auto& node_in = g.nodes[link.node_in];
            auto col = _canvas.colors[ImNodes::ColConnection];
            auto col2 = _canvas.colors[ImNodes::ColConnection2];
            _canvas.colors[ImNodes::ColConnection] = type_colors[std::abs(node_out.output_slots[link.slot_out - node_out.input_slots.size()].type)];
            _canvas.colors[ImNodes::ColConnection2] = type_colors[std::abs(node_in.input_slots[link.slot_in].type)];

            const auto& slot_out = node_out.name[0] == '$' ?
                g.parameters[std::stoi(node_out.name.data() + 1)].name :
                NodeTemplate::GetTable().at(node_out.name).names[link.slot_out];

            if (!ImNodes::Connection(&node_in, NodeTemplate::GetTable().at(node_in.name).names[link.slot_in].c_str(),
                &node_out, slot_out.c_str()))
            {
                link_to_delete = iter;
            }
            _canvas.colors[ImNodes::ColConnection] = col;
            _canvas.colors[ImNodes::ColConnection2] = col2;
        }
        if (link_to_delete != g.links.end())
        {
            addDefaultSlotValue(link_to_delete->node_in, link_to_delete->slot_in);
            g.links.erase(link_to_delete);
            g.Compile();
        }
    }



    string IGE_MaterialEditor::genUniqueParamName(string_view base)
    {
        const auto& params = _graph->parameters;
        string new_name{ base };
        int i = 0;
        while (std::find_if(params.begin(), params.end(),
                            [&new_name](const Parameter& p) { return p.name == new_name; }) != params.end())
        {
            new_name = base;
            new_name += serialize_text(++i);
        }
        return new_name;
    }

    void IGE_MaterialEditor::drawLeftColumn()
    {
        ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4.0f));
        ImGui::BeginChild("LeftChild", ImVec2(), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar();

        auto graph_name = Core::GetResourceManager().GetPath(_graph);
        ImGui::Text(PathHandle(*graph_name).GetStem().data());

        if (ImGui::CollapsingHeader("Master", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& meta = *_graph;
            bool changed = false;
            changed = changed || ImGuidk::EnumCombo("Domain", &meta.domain);
            changed = changed || ImGuidk::EnumCombo("Blend", &meta.blend);
            changed = changed || ImGuidk::EnumCombo("Model", &meta.model);
            if (changed)
            {
                auto master_node = _graph->nodes[_graph->master_node];
                auto new_master_node_name = master_node.name;
				switch (meta.model)
				{
				case ShadingModel::DefaultLit: new_master_node_name = "master\\PBR";          break;
				case ShadingModel::Unlit:      new_master_node_name = (meta.blend==BlendMode::Masked)?"master\\UnlitMasked" : "master\\Unlit";        break;
				case ShadingModel::Specular:   new_master_node_name = "master\\PBR Specular"; break;
				}
                if (master_node.name != new_master_node_name)
                {
                    auto pos = master_node.position;
                    removeNode(master_node);
                    _graph->master_node = addNode(new_master_node_name, pos).guid;
                }
            }
        }

        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static char buf[32];
            static int renaming = 0;
            static bool just_rename = false;
            int i = 0;
            int to_del = 0;
            for (auto& param : _graph->parameters)
            {
                ++i;

                ImGui::PushID(i);
                ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(2.0f, 2.0f));

                strcpy_s(buf, param.name.c_str());
                string label{ param.type.to_string() };
                if (param.type == ValueType::SAMPLER2D)
                    label = "TEXTURE";

                auto label_sz = ImGui::CalcTextSize(label.c_str());

                float h = ImGui::GetTextLineHeightWithSpacing();
                float bw = ImGui::GetWindowContentRegionWidth() * 0.75f;

                if (renaming == i)
                {
                    ImGui::PushItemWidth(bw);
                    if (ImGui::InputText("##rename", buf, 32, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        renaming = -1;
                        string new_name = genUniqueParamName(buf);

                        { // replace name in graph's uniform table
                            auto iter = _graph->uniforms.find(param.name);
                            if (iter != _graph->uniforms.end())
                            {
                                auto node = _graph->uniforms.extract(iter);
                                node.key() = new_name;
                                _graph->uniforms.insert(std::move(node));
                            }
                        }

                        // replace name of all material instances with overriden uniform
                        for (auto& mat_inst : Core::GetResourceManager().GetAll<MaterialInstance>())
                        {
                            if (mat_inst->material.guid == _graph.guid)
                            {
                                auto iter = mat_inst->uniforms.find(param.name);
                                if (iter != mat_inst->uniforms.end())
                                {
                                    auto node = mat_inst->uniforms.extract(iter);
                                    node.key() = new_name;
                                    mat_inst->uniforms.insert(std::move(node));
                                    mat_inst->Dirty();
                                }
                            }
                        }

                        std::swap(param.name, new_name);
                        _graph->Compile();
                        _graph->Dirty();
                    }
                    ImGui::PopItemWidth();

                    if (just_rename)
                    {
                        ImGui::SetKeyboardFocusHere(-1);
                        just_rename = false;
                    }
                    else if (ImGui::IsItemDeactivated())
                        renaming = 0;
                }
                else
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, h * 0.5f);
                    ImGui::Button(param.name.c_str(), ImVec2(bw, h));
                    ImGui::PopStyleVar();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                    {
                        renaming = i;
                        just_rename = true;
                    }

                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        ImGui::Text(param.name.c_str());
                        int param_index = i - 1;
                        ImGui::SetDragDropPayload(DragDrop::PARAMETER, &param_index, sizeof(i));
                        ImGui::EndDragDropSource();
                    }

                    string id = "context_" + i;
                    if (ImGui::BeginPopupContextItem(id.c_str()))
                    {
                        if (ImGui::MenuItem("Delete"))
                        {
                            to_del = i;
                        }
                        if (ImGui::MenuItem("Rename"))
                        {
                            renaming = i;
                            just_rename = true;
                        }
                        ImGui::EndPopup();
                    }
                }

                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - label_sz.x - ImGui::GetStyle().FramePadding.x);
                ImGui::Text(label.c_str());

                ImGui::Text("Default");
                ImGui::SameLine();
                auto default_text_sz = ImGui::CalcTextSize("Default");

                ImGui::PushItemWidth(ImGui::GetWindowContentRegionWidth() - default_text_sz.x - ImGui::GetStyle().ItemSpacing.x);

                switch (param.type)
                {
                case ValueType::FLOAT:
                {
                    float f = helpers::parse_float(param.default_value);
                    if (ImGui::DragFloat("", &f, 0.01f))
                        param.default_value = helpers::serialize_value(f);
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        _graph->Compile();
                    break;
                }
                case ValueType::VEC2:
                {
                    vec2 v = helpers::parse_vec2(param.default_value);
                    if (ImGui::DragFloat2("", v.values, 0.01f))
                        param.default_value = helpers::serialize_value(v);
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        _graph->Compile();
                    break;
                }
                case ValueType::VEC3:
                {
                    vec3 v = helpers::parse_vec3(param.default_value);
                    if (ImGui::DragFloat3("", v.values, 0.01f))
                        param.default_value = helpers::serialize_value(v);
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        _graph->Compile();
                    break;
                } 
                case ValueType::VEC4:
                {
                    vec4 v = helpers::parse_vec4(param.default_value);
                    if (ImGui::DragFloat4("", v.values, 0.01f))
                        param.default_value = helpers::serialize_value(v);
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        _graph->Compile();
                    break;
                }
                case ValueType::SAMPLER2D:
                {
                    RscHandle<Texture> tex = helpers::parse_sampler2d(param.default_value);
                    if (ImGuidk::InputResource("", &tex))
                    {
                        param.default_value = helpers::serialize_value(tex);
                        _graph->Compile();
                    }
                    break;
                }
                default:
                    break;
                }

                ImGui::PopItemWidth();
                ImGui::PopID();

                ImGui::Separator();
            }

            if (to_del)
                removeParam(to_del - 1);

            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() * 0.5f -
                (ImGui::CalcTextSize("Add Parameter").x + ImGui::GetStyle().FramePadding.x * 2) * 0.5f);
            if (ImGui::Button("Add Parameter"))
                ImGui::OpenPopup("addparams");
            if (ImGui::IsPopupOpen("addparams"))
            {
                ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));
                ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 0));
            }
            if (ImGui::BeginPopup("addparams"))
            {
                if (ImGui::MenuItem("Float"))   _graph->parameters.emplace_back(Parameter{ genUniqueParamName("NewFloat"),   ValueType::FLOAT,     "0" });
                if (ImGui::MenuItem("Vec2"))    _graph->parameters.emplace_back(Parameter{ genUniqueParamName("NewVec2"),    ValueType::VEC2,      "0,0" });
                if (ImGui::MenuItem("Vec3"))    _graph->parameters.emplace_back(Parameter{ genUniqueParamName("NewVec3"),    ValueType::VEC3,      "0,0,0" });
                if (ImGui::MenuItem("Vec4"))    _graph->parameters.emplace_back(Parameter{ genUniqueParamName("NewVec4"),    ValueType::VEC4,      "0,0,0,0" });
                if (ImGui::MenuItem("Texture")) _graph->parameters.emplace_back(Parameter{ genUniqueParamName("NewTexture"), ValueType::SAMPLER2D, string(Guid()) });
                ImGui::EndPopup();
            }

        } // collapsing header parameters

        ImGui::EndChild();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

}