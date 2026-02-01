#include <gtest/gtest.h>

#include <core/batch.h>
#include <core/schema.h>
#include <io/csv_reader.h>
#include <io/csv_writer.h>
#include <io/format_reader.h>
#include <io/format_writer.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include "core/row_group.h"
#include "core/types.h"
#include "parser/schema_parser.h"

namespace Columnar::Test {

constexpr const char* kTestInputDataCsv = "test_input.csv";
constexpr const char* kTestInputSchemaCsv = "test_schema_input.csv";
constexpr const char* kTestIyxFile = "test_data.iyx";
constexpr const char* kTestOutputDataCsv = "test_output.csv";
constexpr const char* kTestOutputSchemaCsv = "test_schema_output.csv";

namespace {

static constexpr uint64_t kMod = 1'000'000'007;

void WriteFile(const std::string& filename, const std::string& content) {
    std::ofstream output(filename);
    output << content;
}

void RemoveFile(const std::string& filename) {
    if (std::filesystem::exists(filename)) {
        std::filesystem::remove(filename);
    }
}

int64_t CalcNumericProduct(const std::string& filename, const Schema& schema) {
    int64_t product = 1;

    std::ifstream input(filename);
    std::string line;

    while (std::getline(input, line)) {
        if (line.empty())
            continue;

        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;

        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        for (size_t i = 0; i < schema.GetColumnCount(); ++i) {
            auto type = schema.GetColumn(i).type;
            if (type == Types::DataType::INT16 ||
                type == Types::DataType::INT32 ||
                type == Types::DataType::INT64) {
                product = (product * std::stoll(fields[i])) % kMod;
            }
        }
    }

    return product;
}

class FixtureE2E : public ::testing::Test {
protected:
    void SetUp() override {
        RemoveFile(kTestInputDataCsv);
        RemoveFile(kTestInputSchemaCsv);
        RemoveFile(kTestIyxFile);
        RemoveFile(kTestOutputDataCsv);
        RemoveFile(kTestOutputSchemaCsv);
    }

