#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#ifdef _MSC_VER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

//#include "glad/glad.h"
#include "GL/gl3w.h"

#include "ax/Builders.h"
#include "ax/Widgets.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "imgui.h"
#include "imgui_internal.h"

#include "colormap.hh"
#include "gui_component.hh"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

namespace util = ax::NodeEditor::Utilities;

namespace nnview {

using ax::Widgets::IconType;

inline uint8_t ftoc(const float x) {
  int i = int(x * 255.0f);
  i = std::min(255, std::max(0, i));
  return uint8_t(i);
}

static std::vector<uint8_t> tensor_to_color(const nnview::Tensor &tensor) {
  std::vector<uint8_t> img;
  img.resize(size_t(tensor.shape[0] * tensor.shape[1] * 4));

  // find max/min value
  float min_value = std::numeric_limits<float>::max();
  float max_value = -std::numeric_limits<float>::max();

  for (size_t i = 0; i < size_t(tensor.shape[0] * tensor.shape[1]); i++) {
    min_value = std::min(min_value, tensor.data[i]);
    max_value = std::max(max_value, tensor.data[i]);
  }

  std::cout << "tensor min/max = " << min_value << ", " << max_value
            << std::endl;

  for (size_t i = 0; i < size_t(tensor.shape[0] * tensor.shape[1]); i++) {
    // normalize.
    const float x = (tensor.data[i] - min_value) / (max_value - min_value);
    nnview::vec3 rgb = nnview::viridis(x);

    // std::cout << rgb[0] << ", " << rgb[1] << ", " << rgb[2] << std::endl;

    img[4 * i + 0] = ftoc(rgb[0]);
    img[4 * i + 1] = ftoc(rgb[1]);
    img[4 * i + 2] = ftoc(rgb[2]);
    img[4 * i + 3] = 255;
  }

  return img;
}

static GLuint create_gray_texture() {
  static constexpr std::array<uint8_t, 16> data{
      {35, 35, 35, 255, 35, 35, 35, 255, 35, 35, 35, 255, 35, 35, 35, 255}};

  int width = 2;
  int height = 2;

  GLuint texid = 0;
  GLint last_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

  glGenTextures(1, &texid);
  glBindTexture(GL_TEXTURE_2D, texid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, &data[0]);

  glBindTexture(GL_TEXTURE_2D, GLuint(last_texture));

  return texid;
}

static void show_tensor_value(
    // Window position
    const ImVec2 window_pos,
    // offset of Tensor Image widget from the window upper left location
    // This offset includes scroll factor.
    const ImVec2 tensor_image_widget_offset, const float alpha,
    const float step, const nnview::Tensor &tensor) {
  (void)step;
  (void)tensor;

  const float left_margin = 6.0f;
  const float top_margin = 6.0f;

  const float cell_left_margin = std::max(0.0f, step / 2.0f - 24.0f);
  const float cell_top_margin = std::max(0.0f, step / 2.0f - 10.0f);

  const size_t height = size_t(tensor.shape[0]);
  const size_t width = size_t(tensor.shape[1]);

  ImVec2 window_size = ImGui::GetWindowSize();

  size_t num_rect_draws = 0;

  // Draw values for only visible area
  for (size_t y = 0; y < height; y++) {
    // Assume offset of image item from window corner(upper-left) is rather
    // small(e.g. < 50 pixels)
    // TODO(LTE): Compute bound outside of for loop.
    if (((step + 1) * float(y)) < -tensor_image_widget_offset.y) {
      continue;
    }
    if (((step * float(y)) > (-tensor_image_widget_offset.y + window_size.y))) {
      continue;
    }

    for (size_t x = 0; x < width; x++) {
      // TODO(LTE): Compute bound outside of for loop.
      if ((step + 1) * float(x) < -tensor_image_widget_offset.x) {
        continue;
      }

      if ((step * float(x)) > (-tensor_image_widget_offset.x + window_size.x)) {
        continue;
      }

      const float value = tensor.data[y * width + x];

      char buf[64];
      snprintf(buf, sizeof(buf), "%4.3f", double(value));

      ImVec2 bmin = ImVec2(window_pos.x + tensor_image_widget_offset.x +
                               step * float(x) + left_margin + cell_left_margin,
                           window_pos.y + tensor_image_widget_offset.y +
                               step * float(y) + top_margin + cell_top_margin);

      // Prevent too many AddRectFilled call for safety.
      // ImGui's default uses 16bit indices, so drawing too many rects will
      // cause the assertion failure inside imgui.
      // 1024 = heuristic value.
      if (num_rect_draws < 1024) {
        ImVec2 text_size = ImGui::CalcTextSize(buf);

        ImVec2 fill_bmin = ImVec2(bmin.x - 4, bmin.y - 4);
        ImVec2 fill_bmax =
            ImVec2(bmin.x + text_size.x + 4, bmin.y + text_size.y + 4);

        // Draw quad for background color
        ImGui::GetWindowDrawList()->AddRectFilled(
            fill_bmin, fill_bmax,
            ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.4f * alpha)),
            /* rounding */ 4.0f);

        // HACK
        // num_rect_draws++;
      }

      ImGui::SetCursorScreenPos(bmin);

      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, alpha), "%s", buf);
    }
  }
}

