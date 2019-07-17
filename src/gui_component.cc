#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "glad/glad.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "colormap.hh"
#include "gui_component.hh"

#include <cstdint>
#include <iostream>
#include <limits>
#include <algorithm>

namespace nnview {

inline uint8_t ftoc(const float x) {
  int i = int(x * 255.0f);
  i = std::min(255, std::max(0, i));
  return uint8_t(i);
}

static std::vector<uint8_t> tensor_to_color(const nnview::Tensor& tensor) {
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

static void show_tensor_value(
    // Window position
    const ImVec2 window_pos,
    // offset of Tensor Image widget from the window upper left location
    // This offset includes scroll factor.
    const ImVec2 tensor_image_widget_offset, const float alpha,
    const float step, const nnview::Tensor& tensor) {
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
    if (((step + 1) * y) < -tensor_image_widget_offset.y) {
      continue;
    }
    if (((step * y) > (-tensor_image_widget_offset.y + window_size.y))) {
      continue;
    }

    for (size_t x = 0; x < width; x++) {
      // TODO(LTE): Compute bound outside of for loop.
      if ((step + 1) * x < -tensor_image_widget_offset.x) {
        continue;
      }

      if ((step * x) > (-tensor_image_widget_offset.x + window_size.x)) {
        continue;
      }

      const float value = tensor.data[y * width + x];

      char buf[64];
      snprintf(buf, sizeof(buf), "%4.3f", double(value));

      ImVec2 bmin = ImVec2(window_pos.x + tensor_image_widget_offset.x +
                               step * x + left_margin + cell_left_margin,
                           window_pos.y + tensor_image_widget_offset.y +
                               step * y + top_margin + cell_top_margin);

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

static GLuint gen_gl_texture(const nnview::Tensor& tensor) {
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

void GUIContext::draw_imnodes() {
  ed::SetCurrentEditor(_editor_context);

  ed::Begin("Model");

  for (size_t i = 0; i < _imnodes.size(); i++) {
    const ImNode& node = _imnodes[i];

    ed::BeginNode(node.id);
    ImGui::Text("%s", node.name.c_str());
    // ed::BeginPin(uniqueId++, ed::PinKind::Input);
    // ImGui::Text("-> In");
    // ed::EndPin();
    // ImGui::SameLine();
    // start_pin_id = uniqueId++;
    // ed::BeginPin(start_pin_id, ed::PinKind::Output);
    // ImGui::Text("Out ->");
    // ed::EndPin();
    ed::EndNode();
  }

#if 0
    // Test link
    if (!ed::Link(/*link_id */ 1, start_pin_id, end_pin_id)) {
      std::cerr << "Link fail\n";
    }

    if (ImGui::Button("Flow")) {
      ed::Flow(/*link_id */ 1);
    }
#endif

  ed::End();
}

void GUIContext::init() {
  if (_editor_context != nullptr) {
    // ???
    std::cerr << "EditorContext is already initialized or filled with invalid value.\n";
    exit(-1);
  }

  _editor_context = ed::CreateEditor();

  std::cout << "num tensors"  << _graph.tensors.size() << "\n";
  for (size_t i = 0; i < _graph.tensors.size(); i++) {
    std::cout << "shape size "  << _graph.tensors[i].shape.size() << "\n";
  }

  //std::cout << "tensor "  << _graph.tensors[i].shape[0] << ", " << _graph.tensors[i].shape[1] << std::endl;
  GLuint texid = gen_gl_texture(_graph.tensors[0]);

  _tensor_texture_ids.push_back(texid);
}

void GUIContext::init_imnode_graph() {
  ed::SetCurrentEditor(_editor_context);

  float node_size = 200.0f;

  _imnodes.clear();

  for (size_t i = 0; i < _graph.nodes.size(); i++) {
    const nnview::Node& node = _graph.nodes[i];

    ImNode imnode(GetNextNodeId(), node.name);
    imnode.name = node.name;

    float offset_x = node_size * float(node.depth);
    std::cout << "depth = " << node.depth << "\n";
    std::cout << "id = " << uintptr_t(imnode.id) << "\n";
    ed::SetNodePosition(imnode.id, ImVec2(offset_x, 64.0f));

    _imnodes.push_back(imnode);
  }

  ed::NavigateToContent();
}

void GUIContext::draw_tensor() {
  static float scale = 4.0f;  // Set 4x for better initial visual

  ImGui::Begin("Tensor", /* p_open */ nullptr,
               ImGuiWindowFlags_HorizontalScrollbar);

  {
    std::string name = (_active_tensor_id > -1)
                           ? _graph.tensors[size_t(_active_tensor_id)].name
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

  if (_active_tensor_id == -1) {
    return;
  }

  if (size_t(_active_tensor_id) >= _tensor_texture_ids.size()) {
    // ???
    return;
  }

  GLuint texid = _tensor_texture_ids[size_t(_active_tensor_id)];
  const Tensor& tensor = _graph.tensors[size_t(_active_tensor_id)];

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
                 ImVec2(scale * tensor.shape[1], scale * tensor.shape[0]));

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
