/*
Created by Zhijie Zhang,
Use prefix to obtain tags first
*/

#ifndef STORAGE_EXEC_NEWGETPROPNODE_H_
#define STORAGE_EXEC_NEWGETPROPNODE_H_

#include "common/base/Base.h"
#include "storage/exec/EdgeNode.h"
#include "storage/exec/TagNode.h"

namespace nebula {
namespace storage {

class NewGetTagPropNode : public QueryNode<VertexID> {
 public:
  using RelNode<VertexID>::doExecute;

  NewGetTagPropNode(RuntimeContext* context,
                 std::vector<TagNode*> tagNodes,
                 std::unordered_map<TagID, uint32_t> tagIdxMap,
                 nebula::DataSet* resultDataSet,
                 std::size_t row_length,
                 Expression* filter,
                 std::size_t limit)
      : context_(context),
        tagNodes_(std::move(tagNodes)),
        tagIdxMap_(std::move(tagIdxMap)),
        resultDataSet_(resultDataSet),
        expCtx_(filter == nullptr
                    ? nullptr
                    : new StorageExpressionContext(context->vIdLen(), context->isIntId())),
        row_length_(row_length),
        filter_(filter),
        limit_(limit) {
    name_ = "GetTagPropNode";
  }

  nebula::cpp2::ErrorCode doExecute(PartitionID partId, const VertexID& vId) override {
    // LOG(INFO) << "(debug) calling new get prop node: Vid is" << vId;

    if (resultDataSet_->size() >= limit_) {
      return nebula::cpp2::ErrorCode::SUCCEEDED;
    }

    // Extract TagID range with prefix
    auto ret = resetIter(partId, vId); // reset Iter
    if (ret != nebula::cpp2::ErrorCode::SUCCEEDED) {
      return ret;
    }


    // if none of tags valid, do not emplace the row
    if (!(iter_ && iter_->valid())){
      auto kvstore = context_->env()->kvstore_;
      auto vertexKey = NebulaKeyUtils::vertexKey(context_->vIdLen(), partId, vId);
      std::string value;
      ret = kvstore->get(context_->spaceId(), partId, vertexKey, &value);
      if (ret == nebula::cpp2::ErrorCode::E_KEY_NOT_FOUND) {
        return nebula::cpp2::ErrorCode::SUCCEEDED;
      } else if (ret != nebula::cpp2::ErrorCode::SUCCEEDED) {
        return ret;
      }
    }

    List row;
    row.reserve(row_length_);
    // vertexId is the first column
    if (context_->isIntId()) {
      row.emplace_back(*reinterpret_cast<const int64_t*>(vId.data()));
    } else {
      row.emplace_back(vId);
    }
    auto vIdLen = context_->vIdLen();
    auto isIntId = context_->isIntId();
    
    // traverse the TagNode with iter_
    for (; iter_ && iter_->valid(); iter_->next()){
      TagID tagId = NebulaKeyUtils::getTagId(context_->vIdLen(), iter_->key());
      uint32_t tagIdx = tagIdxMap_[tagId]; 
      TagNode * tagNode = tagNodes_[tagIdx];
      ret = tagNode->execute(partId, vId);
      if (ret != nebula::cpp2::ErrorCode::SUCCEEDED) {
        return ret;
      }
    }

    for (auto* tagNode : tagNodes_) {
      // collectTagProps
      ret =  tagNode->collectTagPropsIfValid(
          [&row, tagNode, this](const std::vector<PropContext>* props) -> nebula::cpp2::ErrorCode {
            for (const auto& prop : *props) {
              if (prop.returned_) {       // although no prop (but such prop need to be returned), add an empty
                row.emplace_back(Value());
              }
              if (prop.filtered_ && expCtx_ != nullptr) {   // if filtered == null, not used
                expCtx_->setTagProp(tagNode->getTagName(), prop.name_, Value()); 
              }
            }
            return nebula::cpp2::ErrorCode::SUCCEEDED;
          },
          [&row, vIdLen, isIntId, tagNode, this](
              folly::StringPiece key,
              RowReader* reader,
              const std::vector<PropContext>* props) -> nebula::cpp2::ErrorCode {
            LOG(INFO) << "(debug) tagNode info: {tagId:" << tagNode->tagId() << ", isValid:" << tagNode->valid() << ", tagName:" << tagNode->getTagName() << "}";
            auto status = QueryUtils::collectVertexProps(
                key, vIdLen, isIntId, reader, props, row, expCtx_.get(), tagNode->getTagName());
            if (!status.ok()) {
              return nebula::cpp2::ErrorCode::E_TAG_PROP_NOT_FOUND;
            }
            LOG(INFO) << "(debug) current row:" << row.toString();
            tagNode->clear();
            return nebula::cpp2::ErrorCode::SUCCEEDED;
          });
      // @debug: what we get in Row?
      if (ret != nebula::cpp2::ErrorCode::SUCCEEDED) {
        return ret;
      }
    }
    // // @debug: how many key in iter_?
    // LOG(INFO) << "(debug) Total Iter key number:" << key_count;

    if (filter_ == nullptr || (QueryUtils::vTrue(filter_->eval(*expCtx_)))) {
      resultDataSet_->rows.emplace_back(std::move(row));
    }
    if (expCtx_ != nullptr) {
      expCtx_->clear();
    }
    return nebula::cpp2::ErrorCode::SUCCEEDED;
  }

 private:
  RuntimeContext* context_;
  std::vector<TagNode*> tagNodes_;  // vector used to keep the order
  std::unordered_map<TagID, uint32_t> tagIdxMap_; // unordered map to extract TagNode
  nebula::DataSet* resultDataSet_;
  std::unique_ptr<StorageExpressionContext> expCtx_{nullptr};

  std::size_t row_length_{0};

  Expression* filter_{nullptr};
  const std::size_t limit_{std::numeric_limits<std::size_t>::max()};
  
  std::unique_ptr<kvstore::KVIterator> iter_;

  std::string buildVidPrefix(size_t vIdLen, PartitionID partId, const VertexID& vId){
    CHECK_GE(vIdLen, vId.size());
    int32_t item = (partId << kPartitionOffset) | static_cast<uint32_t>(NebulaKeyType::kTag_);

    std::string prefix;
    prefix.reserve(sizeof(int32_t) + vIdLen);
    prefix.append(reinterpret_cast<const char*>(&item), sizeof(int32_t))
       .append(vId.data(), vId.size());
    return prefix;
  }

  nebula::cpp2::ErrorCode resetIter(PartitionID partId, const VertexID& vId){
    nebula::cpp2::ErrorCode ret;
    auto prefix = buildVidPrefix(context_->vIdLen(), partId, vId);
    ret = context_->env()->kvstore_->prefix(context_->spaceId(), partId, prefix, &iter_);

    return ret;
  }
};

}  // namespace storage
}  // namespace nebula

#endif  // STORAGE_EXEC_NEWGETPROPNODE_H_
