//===----------------------------------------------------------------------===//
//                         DuckDB
//
// json_common.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "yyjson.hpp"

namespace duckdb {

struct JSONCommon {
public:
	//! Convert JSON query string to JSON path query
	static inline bool ConvertToPath(const string_t &query, string &result, idx_t &len) {
		len = query.GetSize();
		if (len == 0) {
			return false;
		}
		const char *ptr = query.GetDataUnsafe();
		if (*ptr == '/') {
			// Already a path string
			result = query.GetString();
		} else if (*ptr == '$') {
			// Dollar/dot/brackets syntax
			result = StringUtil::Replace(string(ptr + 1, len - 1), ".", "/");
			result = StringUtil::Replace(result, "]", "");
			result = StringUtil::Replace(result, "[", "/");
			len = result.length();
		} else {
			// Plain tag/array index, prepend slash
			len++;
			result = "/" + query.GetString();
		}
		return true;
	}

	//! Get root of JSON document
	static inline yyjson_val *GetRoot(const string_t &input) {
		return yyjson_doc_get_root(yyjson_read(input.GetDataUnsafe(), input.GetSize(), YYJSON_READ_NOFLAG));
	}

	//! Get JSON value using JSON path query
	static inline yyjson_val *GetPointer(const string_t &input, const char *ptr, const idx_t &len) {
		return unsafe_yyjson_get_pointer(GetRoot(input), ptr, len);
	}
};

struct JSONFunctionData : public FunctionData {
public:
	explicit JSONFunctionData(bool constant, string path_p, idx_t len)
	    : constant(constant), path(move(path_p)), len(len) {
	}

public:
	const bool constant;
	const string path;
	const size_t len;

public:
	unique_ptr<FunctionData> Copy() override {
		return make_unique<JSONFunctionData>(constant, path, len);
	}

	static unique_ptr<FunctionData> Bind(ClientContext &context, ScalarFunction &bound_function,
	                                     vector<unique_ptr<Expression>> &arguments) {
		D_ASSERT(bound_function.arguments.size() == 2);
		bool constant = false;
		string path = "";
		idx_t len = 0;
		if (arguments[1]->return_type.id() != LogicalTypeId::SQLNULL && arguments[1]->IsFoldable()) {
			constant = true;
			auto query = ExpressionExecutor::EvaluateScalar(*arguments[1]).GetValueUnsafe<string_t>();
			JSONCommon::ConvertToPath(query, path, len);
		}
		return make_unique<JSONFunctionData>(constant, path, len);
	}
};

} // namespace duckdb
