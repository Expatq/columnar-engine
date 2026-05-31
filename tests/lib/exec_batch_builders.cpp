#include "exec_batch_builders.h"

namespace Columnar::Test {

Exec::ExecBatch MakeBatch(std::shared_ptr<RowGroup> rowGroup) {
    Exec::ExecBatch batch;
    batch.rowCount = rowGroup->GetRowCount();
    batch.rowGroup = std::move(rowGroup);
    return batch;
}

Exec::ExecBatch MakeBatchWithSelection(std::shared_ptr<RowGroup> rowGroup,
                                       std::initializer_list<Exec::RowId> selectedRows) {
    Exec::ExecBatch batch;
    batch.rowCount = rowGroup->GetRowCount();
    batch.rowGroup = std::move(rowGroup);
    batch.has_selection = true;
    batch.selection.MutableRows().assign(selectedRows.begin(), selectedRows.end());
    return batch;
}

}  // namespace Columnar::Test