static GLuint gen_gl_texture(const nnview::Tensor &tensor) {
  GLuint texid = 0;
  glGenTextures(1, &texid);

  glBindTexture(GL_TEXTURE_2D, texid);

  std::vector<uint8_t> img = tensor_to_color(tensor);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tensor.shape[1], tensor.shape[0],
               /* border */ 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());

  // No bilinear filtering.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);

  return texid;
}

static int GetNextId() {
  static int s_NextId = 1;
  return s_NextId++;
}

static ed::NodeId GetNextNodeId() {
  // It looks ed::NodeId type is a integer of pointer value
  return static_cast<uintptr_t>(GetNextId());
}

static ed::LinkId GetNextLinkId() { return ed::LinkId(uint32_t(GetNextId())); }

static ImColor GetIconColor(PinType type) {
  switch (type) {
    case PinType::Flow:
      return ImColor(255, 255, 255);
    case PinType::Bool:
      return ImColor(220, 48, 48);
    case PinType::Int:
      return ImColor(68, 201, 156);
    case PinType::Float:
      return ImColor(147, 226, 74);
    case PinType::String:
      return ImColor(124, 21, 153);
    case PinType::Object:
      return ImColor(51, 150, 215);
    case PinType::Function:
      return ImColor(218, 0, 183);
    case PinType::Delegate:
      return ImColor(255, 48, 48);
  }
};

static void DrawPinIcon(const Pin &pin, bool connected, int alpha) {
  const int PinIconSize = 24;

  IconType iconType;
  ImColor color = GetIconColor(pin.Type);
  color.Value.w = float(alpha) / 255.0f;
  switch (pin.Type) {
    case PinType::Flow:
      iconType = IconType::Flow;
      break;
    case PinType::Bool:
      iconType = IconType::Circle;
      break;
    case PinType::Int:
      iconType = IconType::Circle;
      break;
    case PinType::Float:
      iconType = IconType::Circle;
      break;
    case PinType::String:
      iconType = IconType::Circle;
      break;
    case PinType::Object:
      iconType = IconType::Circle;
      break;
    case PinType::Function:
      iconType = IconType::Circle;
      break;
    case PinType::Delegate:
      iconType = IconType::Square;
      break;
  }

  ax::Widgets::Icon(ImVec2(PinIconSize, PinIconSize), iconType, connected,
                    color, ImColor(32, 32, 32, alpha));
};

// static inline ImRect ImGui_GetItemRect() {
//  return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
//}

