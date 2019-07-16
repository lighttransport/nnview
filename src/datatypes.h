#ifndef NNVIEW_DATATYPES_H_
#define NNVIEW_DATATYPES_H_

#include <vector>
#include <string>

namespace nnview {

typedef std::pair<std::string, int> StrId;

enum LayerType
{
  LAYER_INPUT,
  LAYER_OUTPUT,
  LAYER_LINEAR_FUNCTION,
  LAYER_RELU,
};

class Node
{
 public:
  LayerType type;
  int id = 0; // Unique node id
  int depth = 0; // Depth from the input node. Use this value for initial node layout.
  std::string name;

  std::vector<StrId> inputs;
  std::vector<StrId> outputs;
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
  std::vector<StrId> inputs;
  std::vector<StrId> outputs;

  std::vector<Node> nodes;
  std::vector<Tensor> tensors;
};



} // namespace nnview

#endif // NNVIEW_DATATYPES_H_
