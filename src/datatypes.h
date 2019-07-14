#ifndef NNVIEW_DATATYPES_H_
#define NNVIEW_DATATYPES_H_

#include <vector>
#include <string>

namespace nnview {


// TODO(LTE): Support data type other than float32.
class Tensor
{
 public:
  Tensor() {}

  std::string name;

  std::vector<int> shape;
  std::vector<float> data;
};


} // namespace nnview

#endif // NNVIEW_DATATYPES_H_
