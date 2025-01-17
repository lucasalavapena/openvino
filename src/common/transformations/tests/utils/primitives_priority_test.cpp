// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <cpp/ie_cnn_network.h>
#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>

#include "common_test_utils/ov_test_utils.hpp"
#include "common_test_utils/test_common.hpp"
#include "ie_ngraph_utils.hpp"
#include "openvino/core/model.hpp"
#include "openvino/opsets/opset1.hpp"
#include "transformations/rt_info/primitives_priority_attribute.hpp"
#include "transformations/utils/utils.hpp"

using namespace ov;
using namespace testing;
using namespace InferenceEngine;
using namespace InferenceEngine::details;

TEST(TransformationTests, ConvBiasFusion) {
    std::shared_ptr<ov::Model> f(nullptr);
    {
        auto input1 = std::make_shared<opset1::Parameter>(element::f32, Shape{1, 3, 64, 64});
        auto weights = opset1::Constant::create(element::f32, Shape{3, 3, 1, 1}, {1});
        auto bias = opset1::Constant::create(element::f32, Shape{3, 1, 1}, {1});
        auto conv = std::make_shared<opset1::Convolution>(input1,
                                                          weights,
                                                          Strides{1, 1},
                                                          CoordinateDiff{0, 0},
                                                          CoordinateDiff{0, 0},
                                                          Strides{1, 1});

        auto add = std::make_shared<opset1::Add>(conv, bias);
        add->set_friendly_name("add");

        f = std::make_shared<ov::Model>(NodeVector{add}, ParameterVector{input1});
    }

    std::unordered_map<std::string, std::string> pp;

    InferenceEngine::CNNNetwork network(f);

    // Set PrimitivesPriority to all Convolutions
    auto model = network.getFunction();
    ASSERT_NE(nullptr, model);
    for (auto& op : model->get_ops()) {
        if (auto conv = std::dynamic_pointer_cast<opset1::Convolution>(op)) {
            auto& rtInfo = conv->get_rt_info();
            rtInfo[ov::PrimitivesPriority::get_type_info_static()] = ov::PrimitivesPriority("test");
            pp[op->get_friendly_name()] = "test";
        }
    }

    auto clonedNetwork = InferenceEngine::details::cloneNetwork(network);
    auto funcs = clonedNetwork.getFunction();

    for (auto& op : funcs->get_ops()) {
        if (auto conv = std::dynamic_pointer_cast<opset1::Convolution>(op)) {
            ASSERT_TRUE(pp.find(op->get_friendly_name()) != pp.end());
            ASSERT_EQ(pp[op->get_friendly_name()], "test");
        }
    }
}
