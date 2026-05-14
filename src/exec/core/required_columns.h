#pragma once

#include <absl/container/inlined_vector.h>

#include <string>
#include <utility>
#include <vector>

namespace Columnar::Exec {

class RequiredColumns {
public:
    enum class Mode {
        All,
        Names,
        None,
    };

    static RequiredColumns All() {
        return RequiredColumns(Mode::All, {});
    }

    static RequiredColumns None() {
        return RequiredColumns(Mode::None, {});
    }

    static RequiredColumns Only(std::vector<std::string> names) {
        return RequiredColumns(Mode::Names, std::move(names));
    }

    Mode GetMode() const {
        return mode_;
    }

    const absl::InlinedVector<std::string, 8>& Names() const {
        return names_;
    }

private:
    RequiredColumns(Mode mode, std::vector<std::string> names)
        : mode_(mode),
          names_(std::make_move_iterator(names.begin()), std::make_move_iterator(names.end())) {
    }

    Mode mode_;
    absl::InlinedVector<std::string, 8> names_;
};

}  // namespace Columnar::Exec
