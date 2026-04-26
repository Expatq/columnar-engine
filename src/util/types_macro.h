#pragma once

/**
 * @file types_macro.h
 * @brief Macroses for visitors codegen and functors
 * These macroses should help avoiding code duplication while declaring and defining visitors.
*/

namespace NColumnar::NTypes {

#define COLUMNAR_FOR_EACH_TYPE(F) \
    F(int16_t)                    \
    F(int32_t)                    \
    F(int64_t)                    \
    F(bool)                       \
    F(std::string)

// Declaration macroses

#define DECLARE_VISITOR_OP_CONST(TYPE, RETURN_TYPE) \
    RETURN_TYPE operator()(const std::vector<TYPE>& data) const;

#define DECLARE_VISITOR_OP_MUTUABLE(TYPE, RETURN_TYPE) \
    RETURN_TYPE operator()(std::vector<TYPE>& data) const;

#define DECLARE_CONST_VISITOR_FOR_ALL_TYPES(RETURN_TYPE) \
    DECLARE_VISITOR_OP_CONST(int16_t, RETURN_TYPE)       \
    DECLARE_VISITOR_OP_CONST(int32_t, RETURN_TYPE)       \
    DECLARE_VISITOR_OP_CONST(int64_t, RETURN_TYPE)       \
    DECLARE_VISITOR_OP_CONST(bool, RETURN_TYPE)          \
    DECLARE_VISITOR_OP_CONST(std::string, RETURN_TYPE)

#define DECLARE_MUTUABLE_VISITOR_FOR_ALL_TYPES(RETURN_TYPE) \
    DECLARE_VISITOR_OP_MUTUABLE(int16_t, RETURN_TYPE)       \
    DECLARE_VISITOR_OP_MUTUABLE(int32_t, RETURN_TYPE)       \
    DECLARE_VISITOR_OP_MUTUABLE(int64_t, RETURN_TYPE)       \
    DECLARE_VISITOR_OP_MUTUABLE(bool, RETURN_TYPE)          \
    DECLARE_VISITOR_OP_MUTUABLE(std::string, RETURN_TYPE)

// Impl macroses

#define IMPL_VISITOR_OP_CONST(VISITOR_NAME, TYPE, RETURN_TYPE, BODY)    \
    RETURN_TYPE VISITOR_NAME::operator()(const std::vector<TYPE>& data) \
        const {                                                         \
        BODY                                                            \
    }

#define IMPL_VISITOR_OP_MUTABLE(VISITOR_NAME, TYPE, RETURN_TYPE, BODY)    \
    RETURN_TYPE VISITOR_NAME::operator()(std::vector<TYPE>& data) const { \
        BODY                                                              \
    }

#define IMPL_CONST_VISITOR_FOR_ALL_TYPES(VISITOR_NAME, RETURN_TYPE, BODY) \
    IMPL_VISITOR_OP_CONST(VISITOR_NAME, int16_t, RETURN_TYPE, BODY)       \
    IMPL_VISITOR_OP_CONST(VISITOR_NAME, int32_t, RETURN_TYPE, BODY)       \
    IMPL_VISITOR_OP_CONST(VISITOR_NAME, int64_t, RETURN_TYPE, BODY)       \
    IMPL_VISITOR_OP_CONST(VISITOR_NAME, bool, RETURN_TYPE, BODY)          \
    IMPL_VISITOR_OP_CONST(VISITOR_NAME, std::string, RETURN_TYPE, BODY)

#define IMPL_MUTABLE_VISITOR_FOR_ALL_TYPES(VISITOR_NAME, RETURN_TYPE, BODY) \
    IMPL_VISITOR_OP_MUTABLE(VISITOR_NAME, int16_t, RETURN_TYPE, BODY)       \
    IMPL_VISITOR_OP_MUTABLE(VISITOR_NAME, int32_t, RETURN_TYPE, BODY)       \
    IMPL_VISITOR_OP_MUTABLE(VISITOR_NAME, int64_t, RETURN_TYPE, BODY)       \
    IMPL_VISITOR_OP_MUTABLE(VISITOR_NAME, bool, RETURN_TYPE, BODY)          \
    IMPL_VISITOR_OP_MUTABLE(VISITOR_NAME, std::string, RETURN_TYPE, BODY)

}  // namespace NColumnar::NTypes