/* Copyright (c) 2021 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include "graph/optimizer/rule/PushLimitDownIndexScanAppendVerticesRule.h"

#include "graph/optimizer/OptContext.h"
#include "graph/optimizer/OptGroup.h"
#include "graph/planner/plan/PlanNode.h"
#include "graph/planner/plan/Query.h"

using nebula::graph::AppendVertices;
using nebula::graph::Limit;
using nebula::graph::PlanNode;
using nebula::graph::QueryContext;
using nebula::graph::IndexScan;

namespace nebula {
namespace opt {

// Limit->AppendVertices->IndexScan ==> Limit->AppendVertices->IndexScan(Limit)

std::unique_ptr<OptRule> PushLimitDownIndexScanAppendVerticesRule::kInstance =
    std::unique_ptr<PushLimitDownIndexScanAppendVerticesRule>
        (new PushLimitDownIndexScanAppendVerticesRule());

PushLimitDownIndexScanAppendVerticesRule::PushLimitDownIndexScanAppendVerticesRule() {
  RuleSet::QueryRules().addRule(this);
}

const Pattern &PushLimitDownIndexScanAppendVerticesRule::pattern() const {
  static Pattern pattern =
      Pattern::create(graph::PlanNode::Kind::kLimit,
                      {Pattern::create(graph::PlanNode::Kind::kAppendVertices,
                                       {Pattern::create(graph::PlanNode::Kind::kIndexScan)})});
  return pattern;
}

StatusOr<OptRule::TransformResult> PushLimitDownIndexScanAppendVerticesRule::transform(
    OptContext *octx, const MatchedResult &matched) const {
  auto limitGroupNode = matched.node;
  auto appendVerticesGroupNode = matched.dependencies.front().node;
  auto indexScanGroupNode = matched.dependencies.front().dependencies.front().node;

  const auto limit = static_cast<const Limit *>(limitGroupNode->node());
  const auto appendVertices = static_cast<const AppendVertices *>(appendVerticesGroupNode->node());
  const auto indexScan = static_cast<const IndexScan *>(indexScanGroupNode->node());

  int64_t limitRows = limit->offset() + limit->count();
  if (indexScan->limit() >= 0 && limitRows >= indexScan->limit()) {
    return TransformResult::noTransform();
  }

  auto newLimit = static_cast<Limit *>(limit->clone());
  newLimit->setOutputVar(limit->outputVar());
  auto newLimitGroupNode = OptGroupNode::create(octx, newLimit, limitGroupNode->group());

  auto newAppendVertices = static_cast<AppendVertices *>(appendVertices->clone());
  auto newAppendVerticesGroup = OptGroup::create(octx);
  auto newAppendVerticesGroupNode = newAppendVerticesGroup->makeGroupNode(newAppendVertices);

  auto newIndexScan = static_cast<IndexScan *>(indexScan->clone());
  newIndexScan->setLimit(limitRows);
  auto newIndexScanGroup = OptGroup::create(octx);
  auto newIndexScanGroupNode = newIndexScanGroup->makeGroupNode(newIndexScan);

  newLimitGroupNode->dependsOn(newAppendVerticesGroup);
  newLimit->setInputVar(newAppendVertices->outputVar());
  newAppendVerticesGroupNode->dependsOn(newIndexScanGroup);
  newAppendVertices->setInputVar(newIndexScan->outputVar());
  for (auto dep : indexScanGroupNode->dependencies()) {
    newIndexScanGroupNode->dependsOn(dep);
  }

  TransformResult result;
  result.eraseAll = true;
  result.newGroupNodes.emplace_back(newLimitGroupNode);
  return result;
}

std::string PushLimitDownIndexScanAppendVerticesRule::toString() const {
  return "PushLimitDownIndexScanAppendVerticesRule";
}

}  // namespace opt
}  // namespace nebula