void GUIContext::draw_imnodes() {
  ImGui::Begin("Graph");

  ed::SetCurrentEditor(_editor_context);

  ed::Begin("Graph");

  // const float padding = 6.0f;

  util::BlueprintNodeBuilder builder(
      ImTextureID(intptr_t(_background_texture_id)),
      /* tex width */ 2, /* tex height */ 2);

  for (size_t i = 0; i < _imnodes.size(); i++) {
    const ImNode &node = _imnodes[i];

    builder.Begin(node.id);
    builder.Header(node.color);

    ImGui::Spring(0);
    ImGui::TextUnformatted(node.name.c_str());
    ImGui::Spring(1);
    ImGui::Dummy(ImVec2(0, 28));

    builder.EndHeader();

    // ed::BeginNode(node.id);
    // ImGui::PushID(node.id.AsPointer());
    // ImGui::BeginVertical(node.id.AsPointer());
    // ImGui::BeginHorizontal("inputs");
    // ImGui::Spring(0, padding * 2);

    // ImGui::Text("%s", node.name.c_str());

    if (!node.inputs.empty()) {
      for (const auto &in_pin : node.inputs) {
        auto alpha = ImGui::GetStyle().Alpha;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        builder.Input(in_pin.ID);

        if (!in_pin.Name.empty()) {
          ImGui::Spring(0);
          ImGui::TextUnformatted(in_pin.Name.c_str());
        }

        ImGui::Spring(0);
        DrawPinIcon(in_pin, /* linked*/ false, int(alpha * 255.0f));
        ImGui::PopStyleVar();
        builder.EndInput();
      }
    }

    if (!node.outputs.empty()) {
      for (const auto &out_pin : node.outputs) {
        // ImGui::Dummy(ImVec2(0, padding));
        // ImGui::Spring(1, 0);
        // ImGui::Text("bora");

        // ImRect inputs_rect = ImGui_GetItemRect();

        auto alpha = ImGui::GetStyle().Alpha;

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        builder.Output(out_pin.ID);

        if (!out_pin.Name.empty()) {
          ImGui::Spring(0);
          ImGui::TextUnformatted(out_pin.Name.c_str());
        }

        ImGui::Spring(0);
        DrawPinIcon(out_pin, /* linked*/ false, int(alpha * 255.0f));

        ImGui::PopStyleVar();

        builder.EndOutput();

        // ed::BeginPin(pin.ID, ed::PinKind::Input);
        // ed::PinPivotRect(inputs_rect.GetTL(), inputs_rect.GetBR());
        // ed::PinRect(inputs_rect.GetTL(), inputs_rect.GetBR());
        // ImGui::Text("bora");
        // ed::EndPin();
        // ed::PopStyleVar(3);
      }
    }

    builder.End();

    // draw link
    for (auto &link : _links) {
      ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);
    }

    // ImGui::Spring(1);
    // ImGui::EndHorizontal();

    // ImGui::BeginHorizontal("content_frame");
    // ImGui::Text("doradora");
    // ImGui::EndHorizontal();

    // ImGui::EndVertical();

    // ImGui::Spring(0, padding * 2);
    // ImGui::PopID();
    // ed::EndNode();
  }

  // Update _active_tensor_idx if selected node is a tensor node
  {
    std::vector<ed::NodeId> selectedNodes;
    selectedNodes.resize(size_t(ed::GetSelectedObjectCount()));

    int nodeCount = ed::GetSelectedNodes(
        selectedNodes.data(), static_cast<int>(selectedNodes.size()));

    // Show single node info
    if (nodeCount == 1) {
      int selected_node_id = int(intptr_t(selectedNodes[0].AsPointer()));
      if (_node_id_to_imnode_idx_map.find(selected_node_id) !=
          _node_id_to_imnode_idx_map.end()) {
        int imnode_idx = _node_id_to_imnode_idx_map[selected_node_id];

        // std::cout << "imnode_idx = " << std::to_string(imnode_idx) << "\n";

        if ((imnode_idx >= 0) && (imnode_idx < int(_imnodes.size()))) {
          int tensor_idx = _imnodes[size_t(imnode_idx)].tensor_id;

          // std::cout << "selected tensor idx = " << std::to_string(tensor_idx)
          // << "\n";
          if (tensor_idx != -1) {
            _active_tensor_idx = tensor_idx;
          }
        }
      }
    }
  }

