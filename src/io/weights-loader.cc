#include "io/weights-loader.hh"

#include <cassert>
#include <fstream>
#include <iostream>

namespace nnview {

bool load_weights(const std::string &filename, Tensor *tensor) {
  std::ifstream ifs(filename, std::ios::in | std::ios::binary);
  if (!ifs) {
    std::cerr << "Failed to open file : " << filename << std::endl;
    return false;
  }

  std::string datasize_line;
  std::getline(ifs, datasize_line);

  int datasize = std::stoi(datasize_line);
  if (datasize != 4) {
    std::cerr << "Data size must be 4, but got " << datasize_line << std::endl;
    return false;
  }

  std::cout << "datasize " << datasize << "\n";

  std::string shape_line;
  std::getline(ifs, shape_line);

  // Up to 5D tensor
  int d[5];
  int n = sscanf(shape_line.c_str(), "%d,%d,%d,%d,%d", &d[0], &d[1], &d[2],
                 &d[3], &d[4]);

  size_t num_items = 1;
  std::vector<int> shape;
  for (int i = 0; i < n; i++) {
    assert(d[i] > 0);
    shape.push_back(d[i]);
    num_items *= size_t(d[i]);
  }

  std::cout << "dim : " << shape.size() << std::endl;
  for (size_t i = 0; i < shape.size(); i++) {
    std::cout << "  [" << i << "] = " << shape[i] << std::endl;
  }

  if (shape.size() == 0) {
    std::cerr << "Failed to parse shape information: " << shape_line
              << std::endl;
    return false;
  }

  if (shape.size() == 1) {
    // force create 2D tensor
    shape.push_back(1);
  }

  std::cout << "num_items: " << num_items << "\n";

  tensor->data.resize(num_items);

  tensor->shape = shape;
  tensor->name = filename;

  ifs.read(reinterpret_cast<char *>(tensor->data.data()),
           int64_t(num_items) * datasize);

  if (!ifs) {
    std::cerr << "Failed to read ["
              << std::to_string(int64_t(num_items) * datasize)
              << "] bytes. only [" << ifs.gcount() << "] could be read.\n";
    return false;
  }

  return true;
}

}  // namespace nnview
