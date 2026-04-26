#pragma once

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

    const std::vector<std::string>& Names() const {
        return names_;
    }

private:
    RequiredColumns(Mode mode, std::vector<std::string> names)
        : mode_(mode),
          names_(std::move(names)) {
    }

    Mode mode_;
    std::vector<std::string> names_;
};

}  // namespace Columnar::Exec
