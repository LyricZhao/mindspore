/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

#include "utils/log_adapter.h"
#include "common/utils.h"
#include "common/common.h"
#include "gtest/gtest.h"
#include "securec.h"
#include "minddata/dataset/include/datasets.h"
#include "minddata/dataset/include/status.h"
#include "minddata/dataset/include/transforms.h"
#include "minddata/dataset/include/iterator.h"
#include "minddata/dataset/core/constants.h"
#include "minddata/dataset/include/samplers.h"

using namespace mindspore::dataset::api;
using mindspore::MsLogLevel::ERROR;
using mindspore::ExceptionType::NoExceptionType;
using mindspore::LogStream;
using mindspore::dataset::Tensor;
using mindspore::dataset::Status;
using mindspore::dataset::BorderType;


class MindDataTestPipeline : public UT::DatasetOpTesting {
 protected:
};


TEST_F(MindDataTestPipeline, TestBatchAndRepeat) {
  // Create a Mnist Dataset
  std::string folder_path = datasets_root_path_ + "/testMnistData/";
  std::shared_ptr<Dataset> ds = Mnist(folder_path, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 2;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 10);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestTensorOpsAndMap) {
  // Create a Mnist Dataset
  std::string folder_path = datasets_root_path_ + "/testMnistData/";
  std::shared_ptr<Dataset> ds = Mnist(folder_path, RandomSampler(false, 20));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> resize_op = vision::Resize({30, 30});
  EXPECT_TRUE(resize_op != nullptr);

  std::shared_ptr<TensorOperation> center_crop_op = vision::CenterCrop({16, 16});
  EXPECT_TRUE(center_crop_op != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({resize_op, center_crop_op});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 40);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestUniformAugWithOps) {
  // Create a Mnist Dataset
  std::string folder_path = datasets_root_path_ + "/testMnistData/";
  std::shared_ptr<Dataset> ds = Mnist(folder_path, RandomSampler(false, 20));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 1;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> resize_op = vision::Resize({30, 30});
  EXPECT_TRUE(resize_op != nullptr);

  std::shared_ptr<TensorOperation> random_crop_op = vision::RandomCrop({28, 28});
  EXPECT_TRUE(random_crop_op != nullptr);

  std::shared_ptr<TensorOperation> center_crop_op = vision::CenterCrop({16, 16});
  EXPECT_TRUE(center_crop_op != nullptr);

  std::shared_ptr<TensorOperation> uniform_aug_op = vision::UniformAugment({random_crop_op, center_crop_op}, 2);
  EXPECT_TRUE(uniform_aug_op != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({resize_op, uniform_aug_op});
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestRandomFlip) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> random_vertical_flip_op = vision::RandomVerticalFlip(0.5);
  EXPECT_TRUE(random_vertical_flip_op != nullptr);

  std::shared_ptr<TensorOperation> random_horizontal_flip_op = vision::RandomHorizontalFlip(0.5);
  EXPECT_TRUE(random_horizontal_flip_op != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({random_vertical_flip_op, random_horizontal_flip_op});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestImageFolderBatchAndRepeat) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 2;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 10);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestImageFolderWithSamplers) {
  std::shared_ptr<SamplerObj> sampl = DistributedSampler(2, 1);
  EXPECT_NE(sampl, nullptr);

  sampl = PKSampler(3);
  EXPECT_NE(sampl, nullptr);

  sampl = RandomSampler(false, 12);
  EXPECT_NE(sampl, nullptr);

  sampl = SequentialSampler(0, 12);
  EXPECT_NE(sampl, nullptr);

  std::vector<double> weights = {0.9, 0.8, 0.68, 0.7, 0.71, 0.6, 0.5, 0.4, 0.3, 0.5, 0.2, 0.1};
  sampl = WeightedRandomSampler(weights, 12);
  EXPECT_NE(sampl, nullptr);

  std::vector<int64_t> indices = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
  sampl = SubsetRandomSampler(indices);
  EXPECT_NE(sampl, nullptr);

  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, false, sampl);
  EXPECT_NE(ds, nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_NE(ds, nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 2;
  ds = ds->Batch(batch_size);
  EXPECT_NE(ds, nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_NE(iter, nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 12);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestPad) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> pad_op1 = vision::Pad({1, 2, 3, 4}, {0}, BorderType::kSymmetric);
  EXPECT_TRUE(pad_op1 != nullptr);

  std::shared_ptr<TensorOperation> pad_op2 = vision::Pad({1}, {1, 1, 1}, BorderType::kEdge);
  EXPECT_TRUE(pad_op2 != nullptr);

  std::shared_ptr<TensorOperation> pad_op3 = vision::Pad({1, 4});
  EXPECT_TRUE(pad_op3 != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({pad_op1, pad_op2, pad_op3});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
  i++;
  auto image = row["image"];
  MS_LOG(INFO) << "Tensor image shape: " << image->shape();
  iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestCutOut) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> cut_out1 = vision::CutOut(30, 5);
  EXPECT_TRUE(cut_out1!= nullptr);

  std::shared_ptr<TensorOperation> cut_out2 = vision::CutOut(30);
  EXPECT_TRUE(cut_out2 != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({cut_out1, cut_out2});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
  i++;
  auto image = row["image"];
  MS_LOG(INFO) << "Tensor image shape: " << image->shape();
  iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestNormalize) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> normalize = vision::Normalize({121.0, 115.0, 100.0}, {70.0, 68.0, 71.0});
  EXPECT_TRUE(normalize != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({normalize});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestDecode) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, false, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> decode = vision::Decode(true);
  EXPECT_TRUE(decode != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({decode});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }
  EXPECT_EQ(i, 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestShuffleDataset) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Shuffle operation on ds
  int32_t shuffle_size = 10;
  ds = ds->Shuffle(shuffle_size);
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 2;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 10);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestCifar10Dataset) {

  // Create a Cifar10 Dataset
  std::string folder_path = datasets_root_path_ + "/testCifar10Data/";
  std::shared_ptr<Dataset> ds = Cifar10(folder_path, 0, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 2;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
  i++;
  auto image = row["image"];
  MS_LOG(INFO) << "Tensor image shape: " << image->shape();
  iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 10);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestRandomColorAdjust) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> random_color_adjust1 = vision::RandomColorAdjust({1.0}, {0.0}, {0.5}, {0.5});
  EXPECT_TRUE(random_color_adjust1 != nullptr);

  std::shared_ptr<TensorOperation> random_color_adjust2 = vision::RandomColorAdjust({1.0, 1.0}, {0.0, 0.0}, {0.5, 0.5},
                                                                                    {0.5, 0.5});
  EXPECT_TRUE(random_color_adjust2 != nullptr);

  std::shared_ptr<TensorOperation> random_color_adjust3 = vision::RandomColorAdjust({0.5, 1.0}, {0.0, 0.5}, {0.25, 0.5},
                                                                             {0.25, 0.5});
  EXPECT_TRUE(random_color_adjust3 != nullptr);

  std::shared_ptr<TensorOperation> random_color_adjust4 = vision::RandomColorAdjust();
  EXPECT_TRUE(random_color_adjust4 != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({random_color_adjust1, random_color_adjust2, random_color_adjust3, random_color_adjust4});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
  i++;
  auto image = row["image"];
  MS_LOG(INFO) << "Tensor image shape: " << image->shape();
  iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestRandomRotation) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> random_rotation_op = vision::RandomRotation({-180, 180});
  EXPECT_TRUE(random_rotation_op != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({random_rotation_op});
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}

