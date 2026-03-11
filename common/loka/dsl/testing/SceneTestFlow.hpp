#ifndef LOKA_DSL_TESTING_SCENE_TEST_FLOW_HPP
#define LOKA_DSL_TESTING_SCENE_TEST_FLOW_HPP

#include <string>

#include "app/Text.hpp"
#include "app/scene/Scene.hpp"
#include "loka/dsl/SnapFlow.hpp"
#include "loka/platform/StringUTF8.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class SceneTestAccess
      {
      public:
        static ::loka::app::scene::Node *rootNode(const ::loka::app::scene::Scene &scene)
        {
          return scene.rootNode_;
        }
      };

      enum SceneTestFlowErrorKind
      {
        FLOW_ERROR_KIND_SCENE_SCENARIO = 1002,
        FLOW_ERROR_KIND_SCENE_TEST_ASSERT = 1003
      };

      enum SceneTestFlowErrorCode
      {
        FLOW_ERROR_SCENE_TEST_NULL_SCENE = 1,
        FLOW_ERROR_SCENE_TEST_ROOT_UNAVAILABLE = 2,
        FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND = 3,
        FLOW_ERROR_SCENE_TEST_NODE_TYPE_MISMATCH = 4,
        FLOW_ERROR_SCENE_TEST_DUPLICATE_TEST_ID = 5,
        FLOW_ERROR_SCENE_TEST_MISSING_TEST_ID = 6,
        FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE = 7,
        FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED = 8
      };

      template <class NodeT>
      struct SceneNodeCast;
      // Add explicit specializations for each supported node type so misuse fails at compile time,
      // not later as an unresolved symbol.

      template <class NodeT>
      struct SceneNodeCast
      {
        static NodeT *cast(::loka::app::scene::Node *)
        {
          typedef char LokaSceneNodeCastRequiresSpecialization[(sizeof(NodeT) == 0) ? 1 : -1];
          (void)sizeof(LokaSceneNodeCastRequiresSpecialization);
          return 0;
        }
      };

      template <>
      struct SceneNodeCast< ::loka::app::TextNode>
      {
        static ::loka::app::TextNode *cast(::loka::app::scene::Node *node)
        {
          return node ? node->asTextNode() : 0;
        }
      };

      namespace scene_test_detail
      {
        template <class NodeT>
        static void findNodeByIdRecursive(::loka::app::scene::Node *node,
                                          const std::string &testId,
                                          long &idMatches,
                                          long &typedMatches,
                                          NodeT *&result)
        {
          if (!node)
          {
            return;
          }

          if (node->testId() == testId)
          {
            ++idMatches;
            NodeT *typed = SceneNodeCast<NodeT>::cast(node);
            if (typed)
            {
              ++typedMatches;
              if (!result)
              {
                result = typed;
              }
            }
          }

          ::loka::app::scene::INestable *nestable = node->asNestable();
          if (!nestable)
          {
            return;
          }

          ::loka::app::scene::Node *child = nestable->childrenHead();
          while (child)
          {
            findNodeByIdRecursive<NodeT>(child, testId, idMatches, typedMatches, result);
            child = child->nextInComposition;
          }
        }
      } // namespace scene_test_detail

      template <class NodeT>
      static StepRunStatus LookupNodeById(::loka::app::scene::Scene *scene,
                                          const std::string &testId,
                                          NodeT *&out,
                                          FlowError &error);

      template <class NodeT>
      class FindNodeByIdAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef NodeT *Out;

        explicit FindNodeByIdAdapter(const char *testId)
            : testId_(testId ? testId : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          return LookupNodeById<NodeT>(in, testId_, out, error);
        }

      private:
        std::string testId_;
      };

      template <class NodeT>
      inline FindNodeByIdAdapter<NodeT> FindNodeById(const char *testId)
      {
        return FindNodeByIdAdapter<NodeT>(testId);
      }

      template <class NodeT>
      static StepRunStatus LookupNodeById(::loka::app::scene::Scene *scene,
                                          const std::string &testId,
                                          NodeT *&out,
                                          FlowError &error)
      {
        out = 0;
        if (!scene)
        {
          error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
          error.code = FLOW_ERROR_SCENE_TEST_NULL_SCENE;
          return FLOW_STEP_FAILED;
        }
        ::loka::app::scene::Node *root = SceneTestAccess::rootNode(*scene);
        if (!root)
        {
          error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
          error.code = FLOW_ERROR_SCENE_TEST_ROOT_UNAVAILABLE;
          return FLOW_STEP_FAILED;
        }
        long idMatches = 0;
        long typedMatches = 0;
        scene_test_detail::findNodeByIdRecursive<NodeT>(root, testId, idMatches, typedMatches, out);
        if (idMatches == 0)
        {
          error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
          error.code = FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND;
          return FLOW_STEP_FAILED;
        }
        if (idMatches > 1)
        {
          error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
          error.code = FLOW_ERROR_SCENE_TEST_DUPLICATE_TEST_ID;
          return FLOW_STEP_FAILED;
        }
        if (typedMatches == 0 || !out)
        {
          error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
          error.code = FLOW_ERROR_SCENE_TEST_NODE_TYPE_MISMATCH;
          return FLOW_STEP_FAILED;
        }
        return FLOW_STEP_SUCCEEDED;
      }

      template <class NodeT>
      struct SceneCaptureTraits;

      template <>
      struct SceneCaptureTraits< ::loka::app::TextNode>
      {
        static bool capture(::loka::app::TextNode *node, SnapRecord &out)
        {
          if (!node || !node->props.text_)
          {
            return false;
          }
          std::string utf8;
          if (!::loka::platform::CollectUtf8(node->props.text_->get(), utf8))
          {
            return false;
          }
          out.set("text.value", utf8.c_str());
          return true;
        }
      };

      template <class NodeT>
      class CaptureNodeAdapter
      {
      public:
        typedef NodeT *In;
        typedef SnapRecord Out;

        CaptureNodeAdapter(const char *testName,
                           const char *stepName,
                           long tick,
                           long scenarioVersion,
                           const char *status = SNAP_STATUS_OK)
            : testName_(testName ? testName : ""),
              stepName_(stepName ? stepName : ""),
              tick_(tick),
              scenarioVersion_(scenarioVersion),
              status_(status ? status : SNAP_STATUS_OK) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          if (!in)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND;
            return FLOW_STEP_FAILED;
          }
          const std::string &nodeId = in->testId();
          if (nodeId.empty())
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_MISSING_TEST_ID;
            return FLOW_STEP_FAILED;
          }

          BuildSnapV1RecordAdapter base(testName_.c_str(),
                                        stepName_.c_str(),
                                        nodeId.c_str(),
                                        tick_,
                                        scenarioVersion_,
                                        status_.c_str());
          const int unused = 0;
          FlowError ignored;
          if (base.run(unused, out, ignored) != FLOW_STEP_SUCCEEDED)
          {
            error.kind = FLOW_ERROR_KIND_SNAP;
            error.code = FLOW_ERROR_SNAP_WRITE_FAILED;
            return FLOW_STEP_FAILED;
          }
          if (!SceneCaptureTraits<NodeT>::capture(in, out))
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string testName_;
        std::string stepName_;
        long tick_;
        long scenarioVersion_;
        std::string status_;
      };

      template <class NodeT>
      inline CaptureNodeAdapter<NodeT> CaptureNode(const char *testName,
                                                   const char *stepName,
                                                   long tick,
                                                   long scenarioVersion,
                                                   const char *status)
      {
        return CaptureNodeAdapter<NodeT>(testName, stepName, tick, scenarioVersion, status);
      }

      template <class NodeT>
      inline CaptureNodeAdapter<NodeT> CaptureNode(const char *testName,
                                                   const char *stepName,
                                                   long tick,
                                                   long scenarioVersion)
      {
        return CaptureNodeAdapter<NodeT>(testName, stepName, tick, scenarioVersion);
      }

      class AssertSnapStringEqualsAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        AssertSnapStringEqualsAdapter(const char *key, const char *expected)
            : key_(key ? key : ""),
              expected_(expected ? expected : "") {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          std::string actual;
          if (key_.empty() || !in.get(key_.c_str(), actual))
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          if (actual != expected_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string key_;
        std::string expected_;
      };

      inline AssertSnapStringEqualsAdapter AssertSnapStringEquals(const char *key, const char *expected)
      {
        return AssertSnapStringEqualsAdapter(key, expected);
      }

      class CheckSnapStringEqualsAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        CheckSnapStringEqualsAdapter(const char *key, const char *expected)
            : key_(key ? key : ""),
              expected_(expected ? expected : "") {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          return AssertSnapStringEquals(key_.c_str(), expected_.c_str()).run(in, out, error);
        }

      private:
        std::string key_;
        std::string expected_;
      };

      inline CheckSnapStringEqualsAdapter CheckSnapStringEquals(const char *key, const char *expected)
      {
        return CheckSnapStringEqualsAdapter(key, expected);
      }

      class AssertSnapIntLessEqualAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        AssertSnapIntLessEqualAdapter(const char *key, long maxValue)
            : key_(key ? key : ""),
              maxValue_(maxValue) {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          long actual = 0;
          if (key_.empty() || !in.getInt(key_.c_str(), actual))
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          if (actual > maxValue_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string key_;
        long maxValue_;
      };

      inline AssertSnapIntLessEqualAdapter AssertSnapIntLessEqual(const char *key, long maxValue)
      {
        return AssertSnapIntLessEqualAdapter(key, maxValue);
      }

      class AssertSnapIntGreaterEqualAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        AssertSnapIntGreaterEqualAdapter(const char *key, long minValue)
            : key_(key ? key : ""),
              minValue_(minValue) {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          long actual = 0;
          if (key_.empty() || !in.getInt(key_.c_str(), actual))
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          if (actual < minValue_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string key_;
        long minValue_;
      };

      inline AssertSnapIntGreaterEqualAdapter AssertSnapIntGreaterEqual(const char *key, long minValue)
      {
        return AssertSnapIntGreaterEqualAdapter(key, minValue);
      }

      class CheckSnapIntGreaterEqualAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        CheckSnapIntGreaterEqualAdapter(const char *key, long minValue)
            : key_(key ? key : ""),
              minValue_(minValue) {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          return AssertSnapIntGreaterEqual(key_.c_str(), minValue_).run(in, out, error);
        }

      private:
        std::string key_;
        long minValue_;
      };

      inline CheckSnapIntGreaterEqualAdapter CheckSnapIntGreaterEqual(const char *key, long minValue)
      {
        return CheckSnapIntGreaterEqualAdapter(key, minValue);
      }

      class CheckTextAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        CheckTextAdapter(const char *testId, const char *expected)
            : testId_(testId ? testId : ""),
              expected_(expected ? expected : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          ::loka::app::TextNode *node = 0;
          StepRunStatus lookupStatus = LookupNodeById< ::loka::app::TextNode>(in, testId_, node, error);
          if (lookupStatus != FLOW_STEP_SUCCEEDED)
          {
            return lookupStatus;
          }

          std::string actual;
          if (!node->props.text_ || !::loka::platform::CollectUtf8(node->props.text_->get(), actual))
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          if (actual != expected_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string testId_;
        std::string expected_;
      };

      inline CheckTextAdapter CheckText(const char *testId, const char *expected)
      {
        return CheckTextAdapter(testId, expected);
      }

      class CheckTimingLessEqualAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        CheckTimingLessEqualAdapter(const char *key, long maxValue)
            : key_(key ? key : ""),
              maxValue_(maxValue) {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          return AssertSnapIntLessEqual(key_.c_str(), maxValue_).run(in, out, error);
        }

      private:
        std::string key_;
        long maxValue_;
      };

      inline CheckTimingLessEqualAdapter CheckTimingLessEqual(const char *key, long maxValue)
      {
        return CheckTimingLessEqualAdapter(key, maxValue);
      }

      class AssertTextEqualsAdapter
      {
      public:
        typedef ::loka::app::TextNode *In;
        typedef ::loka::app::TextNode *Out;

        explicit AssertTextEqualsAdapter(const char *expected)
            : expected_(expected ? expected : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          if (!in || !in->props.text_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }

          std::string actual;
          if (!::loka::platform::CollectUtf8(in->props.text_->get(), actual))
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }

          if (actual != expected_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string expected_;
      };

      inline AssertTextEqualsAdapter AssertTextEquals(const char *expected)
      {
        return AssertTextEqualsAdapter(expected);
      }
    } // namespace testing
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_TESTING_SCENE_TEST_FLOW_HPP