#if 0
    if (ImGui::Button("Flow")) {
      ed::Flow(/*link_id */ 1);
    }
#endif

  ed::End();
  ImGui::End();
}

void GUIContext::init() {
  if (_editor_context != nullptr) {
    // ???
    std::cerr << "EditorContext is already initialized or filled with invalid "
                 "value.\n";
    exit(-1);
  }

  _editor_context = ed::CreateEditor();

  std::cout << "num tensors" << _graph.tensors.size() << "\n";
  for (size_t i = 0; i < _graph.tensors.size(); i++) {
    std::cout << "shape size " << _graph.tensors[i].shape.size() << "\n";

    // std::cout << "tensor "  << _graph.tensors[i].shape[0] << ", " <<
    // _graph.tensors[i].shape[1] << std::endl;
    GLuint texid = gen_gl_texture(_graph.tensors[i]);

    _tensor_texture_ids.push_back(texid);
  }

  // Create whilte BG texture.
  _background_texture_id = create_gray_texture();
}

void GUIContext::init_imnode_graph() {
  ed::SetCurrentEditor(_editor_context);

  const float node_size = 128.0f;
  const float node_padding = 160.0f;
  const float layer_stride = 2 * node_size + node_padding;
  const float node_rect_slot_size_y = 32.0f;
  const float tensor_x_offset = node_size + 64.0f;

  _imnodes.clear();
  _links.clear();

  std::map<int, int> tensor_id_imnode_map;  // <id, imnode index>

  // Create node for layers
  for (size_t i = 0; i < _graph.nodes.size(); i++) {
    const nnview::Node &node = _graph.nodes[i];
    std::cout << "=== node[" << std::to_string(i) << "] " << node.name << "\n";
    std::cout << "  inputs = " << std::to_string(node.inputs.size()) << "\n";
    std::cout << "  outputs = " << std::to_string(node.outputs.size()) << "\n";

    {
      ed::NodeId node_id = GetNextNodeId();
      ImNode imnode(node_id, node.name);
      const float node_rect_height =
          float(node.outputs.size()) * node_rect_slot_size_y;
      imnode.size = ImVec2(node_size, node_rect_height);

      // Create pin id
      for (size_t p = 0; p < node.inputs.size(); p++) {
        const Slot &slot = node.inputs[p];
        std::string name = slot.slot_name;

        Pin pin(uint32_t(GetNextId()), name, PinType::Flow);

        imnode.inputs.emplace_back(pin);
      }

      for (size_t p = 0; p < node.outputs.size(); p++) {
        const Slot &slot = node.outputs[p];
        std::string name = slot.slot_name;

        Pin pin(uint32_t(GetNextId()), name, PinType::Flow);

        imnode.outputs.emplace_back(pin);
      }

      float offset_x = layer_stride * float(node.depth);
      std::cout << "depth = " << node.depth << "\n";
      std::cout << "id = " << uintptr_t(imnode.id) << "\n";
      ed::SetNodePosition(imnode.id, ImVec2(offset_x, 64.0f));

      _imnodes.emplace_back(imnode);
    }

    // Use index since `_imnodes` will be resized later(pointer or reference
    // value differs due to std::vector resizing)
    const size_t imnode_idx = _imnodes.size() - 1;

    assert(_imnodes[imnode_idx].inputs.size() == node.inputs.size());
    assert(_imnodes[imnode_idx].outputs.size() == node.outputs.size());

    // Create node for tensor connected to this node
    for (size_t t = 0; t < node.inputs.size(); t++) {
      const Slot &slot = node.inputs[t];
      std::cout << "Node " << node.name << ".inputs[" << std::to_string(t)
                << "] tensor id = " << slot.id << "\n";

      assert(slot.id >= 0);
      assert(slot.id < int(_graph.tensors.size()));

      if (tensor_id_imnode_map.find(slot.id) != tensor_id_imnode_map.end()) {
        ImNode &tensor_imnode = _imnodes[size_t(tensor_id_imnode_map[slot.id])];

        std::cout << "outputs.size = " << tensor_imnode.outputs.size() << "\n";

        if (tensor_imnode.outputs.size() == 0) {
          // Create output pin
          Pin out_pin(uint32_t(GetNextId()), /* empty name */ "",
                      PinType::Flow);
          tensor_imnode.outputs.push_back(out_pin);
        }

        Link link(GetNextLinkId(), tensor_imnode.outputs[0].ID,
                  _imnodes[imnode_idx].inputs[t].ID);

        _links.emplace_back(link);

        continue;
      }

      const nnview::Tensor &tensor = _graph.tensors[size_t(slot.id)];

      ed::NodeId imnode_id = GetNextNodeId();
      ImNode tensor_imnode(imnode_id, tensor.name);
      tensor_imnode.tensor_id = slot.id;

      const float node_rect_width = float(tensor.shape[1]);
      const float node_rect_height = float(tensor.shape[0]);

      tensor_imnode.color = ImColor(32, 255, 32);
      tensor_imnode.size = ImVec2(node_rect_width, node_rect_height);

      Pin out_pin(uint32_t(GetNextId()), /* empty name */ "", PinType::Flow);

      tensor_imnode.outputs.emplace_back(out_pin);

      // FIXME(LTE): Use the depth of previous op
      float offset_x = layer_stride * float(node.depth - 2) + tensor_x_offset;
      float offset_y = 128.0f * float(t);
      ed::SetNodePosition(tensor_imnode.id, ImVec2(offset_x, 64.0f + offset_y));

      tensor_id_imnode_map[slot.id] = int(_imnodes.size());

      Link link(GetNextLinkId(), out_pin.ID, _imnodes[imnode_idx].inputs[t].ID);

      std::cout << "link (" << intptr_t(&out_pin.ID) << ") -> ("
                << intptr_t(&_imnodes[imnode_idx].inputs[t].ID) << std::endl;
      _links.push_back(link);

      _node_id_to_imnode_idx_map[int(intptr_t(imnode_id.AsPointer()))] =
          int(_imnodes.size());

      _imnodes.push_back(tensor_imnode);
    }

    for (size_t t = 0; t < node.outputs.size(); t++) {
      const Slot &slot = node.outputs[t];
      std::cout << "output[" << t << "] tensor id = " << slot.id << "\n";

      assert(slot.id >= 0);
      assert(slot.id < int(_graph.tensors.size()));

      if (tensor_id_imnode_map.find(slot.id) != tensor_id_imnode_map.end()) {
        size_t tensor_imnode_idx = size_t(tensor_id_imnode_map[slot.id]);
        ImNode &tensor_imnode = _imnodes[tensor_imnode_idx];

        if (tensor_imnode.outputs.size() == 0) {
          // ImNode for Tensor is already registered but no input pin registered
          Pin in_pin(uint32_t(GetNextId()), /* empty name */ "", PinType::Flow);

          tensor_imnode.inputs.emplace_back(in_pin);
        }

        continue;
      }

      const nnview::Tensor &tensor = _graph.tensors[size_t(slot.id)];

      ed::NodeId imnode_id = GetNextNodeId();
      ImNode tensor_imnode(imnode_id, tensor.name);
      tensor_imnode.tensor_id = slot.id;

      const float node_rect_width = float(tensor.shape[1]);
      const float node_rect_height = float(tensor.shape[0]);

      tensor_imnode.color = ImColor(32, 32, 255);
      tensor_imnode.size = ImVec2(node_rect_width, node_rect_height);

      Pin in_pin(uint32_t(GetNextId()), /* empty name */ "", PinType::Flow);
      // Pin out_pin(uint32_t(GetNextId()), /* empty name */ "", PinType::Flow);

      tensor_imnode.inputs.emplace_back(in_pin);
      // tensor_imnode.outputs.emplace_back(out_pin);

      Link link(GetNextLinkId(), _imnodes[imnode_idx].outputs[t].ID, in_pin.ID);

      std::cout << "link " << node.name << " ("
                << intptr_t(&_imnodes[imnode_idx].outputs[t].ID) << ") -> ("
                << intptr_t(&in_pin.ID) << std::endl;
      _links.push_back(link);

      float offset_x = layer_stride * float(node.depth) + tensor_x_offset;
      float offset_y = 128.0f * float(t);
      ed::SetNodePosition(tensor_imnode.id, ImVec2(offset_x, 64.0f + offset_y));

      tensor_id_imnode_map[slot.id] = int(_imnodes.size());

      _node_id_to_imnode_idx_map[int(intptr_t(imnode_id.AsPointer()))] =
          int(_imnodes.size());

      _imnodes.push_back(tensor_imnode);
    }
  }

  ed::NavigateToContent();
}

