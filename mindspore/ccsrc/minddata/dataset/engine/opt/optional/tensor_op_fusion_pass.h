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
#ifndef DATASET_TENSOR_OP_FUSION_PASS_H_
#define DATASET_TENSOR_OP_FUSION_PASS_H_

#include <memory>
#include "minddata/dataset/engine/opt/pass.h"

namespace mindspore {
namespace dataset {

/// \class TensorOpFusionPass tensor_op_fusion_pass.h
/// \brief And optional optimization pass identifying and fusing
///     tensor ops within MapOp
class TensorOpFusionPass : public NodePass {
  /// \brief Identifies and fuses tensor ops within MapOp
  /// \param[in] node The node being visited
  /// \param[inout] *modified indicates whether the node has been visited
  /// \return Status The error code return
  Status RunOnNode(std::shared_ptr<MapOp> node, bool *modified) override;
};
}  // namespace dataset
}  // namespace mindspore

#endif  // DATASET_TENSOR_OP_FUSION_PASS_H_
