#include "io/graph-loader.hh"
#include "io/weights-loader.hh"

#include "json11.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace json11;

namespace nnview {

static std::string JoinPath(const std::string &dir,
                            const std::string &filename) {
  if (dir.empty()) {
    return filename;
  } else {
    char lastChar = *dir.rbegin();
    if (lastChar != '/') {
      return dir + std::string("/") + filename;
    } else {
      return dir + filename;
    }
  }
}

static std::string GetBaseDir(const std::string &filepath) {
  if (filepath.find_last_of("/\\") != std::string::npos)
    return filepath.substr(0, filepath.find_last_of("/\\"));
  return "";
}

static bool LoadWeights(
    const std::vector<std::pair<std::string, std::string>> &weights,
    const std::string base_dir, std::map<std::string, Tensor> *tensors) {
  // item = <name, filename>
  for (const auto &item : weights) {
    Tensor tensor;
    std::string filepath = JoinPath(base_dir, item.second);
    if (!load_weights(filepath, &tensor)) {
      std::cerr << "Failed to read weight/tensor : " << filepath << "\n";
      return false;
    }

    // Ensure uniqueness
    if (tensors->count(item.first)) {
      std::cerr << item.first << "(filename: " << item.second
                << ") is already exists.\n";
      return false;
    }

    std::cout << "loaded tensor/weight : " << item.first
              << ", len(shape) = " << tensor.shape.size() << "\n";
    (*tensors)[item.first] = tensor;
  }

  return true;
}

static bool ParseInputProperty(const Json &j, Node *node, Graph *graph) {
  (void)j;
  (void)node;
  (void)graph;
#if 0
  std::vector<int> shape;
  for (auto &d : j["shape"].array_items()) {
    if (d.is_number()) {
      shape.push_back(d.int_value());
    }
  }

  // Create empty Tensor for `input` layer.
  Tensor tensor;
  tensor.name = node->name;
  tensor.shape = shape;

  int id = int(graph->tensors.size());
  graph->tensors.push_back(tensor);

  assert(node->outputs.size() == 1);

  node->outputs[0].second = id;
#endif

  return true;
}

static bool ParseLinearFunctionProperty(
    const Json &j, Node *node,
    std::vector<std::pair<std::string, std::string>> *tensor_files) {
  if (j["source"].is_string()) {
    std::string name = j["source"].string_value();

    // id will be determinted later
    node->inputs.push_back(Slot(name, "input", -1));
  }

  if (j["kernel_weights_file"].is_string()) {
    std::string filepath = j["kernel_weights_file"].string_value();

    (*tensor_files).push_back({filepath, filepath});

    // id will be determinted later
    node->inputs.push_back(Slot(filepath, "W", -1));
  }

  if (j["bias_weights_file"].is_string()) {
    std::string filepath = j["bias_weights_file"].string_value();

    (*tensor_files).push_back({filepath, filepath});

    // id will be determinted later
    node->inputs.push_back(Slot(filepath, "b", -1));
  }

  return true;
}

static bool ParseReLUProperty(const Json &j, Node *node) {
  if (j["source"].is_string()) {
    std::string name = j["source"].string_value();

    // id will be determinted later
    node->inputs.push_back(Slot(name, "input", -1));
  }

  return true;
}

static int FindTensor(const std::string &name,
                      const std::vector<Tensor> &tensors) {
  for (size_t i = 0; i < tensors.size(); i++) {
    if (name.compare(tensors[i].name) == 0) {
      return int(i);
    }
  }

  return -1;  // not found
}

bool load_json_graph(const std::string &filename, Graph *graph) {
  if (graph == nullptr) {
    std::cerr << "`graph` is nullptr\n";
    return false;
  }

  std::ifstream ifs(filename, std::ios::in);
  if (!ifs) {
    std::cerr << "Failed to open graph file : " << filename << std::endl;
    return false;
  }

  std::stringstream ss;
  ss << ifs.rdbuf();
  ifs.close();

  std::string json_str(ss.str());

  std::string err;
  Json json = Json::parse(json_str, err);

  if (!err.empty()) {
    std::cerr << "JSON parse error. filename: " << filename << " err: " << err
              << std::endl;
    return false;
  }

  std::vector<std::string> inputs;
  for (auto input : json["inputs"].array_items()) {
    if (input.is_string()) {
      inputs.push_back(input.string_value());
    }
  }

  std::vector<std::string> outputs;
  for (auto output : json["outputs"].array_items()) {
    // Chainer-TRT's outpus is an array of array item.
    if (output.is_array()) {
      // Just take the first one.
      auto &item = output.array_items()[0];
      if (item.is_string()) {
        outputs.push_back(item.string_value());
      }
    }
  }

  std::map<std::string, int> node_name_to_id_map;

  std::vector<std::pair<std::string, std::string>>
      temp_tensors;  // <name, filename>

  graph->nodes.clear();
  // layers
  for (auto layer : json["layers"].array_items()) {
    // Exampe definition of layer.
    // See $nnview/models/mnist/model.json for details.
    //
    // {
    //   "type": "input",
    //   "name": "input",
    //   "output_names": [
    //     "input"
    //   ],
    //   "rank": -2,
    //   "shape": [
    //     784
    //   ]
    // },

    if (!layer.is_object()) {
      continue;
    }

    std::string type = layer["type"].string_value();
    std::string name = layer["name"].string_value();

    std::vector<std::string> output_names;

    // rank = layer depth
    int rank = layer["rank"].int_value();

    Node node;
    node.name = name;
    node.depth = rank;

    for (auto &output_name : layer["output_names"].array_items()) {
      if (output_name.is_string()) {
        node.outputs.push_back(Slot(output_name.string_value(), "output", -1));
      }
    }

    // TODO(LTE): Support multiple outputs.
    if (node.outputs.size() == 1) {
      if (layer["output_tensor"].is_string()) {
        std::string tensor_filename = layer["output_tensor"].string_value();
        temp_tensors.push_back({node.outputs[0].name, tensor_filename});
      }
    }

    if (type.compare("input") == 0) {
      bool ret = ParseInputProperty(layer, &node, graph);
      if (!ret) {
        std::cerr << "Failed to parse `input` layer.\n";
        return false;
      }

      // `input` layer has `input_tensor`.
      // We treat it as output tensor.
      assert(node.outputs.size() == 1);
      if (layer["input_tensor"].is_string()) {
        std::string tensor_filename = layer["input_tensor"].string_value();
        temp_tensors.push_back({node.outputs[0].name, tensor_filename});
      }

    } else if (type.compare("LinearFunction") == 0) {
      bool ret = ParseLinearFunctionProperty(layer, &node, &temp_tensors);
      if (!ret) {
        std::cerr << "Failed to parse `LinearFunction` layer.\n";
        return false;
      }
    } else if (type.compare("ReLU") == 0) {
      bool ret = ParseReLUProperty(layer, &node);
      if (!ret) {
        std::cerr << "Failed to parse `ReLU` layer.\n";
        return false;
      }
    } else {
      // Unknown
    }

    node.id = int(graph->nodes.size());
    graph->nodes.push_back(node);

    std::cout << "Node: " << name << ", id: " << node.id << "\n";
    std::cout << "  # of inputs: " << node.inputs.size() << "\n";
    std::cout << "  # of outputs: " << node.outputs.size() << "\n";

    node_name_to_id_map[name] = node.id;
  }

  for (size_t i = 0; i < temp_tensors.size(); i++) {
    std::cout << temp_tensors[i].first << " = " << temp_tensors[i].second
              << "\n";
  }

  // Batch load weights/tensors.
  {
    std::string base_dir = GetBaseDir(filename);

    std::map<std::string, Tensor> tensors;
    if (!LoadWeights(temp_tensors, base_dir, &tensors)) {
      return false;
    }

    for (auto &item : tensors) {
      // Rename
      item.second.name = item.first;
      graph->tensors.push_back(item.second);
      std::cout << "len(shape) = " << item.second.shape.size() << "\n";
    }
  }

  // Find id for input and output of the graph.
  {
    for (const auto &input : inputs) {
      int input_id = node_name_to_id_map[input];
      graph->inputs.push_back(Slot(input, "input", input_id));
      std::cout << "Input: " << input << ", id: " << input_id << "\n";
    }

    for (const auto &output : outputs) {
      int output_id = node_name_to_id_map[output];
      graph->inputs.push_back(Slot(output, "output", output_id));
      std::cout << "Output: " << output << ", id: " << output_id << "\n";
    }
  }

  // Establish the link of inputs and outpus for each layers.
  {
    for (size_t n = 0; n < graph->nodes.size(); n++) {
      Node &node = graph->nodes[n];

      for (size_t i = 0; i < node.inputs.size(); i++) {
        const std::string &name = node.inputs[i].name;

        int tensor_id = FindTensor(name, graph->tensors);
        if (tensor_id == -1) {
          std::cerr << "Input tensor \"" << name
                    << "\" not found in the graph.\n";
          return false;
        } else {
          std::cerr << "Input tensor \"" << name
                    << "\" has connection. tensor id " << tensor_id << "\n";
        }

        node.inputs[i].id = tensor_id;
      }

      for (size_t o = 0; o < node.outputs.size(); o++) {
        const std::string &name = node.outputs[o].name;

        int tensor_id = FindTensor(name, graph->tensors);
        if (tensor_id == -1) {
          std::cerr << "Output tensor \"" << name
                    << "\" not found in the graph.\n";
          return false;
        } else {
          std::cerr << "Output tensor \"" << name
                    << "\" has connection. tensor id " << tensor_id << "\n";
        }

        node.outputs[o].id = tensor_id;
      }
    }
  }

  return true;
}

}  // namespace nnview
