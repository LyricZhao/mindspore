# Copyright 2020 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""
@File  : test_indexed_slices.py
@Author:
@Date  : 2020-06-08
@Desc  : test mindspore indexed_slices's operation
"""
import numpy as np

import mindspore as ms
import mindspore.nn as nn
from mindspore.ops import composite as C
from mindspore.ops import functional as F
from mindspore.ops import operations as P
from mindspore.ops.composite.multitype_ops.zeros_like_impl import zeros_like
from mindspore.ops.primitive import constexpr
from mindspore.ops._grad.grad_base import bprop_getters
from mindspore import Tensor, IndexedSlices, context
from mindspore.common.parameter import Parameter, ParameterTuple
from mindspore.common import dtype as mstype
from mindspore._checkparam import Validator as validator
from mindspore._checkparam import Rel
from mindspore.nn import Optimizer
from mindspore.nn import TrainOneStepCell, WithLossCell

context.set_context(mode=context.GRAPH_MODE, enable_sparse=True)

reduce_sum = P.ReduceSum()
unsorted_segment_sum = P.UnsortedSegmentSum()
transpose = P.Transpose()
shape_op = P.Shape()
reshape = P.Reshape()
size_op = P.Size()
invert_permutation = P.InvertPermutation()
logical_and = P.LogicalAnd()

@constexpr
def _generate_shape_index(out_shape, indices_shape, axis):
    out_rank = len(out_shape)
    ind_rank = len(indices_shape)
    if axis < 0:
        axis += out_rank - ind_rank + 1
    perm_part1 = tuple(range(axis, axis + ind_rank))
    index = tuple(range(out_rank))
    perm = perm_part1 + index[:axis] + index[axis + ind_rank:]
    return perm

@constexpr
def _generate_inverse_index(x_shape, axis):
    x_rank = len(x_shape)
    index = tuple(range(x_rank))
    if axis < 0:
        axis += x_rank
    perm = index[1:1 + axis] + (0,) + index[1 + axis:]
    return perm

class MySparseGatherV2(P.GatherV2):
    """
    For test
    """

@bprop_getters.register(MySparseGatherV2)
def get_bprop_sparse_gather_v2(self):
    """Generate bprop for MySparseGatherV2"""

    def bprop(x, indices, axis, out, dout):
        x_shp = shape_op(x)
        if axis == 0:
            indices_size = (size_op(indices),)
            x_tail_shp = x_shp[1:]
            values_shape = indices_size + x_tail_shp
            values = reshape(dout, values_shape)
            indices = reshape(indices, indices_size)
            return IndexedSlices(indices, values, x_shp), zeros_like(indices), zeros_like(axis)
        if F.rank(dout) == 0:
            dout = P.ExpandDims()(dout, -1)
        if F.rank(indices) == 0:
            indices = P.ExpandDims()(indices, -1)
        out_shp = shape_op(dout)
        ind_shp = shape_op(indices)
        # Example: out_shape:(3,2,3) axis 1 -> (1,0,2)
        perm_1 = _generate_shape_index(out_shp, ind_shp, axis)
        values_transpose = transpose(dout, perm_1)
        params_grad = unsorted_segment_sum(values_transpose, indices, shape_op(x)[axis])
        # Example: out_shape:(3,2,3) axis 2 -> (1,2,0)
        perm_2 = _generate_inverse_index(x_shp, axis)
        params_grad = transpose(params_grad, perm_2)
        return params_grad, zeros_like(indices), zeros_like(axis)

    return bprop

adam_opt_for_map = C.MultitypeFuncGraph("adam_opt_for_map")
@adam_opt_for_map.register("Tensor", "Tensor", "Tensor", "Tensor", "Tensor",
                           "Tensor", "Tensor", "Tensor", "IndexedSlices", "Bool")
def _update_run_op_for_map_indexed_slices(beta1, beta2, eps, lr, weight_decay_tensor, param,
                                          m, v, gradient, decay_flag):
    return gradient.values()

@adam_opt_for_map.register("Tensor", "Tensor", "Tensor", "Tensor", "Tensor",
                           "Tensor", "Tensor", "Tensor", "Tensor", "Bool")
def _update_run_op_for_map_tensor(beta1, beta2, eps, lr, weight_decay_tensor, param,
                                  m, v, gradient, decay_flag):
    op_mul = P.Mul()
    op_square = P.Square()
    op_sqrt = P.Sqrt()
    op_cast = P.Cast()
    op_reshape = P.Reshape()
    op_shape = P.Shape()

    param_fp32 = op_cast(param, mstype.float32)
    m_fp32 = op_cast(m, mstype.float32)
    v_fp32 = op_cast(v, mstype.float32)
    gradient_fp32 = op_cast(gradient, mstype.float32)

    next_m = op_mul(beta1, m_fp32) + op_mul(op_cast(F.tuple_to_array((1.0,)), mstype.float32) - beta1, gradient_fp32)

    next_v = op_mul(beta2, v_fp32) + op_mul(op_cast(F.tuple_to_array((1.0,)), mstype.float32)
                                            - beta2, op_square(gradient_fp32))

    update = next_m / (op_sqrt(next_v) + eps)
    if decay_flag:
        update = update + op_mul(weight_decay_tensor, param_fp32)

    update_with_lr = op_mul(lr, update)
    next_param = param_fp32 - op_reshape(update_with_lr, op_shape(param_fp32))

    next_v = F.depend(next_v, F.assign(param, next_param))
    next_v = F.depend(next_v, F.assign(m, next_m))
    next_v = F.depend(next_v, F.assign(v, next_v))
    return next_v


def _check_param_value(beta1, beta2, eps, weight_decay, prim_name):
    """Check the type of inputs."""
    validator.check_value_type("beta1", beta1, [float], prim_name)
    validator.check_value_type("beta2", beta2, [float], prim_name)
    validator.check_value_type("eps", eps, [float], prim_name)
    validator.check_value_type("weight_dacay", weight_decay, [float], prim_name)
    validator.check_number_range("beta1", beta1, 0.0, 1.0, Rel.INC_NEITHER, prim_name)
    validator.check_number_range("beta2", beta2, 0.0, 1.0, Rel.INC_NEITHER, prim_name)
    validator.check_number_range("eps", eps, 0.0, float("inf"), Rel.INC_NEITHER, prim_name)
    validator.check_number_range("weight_decay", weight_decay, 0.0, float("inf"), Rel.INC_LEFT, prim_name)


class AdamWeightDecaySparse(Optimizer):
    def __init__(self, params, learning_rate=1e-3, beta1=0.9, beta2=0.999, eps=1e-6, weight_decay=0.0,
                 decay_filter=lambda x: 'beta' not in x.name and 'gamma' not in x.name):
        super(AdamWeightDecaySparse, self).__init__(learning_rate, params)
        if self.is_group:
            raise RuntimeError(f"The {self.cls_name} optimizer cannot support group setting.")
        _check_param_value(beta1, beta2, eps, weight_decay, self.cls_name)
        self.beta1 = Tensor(np.array([beta1]).astype(np.float32))
        self.beta2 = Tensor(np.array([beta2]).astype(np.float32))
        self.eps = Tensor(np.array([eps]).astype(np.float32))
        self.weight_decay_tensor = Tensor(np.array([weight_decay]).astype(np.float32))

        self.params = self.parameters
        self.moments1 = self.params.clone(prefix="adam_m", init='zeros')
        self.moments2 = self.params.clone(prefix="adam_v", init='zeros')
        self.decay_flag = tuple(decay_filter(x) for x in self.params)
        self.map = C.Map()

    def construct(self, gradients):
        lr = self.get_lr()
        updated_velocity = self.map(F.partial(adam_opt_for_map, self.beta1, self.beta2, self.eps, lr,
                                              self.weight_decay_tensor),
                                    self.params, self.moments1, self.moments2, gradients, self.decay_flag)
        return updated_velocity


def test_indexed_slices_make_indexed_slices():
    class MakeIndexedSlices(nn.Cell):
        def __init__(self):
            super(MakeIndexedSlices, self).__init__()
            self.dense_shape = (3, 4)
        def construct(self, indices, values):
            ret = (IndexedSlices(indices, values, self.dense_shape),)
            return ret[0]
    indices = Tensor([[0, 0], [1, 2]])
    values = Tensor([1, 2], dtype=ms.float32)
    MakeIndexedSlices()(indices, values)


def test_indexed_slices_attr():
    class IndexedSlicesGetAttr(nn.Cell):
        def __init__(self):
            super(IndexedSlicesGetAttr, self).__init__()
            self.dense_shape = (3, 4)
        def construct(self, indices, values):
            x = IndexedSlices(indices, values, self.dense_shape)
            return x.values(), x.indices(), x.dense_shape()
    indices = Tensor([[0, 0], [1, 2]])
    values = Tensor([1, 2], dtype=ms.float32)
    IndexedSlicesGetAttr()(indices, values)


def test_indexed_slices_sparse_gatherv2_grad_all():
    grad_all = C.GradOperation('get_all', get_all=True)
    class GradWrap(nn.Cell):
        def __init__(self, network):
            super(GradWrap, self).__init__()
            self.network = network
        def construct(self, x, y):
            grad = grad_all(self.network)(x, y)
            return grad, grad[0], grad[1]
    class SparseGatherV2(nn.Cell):
        def __init__(self):
            super(SparseGatherV2, self).__init__()
            self.sparse_gatherv2 = MySparseGatherV2()
            self.axis = 0
        def construct(self, params, indices):
            return self.sparse_gatherv2(params, indices, self.axis)
    params = Tensor(np.ones([3, 1, 2]).astype(np.int32))
    indices = Tensor(np.array([0, 1]).astype(np.int32))
    GradWrap(SparseGatherV2())(params, indices)


def test_indexed_slices_sparse_gatherv2_grad_with_pram():
    grad_by_list = C.GradOperation('get_by_list', get_by_list=True)
    class GradWrap(nn.Cell):
        def __init__(self, network):
            super(GradWrap, self).__init__()
            self.network = network
            self.weights = ParameterTuple(filter(lambda x: x.requires_grad, network.get_parameters()))
        def construct(self, x):
            weights = self.weights
            grad = grad_by_list(self.network, weights)(x)
            x = grad[0]
            return x, x.values(), x.indices(), x.dense_shape()
    class SparseGatherV2(nn.Cell):
        def __init__(self):
            super(SparseGatherV2, self).__init__()
            self.sparse_gatherv2 = MySparseGatherV2()
            self.axis = 0
            self.params = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.int32)), name="params")
        def construct(self, indices):
            return self.sparse_gatherv2(self.params, indices, self.axis)
    indices = Tensor(np.array([0, 1]).astype(np.int32))
    network = GradWrap(SparseGatherV2())
    network(indices)


def test_indexed_slices_env_get():
    class Loss(nn.Cell):
        def __init__(self):
            super(Loss, self).__init__()
        def construct(self, base, target):
            return base
    class NetWithSparseGatherV2(nn.Cell):
        def __init__(self):
            super(NetWithSparseGatherV2, self).__init__()
            self.w1 = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="w1")
            self.w2 = Parameter(Tensor(np.ones([2, 1, 2]).astype(np.float32)), name="w2")
            self.gatherv2 = MySparseGatherV2()
            self.axis = 0
        def construct(self, indices):
            return self.gatherv2(self.w1, indices, self.axis) * self.w2

    inputs = Tensor(np.array([0, 1]).astype(np.int32))
    label = Tensor(np.zeros([2, 1, 2]).astype(np.float32))
    net = NetWithSparseGatherV2()
    net.set_train()
    loss = Loss()
    optimizer = AdamWeightDecaySparse(net.trainable_params())

    net_with_loss = WithLossCell(net, loss)
    train_network = TrainOneStepCell(net_with_loss, optimizer)
    train_network(inputs, label)
