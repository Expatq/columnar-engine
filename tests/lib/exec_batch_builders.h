#pragma once

#include <core/row_group.h>
#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>

#include <initializer_list>
#include <memory>

namespace Columnar::Test {

Exec::ExecBatch MakeBatch(std::shared_ptr<RowGroup> rowGroup);

Exec::ExecBatch MakeBatchWithSelection(std::shared_ptr<RowGroup> rowGroup,
                                       std::initializer_list<Exec::RowId> selectedRows);

}  // namespace Columnar::Test
