#ifndef NNVIEW_DATATYPES_H_
#define NNVIEW_DATATYPES_H_

#include <vector>
#include <string>

namespace nnview {

struct Slot
{
  std::string name;       // name of tensor/weight
  std::string slot_name;  // Appear in connection name on GUI node
  int id; // tensor id

  Slot(const std::string &_name, const std::string &_slot_name, int _id) : name(_name), slot_name(_slot_name), id(_id) {}
};

enum LayerType
{
  LAYER_INPUT,
  LAYER_OUTPUT,
  LAYER_LINEAR_FUNCTION,
  LAYER_RELU,
  LAYER_TENSOR,
};

class Node
{
 public:
  LayerType type;
  int id = 0; // Unique node id
  int depth = 0; // Depth from the input node. Use this value for initial node layout.
  std::string name;

  std::vector<Slot> inputs;
  std::vector<Slot> outputs;
};

class Tensor
{
 public:
  Tensor() {}

  std::string name;
  std::string datatype = "float32"; // TODO(LTE): Support more data types.
  std::vector<int> shape;
  std::vector<float> data;
};

class Graph
{
 public:
  std::vector<Slot> inputs;
  std::vector<Slot> outputs;

  std::vector<Node> nodes;
  std::vector<Tensor> tensors;
};



} // namespace nnview

#endif // NNVIEW_DATATYPES_H_
