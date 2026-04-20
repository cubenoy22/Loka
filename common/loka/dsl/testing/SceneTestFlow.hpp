#ifndef LOKA_DSL_TESTING_SCENE_TEST_FLOW_HPP
#define LOKA_DSL_TESTING_SCENE_TEST_FLOW_HPP

#include <cstdio>
#include <string>

#include "app/nodes/Text.hpp"
#include "app/scene/Scene.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
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

        static ::loka::app::scene::BoundaryNode *rootBoundary(const ::loka::app::scene::Scene &scene)
        {
          return scene.rootNode_ ? scene.rootNode_->asBoundary() : 0;
        }

        static ::loka::app::scene::SceneDirector &director(::loka::app::scene::Scene &scene)
        {
          return scene.director_;
        }

        static const ::loka::app::scene::SceneDirector &director(const ::loka::app::scene::Scene &scene)
        {
          return scene.director_;
        }

        static const ::loka::app::scene::PlatformApplyPlan &lastApplyPlan(const ::loka::app::scene::Scene &scene)
        {
          return scene.updateCycleState_.lastApplyPlan;
        }

        static const ::loka::app::scene::SceneDirector::SceneUpdateSnapshot &updateSnapshot(const ::loka::app::scene::Scene &scene)
        {
          return scene.updateCycleState_.pendingSnapshot;
        }

        static const ::loka::app::scene::SceneDirector::SceneUpdateSnapshot &lastUpdateSnapshot(const ::loka::app::scene::Scene &scene)
        {
          return scene.updateCycleState_.lastAppliedSnapshot;
        }

        static const ::loka::app::scene::SceneProjectionTransaction &projectionTransaction(const ::loka::app::scene::Scene &scene)
        {
          return scene.director_.projectionTransaction();
        }

        static long projectionTransactionTargetCount(const ::loka::app::scene::Scene &scene)
        {
          long count = 0;
          const ::loka::app::scene::SceneProjectionTransaction::TargetEntry *entry = scene.director_.projectionTransaction().targetsHead();
          while (entry)
          {
            ++count;
            entry = entry->next;
          }
          return count;
        }

        static unsigned long projectionTransactionGeneration(const ::loka::app::scene::Scene &scene)
        {
          return scene.director_.projectionTransactionGenerationForTesting();
        }

        static ::loka::app::scene::NodeDirtyFlags requestedDirtyFlags(const ::loka::app::scene::Scene &scene)
        {
          return scene.director_.requestedDirtyFlags();
        }

        static ::loka::app::scene::NodeDirtyFlags effectiveRequestedDirtyFlags(const ::loka::app::scene::Scene &scene)
        {
          return scene.director_.effectiveRequestedDirtyFlags();
        }

        static bool hasRequestedInput(const ::loka::app::scene::Scene &scene)
        {
          return scene.director_.hasRequestedInput();
        }

        static bool requestedFullRebuild(const ::loka::app::scene::Scene &scene)
        {
          return scene.director_.requestedFullRebuild();
        }

        static bool flushInvalidation(::loka::app::scene::Scene &scene)
        {
          return scene.flushInvalidation();
        }

        static ::loka::app::scene::IPlatformController *platformController(const ::loka::app::scene::Scene &scene)
        {
          return scene.platformController_;
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
      struct SceneNodeCast< ::loka::app::scene::Node>
      {
        static ::loka::app::scene::Node *cast(::loka::app::scene::Node *node)
        {
          return node;
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

      template <>
      struct SceneNodeCast< ::loka::app::ButtonNode>
      {
        static ::loka::app::ButtonNode *cast(::loka::app::scene::Node *node)
        {
          return node ? node->asButtonNode() : 0;
        }
      };

      namespace scene_test_detail
      {
        inline void appendDirtyFlagName(std::string &out, const char *name)
        {
          if (!out.empty())
          {
            out += '|';
          }
          out += name;
        }

        inline std::string nodeDirtyFlagsToString(::loka::app::scene::NodeDirtyFlags flags)
        {
          if (flags == ::loka::app::scene::NODE_DIRTY_NONE)
          {
            return std::string("NONE");
          }

          std::string result;
          if (flags & ::loka::app::scene::NODE_DIRTY_PROPS)
          {
            appendDirtyFlagName(result, "PROPS");
          }
          if (flags & ::loka::app::scene::NODE_DIRTY_CHILD)
          {
            appendDirtyFlagName(result, "CHILD");
          }
          if (flags & ::loka::app::scene::NODE_DIRTY_LAYOUT)
          {
            appendDirtyFlagName(result, "LAYOUT");
          }
          if (flags & ::loka::app::scene::NODE_DIRTY_INITIAL)
          {
            appendDirtyFlagName(result, "INITIAL");
          }

          const int knownMask = ::loka::app::scene::NODE_DIRTY_PROPS |
                                ::loka::app::scene::NODE_DIRTY_CHILD |
                                ::loka::app::scene::NODE_DIRTY_LAYOUT |
                                ::loka::app::scene::NODE_DIRTY_INITIAL;
          const int unknownBits = static_cast<int>(flags) & ~knownMask;
          if (unknownBits != 0)
          {
            char buf[32];
            ::snprintf(buf, sizeof(buf), "0x%X", unknownBits);
            appendDirtyFlagName(result, buf);
          }
          return result.empty() ? std::string("NONE") : result;
        }

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
          const ::loka::app::scene::NodeDirtyFlags dirtyFlags = node->dirty.get();
          out.setInt("dirty.mask", static_cast<long>(dirtyFlags));
          out.set("dirty.flags", scene_test_detail::nodeDirtyFlagsToString(dirtyFlags).c_str());
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
                           const char *status = SnapStatusOk())
            : testName_(testName ? testName : ""),
              stepName_(stepName ? stepName : ""),
              tick_(tick),
              scenarioVersion_(scenarioVersion),
              status_(status ? status : SnapStatusOk()) {}

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

      class CaptureSceneAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef SnapRecord Out;

        CaptureSceneAdapter(const char *testName,
                            const char *stepName,
                            long tick,
                            long scenarioVersion,
                            const char *status = SnapStatusOk())
            : testName_(testName ? testName : ""),
              stepName_(stepName ? stepName : ""),
              tick_(tick),
              scenarioVersion_(scenarioVersion),
              status_(status ? status : SnapStatusOk()) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          if (!in)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NULL_SCENE;
            return FLOW_STEP_FAILED;
          }

          BuildSnapV1RecordAdapter base(testName_.c_str(),
                                        stepName_.c_str(),
                                        "Scene",
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

          ::loka::app::scene::Node *root = SceneTestAccess::rootNode(*in);
          ::loka::app::scene::BoundaryNode *boundary = SceneTestAccess::rootBoundary(*in);
          out.setInt("scene.root.present", root ? 1 : 0);
          out.setInt("scene.root_boundary.present", boundary ? 1 : 0);
          if (root)
          {
            out.setInt("scene.root.kind", static_cast<long>(root->kind()));
            if (!root->testId().empty())
            {
              out.set("scene.root.test_id", root->testId().c_str());
            }
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

      inline CaptureSceneAdapter CaptureScene(const char *testName,
                                             const char *stepName,
                                             long tick,
                                             long scenarioVersion,
                                             const char *status)
      {
        return CaptureSceneAdapter(testName, stepName, tick, scenarioVersion, status);
      }

      inline CaptureSceneAdapter CaptureScene(const char *testName,
                                             const char *stepName,
                                             long tick,
                                             long scenarioVersion)
      {
        return CaptureSceneAdapter(testName, stepName, tick, scenarioVersion);
      }

      struct ViewCaptureTarget
      {
        ViewCaptureTarget()
            : node(0),
              hasContext(false),
              capturableBitmap(0),
              kind(0),
              testId() {}

        ::loka::app::scene::Node *node;
        bool hasContext;
        const ::loka::app::scene::ICapturableBitmap *capturableBitmap;
        long kind;
        std::string testId;
      };

      class CaptureViewTargetAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ViewCaptureTarget Out;

        explicit CaptureViewTargetAdapter(const char *testId)
            : testId_(testId ? testId : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          ::loka::app::scene::Node *node = 0;
          StepRunStatus lookupStatus = LookupNodeById< ::loka::app::scene::Node>(in, testId_, node, error);
          if (lookupStatus != FLOW_STEP_SUCCEEDED)
          {
            return lookupStatus;
          }
          out.node = node;
          out.hasContext = node && node->getContext() != 0;
          out.capturableBitmap = (node && node->getContext()) ? node->getContext()->asCapturableBitmap() : 0;
          out.kind = node ? static_cast<long>(node->kind()) : 0;
          out.testId = node ? node->testId() : std::string();
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string testId_;
      };

      inline CaptureViewTargetAdapter CaptureViewTarget(const char *testId)
      {
        return CaptureViewTargetAdapter(testId);
      }

      class CaptureViewTargetSnapAdapter
      {
      public:
        typedef ViewCaptureTarget In;
        typedef SnapRecord Out;

        CaptureViewTargetSnapAdapter(const char *testName,
                                     const char *stepName,
                                     long tick,
                                     long scenarioVersion,
                                     const char *status = SnapStatusOk())
            : testName_(testName ? testName : ""),
              stepName_(stepName ? stepName : ""),
              tick_(tick),
              scenarioVersion_(scenarioVersion),
              status_(status ? status : SnapStatusOk()) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          if (!in.node)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND;
            return FLOW_STEP_FAILED;
          }

          BuildSnapV1RecordAdapter base(testName_.c_str(),
                                        stepName_.c_str(),
                                        in.testId.empty() ? "ViewTarget" : in.testId.c_str(),
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

          out.setInt("view.target.present", 1);
          out.setInt("view.context.present", in.hasContext ? 1 : 0);
          out.setInt("view.node.kind", in.kind);
          if (!in.testId.empty())
          {
            out.set("view.node.test_id", in.testId.c_str());
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

      inline CaptureViewTargetSnapAdapter CaptureView(const char *testName,
                                                      const char *stepName,
                                                      long tick,
                                                      long scenarioVersion,
                                                      const char *status)
      {
        return CaptureViewTargetSnapAdapter(testName, stepName, tick, scenarioVersion, status);
      }

      inline CaptureViewTargetSnapAdapter CaptureView(const char *testName,
                                                      const char *stepName,
                                                      long tick,
                                                      long scenarioVersion)
      {
        return CaptureViewTargetSnapAdapter(testName, stepName, tick, scenarioVersion);
      }

      struct ViewBitmapCapture
      {
        ViewBitmapCapture()
            : image(::loka::core::resource::Image::Empty()),
              captured(false),
              width(0),
              height(0) {}

        ::loka::core::resource::Image image;
        bool captured;
        int width;
        int height;
      };

      class CaptureViewBitmapAdapter
      {
      public:
        typedef ViewCaptureTarget In;
        typedef ViewBitmapCapture Out;
        typedef bool (*CaptureFn)(const ViewCaptureTarget &target,
                                  ViewBitmapCapture &out,
                                  FlowError &error,
                                  void *user);

        CaptureViewBitmapAdapter(CaptureFn captureFn, void *user)
            : captureFn_(captureFn),
              user_(user) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          if (!in.node)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND;
            return FLOW_STEP_FAILED;
          }
          if (!this->captureFn_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          if (!this->captureFn_(in, out, error, this->user_))
          {
            if (error.kind == 0 && error.code == 0)
            {
              error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
              error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            }
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        CaptureFn captureFn_;
        void *user_;
      };

      inline CaptureViewBitmapAdapter CaptureViewBitmap(CaptureViewBitmapAdapter::CaptureFn captureFn,
                                                        void *user)
      {
        return CaptureViewBitmapAdapter(captureFn, user);
      }

      class CaptureViewBitmapFromPlatformAdapter
      {
      public:
        typedef ViewCaptureTarget In;
        typedef ViewBitmapCapture Out;

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          if (!in.node)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND;
            return FLOW_STEP_FAILED;
          }
          if (!in.capturableBitmap)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          if (!in.capturableBitmap->captureBitmap(out.image))
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          out.captured = out.image.isValid();
          out.width = out.image.width();
          out.height = out.image.height();
          return FLOW_STEP_SUCCEEDED;
        }
      };

      inline CaptureViewBitmapFromPlatformAdapter CaptureViewBitmapFromPlatform()
      {
        return CaptureViewBitmapFromPlatformAdapter();
      }

      class CaptureViewBitmapSnapAdapter
      {
      public:
        typedef ViewBitmapCapture In;
        typedef SnapRecord Out;

        CaptureViewBitmapSnapAdapter(const char *testName,
                                     const char *stepName,
                                     long tick,
                                     long scenarioVersion,
                                     const char *status = SnapStatusOk())
            : testName_(testName ? testName : ""),
              stepName_(stepName ? stepName : ""),
              tick_(tick),
              scenarioVersion_(scenarioVersion),
              status_(status ? status : SnapStatusOk()) {}

        StepRunStatus run(In const &in, Out &out, FlowError &) const
        {
          BuildSnapV1RecordAdapter base(this->testName_.c_str(),
                                        this->stepName_.c_str(),
                                        "ViewBitmap",
                                        this->tick_,
                                        this->scenarioVersion_,
                                        this->status_.c_str());
          const int unused = 0;
          FlowError ignored;
          if (base.run(unused, out, ignored) != FLOW_STEP_SUCCEEDED)
          {
            return FLOW_STEP_FAILED;
          }
          out.setInt("view.bitmap.captured", in.captured ? 1 : 0);
          out.setInt("view.bitmap.image.valid", in.image.isValid() ? 1 : 0);
          out.setInt("view.bitmap.width", in.width);
          out.setInt("view.bitmap.height", in.height);
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string testName_;
        std::string stepName_;
        long tick_;
        long scenarioVersion_;
        std::string status_;
      };

      inline CaptureViewBitmapSnapAdapter CaptureViewBitmapSnap(const char *testName,
                                                                const char *stepName,
                                                                long tick,
                                                                long scenarioVersion,
                                                                const char *status)
      {
        return CaptureViewBitmapSnapAdapter(testName, stepName, tick, scenarioVersion, status);
      }

      inline CaptureViewBitmapSnapAdapter CaptureViewBitmapSnap(const char *testName,
                                                                const char *stepName,
                                                                long tick,
                                                                long scenarioVersion)
      {
        return CaptureViewBitmapSnapAdapter(testName, stepName, tick, scenarioVersion);
      }

      class SnapTextAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef SnapRecord Out;

        SnapTextAdapter(const char *testId,
                        const char *testName,
                        const char *stepName,
                        long tick,
                        long scenarioVersion,
                        const char *status = SnapStatusOk())
            : testId_(testId ? testId : ""),
              testName_(testName ? testName : ""),
              stepName_(stepName ? stepName : ""),
              tick_(tick),
              scenarioVersion_(scenarioVersion),
              status_(status ? status : SnapStatusOk()) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          ::loka::app::TextNode *node = 0;
          StepRunStatus lookupStatus = LookupNodeById< ::loka::app::TextNode>(in, testId_, node, error);
          if (lookupStatus != FLOW_STEP_SUCCEEDED)
          {
            return lookupStatus;
          }
          return CaptureNode< ::loka::app::TextNode>(testName_.c_str(),
                                                     stepName_.c_str(),
                                                     tick_,
                                                     scenarioVersion_,
                                                     status_.c_str())
              .run(node, out, error);
        }

      private:
        std::string testId_;
        std::string testName_;
        std::string stepName_;
        long tick_;
        long scenarioVersion_;
        std::string status_;
      };

      inline SnapTextAdapter SnapText(const char *testId,
                                      const char *testName,
                                      const char *stepName,
                                      long tick,
                                      long scenarioVersion,
                                      const char *status)
      {
        return SnapTextAdapter(testId, testName, stepName, tick, scenarioVersion, status);
      }

      inline SnapTextAdapter SnapText(const char *testId,
                                      const char *testName,
                                      const char *stepName,
                                      long tick,
                                      long scenarioVersion)
      {
        return SnapTextAdapter(testId, testName, stepName, tick, scenarioVersion);
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

      class AssertSnapIntEqualsAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        AssertSnapIntEqualsAdapter(const char *key, long expectedValue)
            : key_(key ? key : ""),
              expectedValue_(expectedValue) {}

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
          if (actual != expectedValue_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string key_;
        long expectedValue_;
      };

      inline AssertSnapIntEqualsAdapter AssertSnapIntEquals(const char *key, long expectedValue)
      {
        return AssertSnapIntEqualsAdapter(key, expectedValue);
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

      class AssertSnapIntMaskHasBitsAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        AssertSnapIntMaskHasBitsAdapter(const char *key, long mask)
            : key_(key ? key : ""),
              mask_(mask) {}

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
          if ((actual & mask_) != mask_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return FLOW_STEP_FAILED;
          }
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string key_;
        long mask_;
      };

      inline AssertSnapIntMaskHasBitsAdapter AssertSnapIntMaskHasBits(const char *key, long mask)
      {
        return AssertSnapIntMaskHasBitsAdapter(key, mask);
      }

      class CheckDirtyHasBitsAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        explicit CheckDirtyHasBitsAdapter(long mask)
            : mask_(mask) {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          return AssertSnapIntMaskHasBits("dirty.mask", mask_).run(in, out, error);
        }

      private:
        long mask_;
      };

      inline CheckDirtyHasBitsAdapter CheckDirtyHasBits(long mask)
      {
        return CheckDirtyHasBitsAdapter(mask);
      }

      class CheckDirtyEqualsAdapter
      {
      public:
        typedef SnapRecord In;
        typedef SnapRecord Out;

        explicit CheckDirtyEqualsAdapter(long expectedMask)
            : expectedMask_(expectedMask) {}

        StepRunStatus run(const In &in, Out &out, FlowError &error) const
        {
          out = in;
          return AssertSnapIntEquals("dirty.mask", expectedMask_).run(in, out, error);
        }

      private:
        long expectedMask_;
      };

      inline CheckDirtyEqualsAdapter CheckDirtyEquals(long expectedMask)
      {
        return CheckDirtyEqualsAdapter(expectedMask);
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

      class CheckTextDirtyHasBitsAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        CheckTextDirtyHasBitsAdapter(const char *testId, long mask)
            : testId_(testId ? testId : ""),
              mask_(mask) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          // Internal snap only: used as a transient assertion payload, not for file output.
          SnapRecord snap;
          StepRunStatus snapStatus = SnapText(testId_.c_str(), "SceneCheck", "check-text-dirty", 0, 0).run(in, snap, error);
          if (snapStatus != FLOW_STEP_SUCCEEDED)
          {
            return snapStatus;
          }
          SnapRecord ignored;
          return CheckDirtyHasBits(mask_).run(snap, ignored, error);
        }

      private:
        std::string testId_;
        long mask_;
      };

      inline CheckTextDirtyHasBitsAdapter CheckTextDirtyHasBits(const char *testId, long mask)
      {
        return CheckTextDirtyHasBitsAdapter(testId, mask);
      }

      class CheckTextDirtyEqualsAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        CheckTextDirtyEqualsAdapter(const char *testId, long expectedMask)
            : testId_(testId ? testId : ""),
              expectedMask_(expectedMask) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          // Internal snap only: used as a transient assertion payload, not for file output.
          SnapRecord snap;
          StepRunStatus snapStatus = SnapText(testId_.c_str(), "SceneCheck", "check-text-dirty", 0, 0).run(in, snap, error);
          if (snapStatus != FLOW_STEP_SUCCEEDED)
          {
            return snapStatus;
          }
          SnapRecord ignored;
          return CheckDirtyEquals(expectedMask_).run(snap, ignored, error);
        }

      private:
        std::string testId_;
        long expectedMask_;
      };

      inline CheckTextDirtyEqualsAdapter CheckTextDirtyEquals(const char *testId, long expectedMask)
      {
        return CheckTextDirtyEqualsAdapter(testId, expectedMask);
      }

      class FlushSceneInvalidationAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          if (!in)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NULL_SCENE;
            return FLOW_STEP_FAILED;
          }
          SceneTestAccess::flushInvalidation(*in);
          return FLOW_STEP_SUCCEEDED;
        }
      };

      inline FlushSceneInvalidationAdapter FlushSceneInvalidation()
      {
        return FlushSceneInvalidationAdapter();
      }

      class SetStringStateAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        SetStringStateAdapter(::loka::core::MutableState< ::loka::core::String> *state, const char *value)
            : state_(state),
              value_(value ? value : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          if (!in || !state_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NULL_SCENE;
            return FLOW_STEP_FAILED;
          }
          ::loka::app::scene::BoundaryNode *boundary = SceneTestAccess::rootBoundary(*in);
          if (!boundary || !boundary->tracker())
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_ROOT_UNAVAILABLE;
            return FLOW_STEP_FAILED;
          }
          ::loka::core::StateTrackerGuard guard(boundary->tracker());
          state_->set(::loka::core::String::Literal(value_.c_str()));
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        ::loka::core::MutableState< ::loka::core::String> *state_;
        std::string value_;
      };

      inline SetStringStateAdapter SetStringState(::loka::core::MutableState< ::loka::core::String> *state,
                                                  const char *value)
      {
        return SetStringStateAdapter(state, value);
      }

      class SetStringStateAndFlushAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        SetStringStateAndFlushAdapter(::loka::core::MutableState< ::loka::core::String> *state, const char *value)
            : state_(state),
              value_(value ? value : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          StepRunStatus updateStatus = SetStringState(state_, value_.c_str()).run(in, out, error);
          if (updateStatus != FLOW_STEP_SUCCEEDED)
          {
            return updateStatus;
          }
          return FlushSceneInvalidation().run(out, out, error);
        }

      private:
        ::loka::core::MutableState< ::loka::core::String> *state_;
        std::string value_;
      };

      inline SetStringStateAndFlushAdapter SetStringStateAndFlush(::loka::core::MutableState< ::loka::core::String> *state,
                                                                  const char *value)
      {
        return SetStringStateAndFlushAdapter(state, value);
      }

      class SetBoolStateAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        SetBoolStateAdapter(::loka::core::MutableState<bool> *state, bool value)
            : state_(state),
              value_(value) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          if (!in || !state_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NULL_SCENE;
            return FLOW_STEP_FAILED;
          }
          ::loka::app::scene::BoundaryNode *boundary = SceneTestAccess::rootBoundary(*in);
          if (!boundary || !boundary->tracker())
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_ROOT_UNAVAILABLE;
            return FLOW_STEP_FAILED;
          }
          ::loka::core::StateTrackerGuard guard(boundary->tracker());
          state_->set(value_);
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        ::loka::core::MutableState<bool> *state_;
        bool value_;
      };

      inline SetBoolStateAdapter SetBoolState(::loka::core::MutableState<bool> *state, bool value)
      {
        return SetBoolStateAdapter(state, value);
      }

      class SetBoolStateAndFlushAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        SetBoolStateAndFlushAdapter(::loka::core::MutableState<bool> *state, bool value)
            : state_(state),
              value_(value) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          StepRunStatus updateStatus = SetBoolState(state_, value_).run(in, out, error);
          if (updateStatus != FLOW_STEP_SUCCEEDED)
          {
            return updateStatus;
          }
          return FlushSceneInvalidation().run(out, out, error);
        }

      private:
        ::loka::core::MutableState<bool> *state_;
        bool value_;
      };

      inline SetBoolStateAndFlushAdapter SetBoolStateAndFlush(::loka::core::MutableState<bool> *state, bool value)
      {
        return SetBoolStateAndFlushAdapter(state, value);
      }

      class SetIntStateAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        SetIntStateAdapter(::loka::core::MutableState<int> *state, int value)
            : state_(state),
              value_(value) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          if (!in || !state_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NULL_SCENE;
            return FLOW_STEP_FAILED;
          }
          ::loka::app::scene::BoundaryNode *boundary = SceneTestAccess::rootBoundary(*in);
          if (!boundary || !boundary->tracker())
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_ROOT_UNAVAILABLE;
            return FLOW_STEP_FAILED;
          }
          ::loka::core::StateTrackerGuard guard(boundary->tracker());
          state_->set(value_);
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        ::loka::core::MutableState<int> *state_;
        int value_;
      };

      inline SetIntStateAdapter SetIntState(::loka::core::MutableState<int> *state, int value)
      {
        return SetIntStateAdapter(state, value);
      }

      class SetIntStateAndFlushAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        SetIntStateAndFlushAdapter(::loka::core::MutableState<int> *state, int value)
            : state_(state),
              value_(value) {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          StepRunStatus updateStatus = SetIntState(state_, value_).run(in, out, error);
          if (updateStatus != FLOW_STEP_SUCCEEDED)
          {
            return updateStatus;
          }
          return FlushSceneInvalidation().run(out, out, error);
        }

      private:
        ::loka::core::MutableState<int> *state_;
        int value_;
      };

      inline SetIntStateAndFlushAdapter SetIntStateAndFlush(::loka::core::MutableState<int> *state, int value)
      {
        return SetIntStateAndFlushAdapter(state, value);
      }

      class ClickButtonByIdAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        explicit ClickButtonByIdAdapter(const char *testId)
            : testId_(testId ? testId : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          if (!in)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_NULL_SCENE;
            return FLOW_STEP_FAILED;
          }
          ::loka::app::ButtonNode *button = 0;
          StepRunStatus lookupStatus = LookupNodeById< ::loka::app::ButtonNode>(in, testId_, button, error);
          if (lookupStatus != FLOW_STEP_SUCCEEDED)
          {
            return lookupStatus;
          }
          ::loka::app::scene::BoundaryNode *boundary = SceneTestAccess::rootBoundary(*in);
          if (!boundary || !boundary->tracker())
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_ROOT_UNAVAILABLE;
            return FLOW_STEP_FAILED;
          }
          if (button->props.enabled_ && !button->props.enabled_->get())
          {
            return FLOW_STEP_SUCCEEDED;
          }
          if (!button->props.onClick_)
          {
            error.kind = FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return FLOW_STEP_FAILED;
          }
          ::loka::core::StateTrackerGuard guard(boundary->tracker());
          button->props.onClick_->emit();
          return FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string testId_;
      };

      inline ClickButtonByIdAdapter ClickButtonById(const char *testId)
      {
        return ClickButtonByIdAdapter(testId);
      }

      class ClickButtonByIdAndFlushAdapter
      {
      public:
        typedef ::loka::app::scene::Scene *In;
        typedef ::loka::app::scene::Scene *Out;

        explicit ClickButtonByIdAndFlushAdapter(const char *testId)
            : testId_(testId ? testId : "") {}

        StepRunStatus run(In const &in, Out &out, FlowError &error) const
        {
          out = in;
          StepRunStatus clickStatus = ClickButtonById(testId_.c_str()).run(in, out, error);
          if (clickStatus != FLOW_STEP_SUCCEEDED)
          {
            return clickStatus;
          }
          return FlushSceneInvalidation().run(out, out, error);
        }

      private:
        std::string testId_;
      };

      inline ClickButtonByIdAndFlushAdapter ClickButtonByIdAndFlush(const char *testId)
      {
        return ClickButtonByIdAndFlushAdapter(testId);
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
