#pragma once

#include <exec/interface/operator.h>

#include <memory>
#include <string>

namespace Columnar::Exec {

std::unique_ptr<IOperator> BuildQ1(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ2(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ3(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ4(const std::string& iyxPath);

}  // namespace Columnar::Exec