    void TearDown() override {
        RemoveFile(kTestInputDataCsv);
        RemoveFile(kTestInputSchemaCsv);
        RemoveFile(kTestIyxFile);
        RemoveFile(kTestOutputDataCsv);
        RemoveFile(kTestOutputSchemaCsv);
    }
};

}  // namespace

TEST_F(FixtureE2E, CsvToIyxToCsvWithNumericSum) {
    std::string originalCsv =
        "1,100,Alice\n"
        "2,200,Bob\n"
        "3,300,Charlie\n"
        "4,400,Diana\n"
        "5,500,Eve\n";

    std::string originalSchema =
        "id,int32\n"
        "score,int64\n"
        "name,string\n";

    WriteFile(kTestInputDataCsv, originalCsv);
    WriteFile(kTestInputSchemaCsv, originalSchema);

    Schema inputSchema = Parser::LoadSchemaFromCsv(kTestInputSchemaCsv);

    int64_t inputProduct = CalcNumericProduct(kTestInputDataCsv, inputSchema);
    EXPECT_EQ(inputProduct, 998992007)
        << "Input CSV numeric product should be 998992007";

    size_t writtenRowGroups = 0;
    size_t writtenTotalRows = 0;

    {
        IO::CsvReader csvReader(kTestInputDataCsv, inputSchema);
        IO::FormatWriter formatWriter(kTestIyxFile);
        formatWriter.Begin(inputSchema);

        while (auto batch = csvReader.ReadBatch()) {
            RowGroup rg(std::move(*batch));
            formatWriter.WriteRowGroup(rg);
        }

        formatWriter.End();

        writtenRowGroups = formatWriter.GetRowGroupCount();
        writtenTotalRows = formatWriter.GetTotalRowsWritten();

        Parser::SaveSchemaToCsv(inputSchema, kTestOutputSchemaCsv);
    }

    EXPECT_EQ(writtenTotalRows, 5) << "Writer should report 5 rows written";
    EXPECT_GE(writtenRowGroups, 1) << "Writer should have at least 1 row group";

    {
        IO::FormatReader formatReader(kTestIyxFile);
        formatReader.Open();

        const Schema& iyxSchema = formatReader.GetSchema();
        EXPECT_EQ(iyxSchema.GetColumnCount(), inputSchema.GetColumnCount())
            << "Schema column count mismatch";

        for (size_t i = 0; i < inputSchema.GetColumnCount(); ++i) {
            EXPECT_EQ(iyxSchema.GetColumn(i).name,
                      inputSchema.GetColumn(i).name)
                << "Column " << i << " name mismatch";
            EXPECT_EQ(iyxSchema.GetColumn(i).type,
                      inputSchema.GetColumn(i).type)
                << "Column " << i << " type mismatch";
        }

        EXPECT_EQ(formatReader.GetRowGroupCount(), writtenRowGroups)
            << "Row group count mismatch between writer and reader";

        EXPECT_EQ(formatReader.GetTotalRowCount(), writtenTotalRows)
            << "Total row count mismatch between writer and reader";

        for (size_t i = 0; i < formatReader.GetRowGroupCount(); ++i) {
            const RowGroupMeta& meta = formatReader.GetRowGroupMeta(i);
            EXPECT_GT(meta.rowCount, 0)
                << "Row group " << i << " should have rows";
            EXPECT_GT(meta.size, 0)
                << "Row group " << i << " should have non-zero size";
        }

        IO::CsvWriter csvWriter(kTestOutputDataCsv);

        while (formatReader.HasMore()) {
            auto batch = formatReader.ReadBatch();
            if (batch) {
                csvWriter.WriteBatch(*batch);
            }
        }
    }

    Schema outputSchema = Parser::LoadSchemaFromCsv(kTestOutputSchemaCsv);
    EXPECT_EQ(outputSchema.GetColumnCount(), inputSchema.GetColumnCount())
        << "Saved schema column count mismatch";

    uint64_t outputProduct =
        CalcNumericProduct(kTestOutputDataCsv, inputSchema);

    EXPECT_EQ(inputProduct, outputProduct)
        << "Numeric product should be preserved: input=" << inputProduct
        << ", output=" << outputProduct;
}

TEST_F(FixtureE2E, LargeDataMultipleRowGroups) {
    std::string largeData;
    const size_t numRows = 5000;

    for (size_t i = 1; i <= numRows; ++i) {
        largeData += std::to_string(i) + "," + std::to_string(i * 2) + "\n";
    }

    std::string schemaContent =
        "id,int64\n"
        "value,int64\n";

    WriteFile(kTestInputDataCsv, largeData);
    WriteFile(kTestInputSchemaCsv, schemaContent);

    Schema schema = Parser::LoadSchemaFromCsv(kTestInputSchemaCsv);
    uint64_t inputProduct = CalcNumericProduct(kTestInputDataCsv, schema);

    size_t writtenRowGroups = 0;

    {
        IO::CsvReader csvReader(kTestInputDataCsv, schema);
        IO::FormatWriter formatWriter(kTestIyxFile);
        formatWriter.Begin(schema);

        while (auto batch = csvReader.ReadBatch()) {
            RowGroup rg(std::move(*batch));
            formatWriter.WriteRowGroup(rg);
        }

        formatWriter.End();
        writtenRowGroups = formatWriter.GetRowGroupCount();

        // минимум 3 row groups (5000 / 2048 ≈ 2.44)
        EXPECT_GE(writtenRowGroups, 3)
            << "Should have at least 3 row groups for 5000 rows";
        EXPECT_EQ(formatWriter.GetTotalRowsWritten(), numRows);
    }

    {
        IO::FormatReader formatReader(kTestIyxFile);
        formatReader.Open();

        EXPECT_EQ(formatReader.GetRowGroupCount(), writtenRowGroups);
        EXPECT_EQ(formatReader.GetTotalRowCount(), numRows);

        uint64_t sumRowCounts = 0;
        for (size_t i = 0; i < formatReader.GetRowGroupCount(); ++i) {
            sumRowCounts += formatReader.GetRowGroupMeta(i).rowCount;
        }
        EXPECT_EQ(sumRowCounts, numRows)
            << "Sum of row group row counts should equal total rows";

        IO::CsvWriter csvWriter(kTestOutputDataCsv);

        while (formatReader.HasMore()) {
            auto batch = formatReader.ReadBatch();
            if (batch) {
                csvWriter.WriteBatch(*batch);
            }
        }
    }

    uint64_t outputProduct = CalcNumericProduct(kTestOutputDataCsv, schema);

    EXPECT_EQ(inputProduct, outputProduct)
        << "Large data: product should be preserved";
}

}  // namespace Columnar::Test
