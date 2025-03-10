/*!
 * Copyright 2023 XGBoost contributors
 */
#include <dmlc/parameter.h>
#include <gtest/gtest.h>
#include <xgboost/data.h>

#include <fstream>
#include <iostream>
#include <thread>

#include "../../../plugin/federated/federated_server.h"
#include "../../../src/collective/communicator-inl.h"
#include "../filesystem.h"
#include "../helpers.h"
#include "helpers.h"

namespace xgboost {

class FederatedDataTest : public BaseFederatedTest {
 public:
  void VerifyLoadUri(int rank) {
    InitCommunicator(rank);

    size_t constexpr kRows{16};
    size_t const kCols = 8 + rank;

    dmlc::TemporaryDirectory tmpdir;
    std::string path = tmpdir.path + "/small" + std::to_string(rank) + ".csv";
    CreateTestCSV(path, kRows, kCols);

    std::unique_ptr<DMatrix> dmat;
    std::string uri = path + "?format=csv";
    dmat.reset(DMatrix::Load(uri, false, DataSplitMode::kCol));

    ASSERT_EQ(dmat->Info().num_col_, 8 * kWorldSize + 3);
    ASSERT_EQ(dmat->Info().num_row_, kRows);

    for (auto const& page : dmat->GetBatches<SparsePage>()) {
      auto entries = page.GetView().data;
      auto index = 0;
      int offsets[] = {0, 8, 17};
      int offset = offsets[rank];
      for (auto row = 0; row < kRows; row++) {
        for (auto col = 0; col < kCols; col++) {
          EXPECT_EQ(entries[index].index, col + offset);
          index++;
        }
      }
    }

    xgboost::collective::Finalize();
  }
};

TEST_F(FederatedDataTest, LoadUri) {
  std::vector<std::thread> threads;
  for (auto rank = 0; rank < kWorldSize; rank++) {
    threads.emplace_back(&FederatedDataTest_LoadUri_Test::VerifyLoadUri, this, rank);
  }
  for (auto& thread : threads) {
    thread.join();
  }
}
}  // namespace xgboost
