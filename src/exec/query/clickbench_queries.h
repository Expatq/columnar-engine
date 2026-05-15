#pragma once

#include <exec/interface/operator.h>

#include <memory>
#include <string>

namespace Columnar::Exec {

static constexpr size_t kClickBenchQueryCount = 43;

std::unique_ptr<IOperator> BuildQ0(const std::string& iyxPath);
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
std::unique_ptr<IOperator> BuildQ21(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ22(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ23(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ24(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ25(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ26(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ27(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ28(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ29(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ30(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ31(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ32(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ33(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ34(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ35(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ36(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ37(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ38(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ39(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ40(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ41(const std::string& iyxPath);
std::unique_ptr<IOperator> BuildQ42(const std::string& iyxPath);

std::unique_ptr<IOperator> BuildQuery(const std::string& iyxPath, size_t queryId);

}  // namespace Columnar::Exec