TEST_F(MindDataTestPipeline, TestProjectMap) {
  // Create an ImageFolder Dataset
  std::string folder_path = datasets_root_path_ + "/testPK/data/";
  std::shared_ptr<Dataset> ds = ImageFolder(folder_path, true, RandomSampler(false, 10));
  EXPECT_TRUE(ds != nullptr);

  // Create a Repeat operation on ds
  int32_t repeat_num = 2;
  ds = ds->Repeat(repeat_num);
  EXPECT_TRUE(ds != nullptr);

  // Create objects for the tensor ops
  std::shared_ptr<TensorOperation> random_vertical_flip_op = vision::RandomVerticalFlip(0.5);
  EXPECT_TRUE(random_vertical_flip_op != nullptr);

  // Create a Map operation on ds
  ds = ds->Map({random_vertical_flip_op}, {}, {}, {"image", "label"});
  EXPECT_TRUE(ds != nullptr);

  // Create a Project operation on ds
  std::vector<std::string> column_project = {"image"};
  ds = ds->Project(column_project);
  EXPECT_TRUE(ds != nullptr);

  // Create a Batch operation on ds
  int32_t batch_size = 1;
  ds = ds->Batch(batch_size);
  EXPECT_TRUE(ds != nullptr);

  // Create an iterator over the result of the above dataset
  // This will trigger the creation of the Execution Tree and launch it.
  std::shared_ptr<Iterator> iter = ds->CreateIterator();
  EXPECT_TRUE(iter != nullptr);

  // Iterate the dataset and get each row
  std::unordered_map<std::string, std::shared_ptr<Tensor>> row;
  iter->GetNextRow(&row);

  uint64_t i = 0;
  while (row.size() != 0) {
    i++;
    auto image = row["image"];
    MS_LOG(INFO) << "Tensor image shape: " << image->shape();
    iter->GetNextRow(&row);
  }

  EXPECT_TRUE(i == 20);

  // Manually terminate the pipeline
  iter->Stop();
}