#pragma once

#include <cask/cask.h>
#include <string>
#include <vector>

namespace cask {
namespace jit {

/////////////////////////////////////////////////////////////////////////////////////////////////

class Compiler {
public:

  enum class KindID {
    CUBIN,
    PTX
  };

public:

  //
  // Data members
  //

  std::string              nvcc_path;
  std::vector<std::string> include_paths;
  std::vector<std::string> options;

public:

  Compiler(std::vector<int> const & compute_capabilities = {80});
  ~Compiler();

  std::ostream & compile(
    std::ostream &out,
    std::string const &dst_path, 
    std::string const &src_path,
    KindID target_kind = KindID::CUBIN);

  int execute(std::string const &cmd); 

};

/////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace jit
} // namespace cask

/////////////////////////////////////////////////////////////////////////////////////////////////
