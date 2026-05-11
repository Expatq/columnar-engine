#pragma once

#include <exec/interface/operator.h>

#include <memory>
#include <string>

namespace Columnar::Exec {

std::unique_ptr<IOperator> BuildQ1(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ2(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ3(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ4(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ5(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ6(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ7(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ8(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ9(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ10(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ11(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ12(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ13(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ14(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ15(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ16(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ17(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ18(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ19(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ20(const std::string& iyxPath);

}  // namespace Columnar::Exec
