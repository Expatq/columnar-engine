#pragma once

#include <exec/core/selection_vector.h>
#include <core/row_group.h>

namespace Columnar::Exec {

struct ExecBatch {
    std::optional<RowGroup> rowGroup;
    size_t rowCount = 0;

    /*
    Need this because has_selection = true and selection.empty() == true ==> 0 active rows
    */
    SelectionVector selection;
    bool has_selection = false;

    void Reset() {
        rowGroup.reset();
        rowCount = 0;
        has_selection = false;
        selection.Clear();
    }

    size_t ActiveRowCount() const {
        return has_selection ? selection.Size() : rowCount;
    }

    bool Empty() const {
        return ActiveRowCount() == 0;
    }

};

}  // namespace Columnar::Exec
