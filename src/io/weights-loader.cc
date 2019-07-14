#include "io/weights-loader.hh"

#include <fstream>
#include <iostream>
#include <regex>

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

  std::regex regex{R"([\s,]+)"}; // split on space and comma
  std::sregex_token_iterator it{shape_line.begin(), shape_line.end(), regex, -1};
  std::vector<std::string> words{it, {}};

  std::vector<int> shape;

  std::cout << "dim : " << words.size() << std::endl;

  size_t n = 1;
  for (auto &m : words) {
    int d = std::stoi(m);
    shape.push_back(d);

    n *= size_t(d);
  }

  if (words.size() == 1) {
    // force create 2D tensor
    shape.push_back(1);
  }

  std::cout << n << "\n";

  tensor->data.resize(n);

  tensor->shape = shape;
  tensor->name = filename;

  ifs.read(reinterpret_cast<char *>(tensor->data.data()), int64_t(n) * datasize);

  if (!ifs) {
    std::cerr << "Failed to read [" << std::to_string(int64_t(n) * datasize) << "] bytes. only [" << ifs.gcount() << "] could be read.\n";
    return false;
  }

  return true;
}


} // namespace nnview