void GUIContext::draw_tensor() {
  static float scale = 4.0f;  // Set 4x for better initial visual

  ImGui::Begin("Tensor", /* p_open */ nullptr,
               ImGuiWindowFlags_HorizontalScrollbar);

  {
    std::string name = (_active_tensor_idx > -1)
                           ? _graph.tensors[size_t(_active_tensor_idx)].name
                           : "no selection";
    ImGui::Text("Tensor : %s", name.c_str());

    ImGui::SliderFloat("scale", &scale, 0.0f, 100.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 win_pos = ImGui::GetWindowPos();

    ImGui::Text("win pos %f, %f", double(win_pos.x), double(win_pos.y));
    ImGui::Text("cur pos %f, %f", double(pos.x), double(pos.y));

    ImGui::End();
  }

  // Child window looks not working well.
  // Create dedicated window for Tensor image display

  if (_active_tensor_idx == -1) {
    return;
  }

  // std::cout << "active_tensor_idx " << std::to_string(_active_tensor_idx) <<
  // ", tensor_texids = " << std::to_string(_tensor_texture_ids.size()) << "\n";

  if (size_t(_active_tensor_idx) >= _tensor_texture_ids.size()) {
    // ???
    return;
  }

  GLuint texid = _tensor_texture_ids[size_t(_active_tensor_idx)];
  const Tensor &tensor = _graph.tensors[size_t(_active_tensor_idx)];

  // Create child so that scroll bar only effective to the image region.
  ImGui::Begin("Tensor Image", /* p_open */ nullptr,
               ImGuiWindowFlags_HorizontalScrollbar);
  {
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 image_pos = ImGui::GetCursorScreenPos();

    ImVec2 image_local_offset;
    image_local_offset.x = image_pos.x - win_pos.x;
    image_local_offset.y = image_pos.y - win_pos.y;

    ImGui::Image(ImTextureID(intptr_t(texid)),
                 ImVec2(scale * float(tensor.shape[1]), scale * float(tensor.shape[0])));

    if (scale > 40.0f) {
      // 40.0 ~ 64.0 : alpha 0 -> 1
      // 64.0 > : 1
      const float alpha =
          (scale > 64.0f) ? 1.0f : (scale - 40.0f) / (64.0f - 40.0f);
      show_tensor_value(win_pos, image_local_offset, alpha, scale, tensor);
    }

    ImGui::End();
  }
}

void GUIContext::finalize() {
  if (_editor_context) {
    ed::DestroyEditor(_editor_context);
  }
}

}  // namespace nnview
