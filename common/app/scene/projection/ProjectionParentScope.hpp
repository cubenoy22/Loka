#ifndef LOKA_APP_SCENE_PROJECTION_PROJECTION_PARENT_SCOPE_HPP
#define LOKA_APP_SCENE_PROJECTION_PROJECTION_PARENT_SCOPE_HPP

#include <cassert>
#include <climits>

#include "app/scene/Node.hpp"
#include "core/Frame.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** One platform-projection parent frame. Native parent identity is
          deliberately opaque at the shared wall; each rail owns the cast. */
      struct ProjectionParentScope
      {
        ProjectionParentScope()
            : nativeParent(0),
              translationX(0),
              translationY(0),
              clipRect(0, 0, 0, 0),
              contentHeight_(0)
        {
        }

        ProjectionParentScope(void *parent,
                              int translateX,
                              int translateY,
                              const loka::core::Frame &clip)
            : nativeParent(parent),
              translationX(translateX),
              translationY(translateY),
              clipRect(clip),
              contentHeight_(0)
        {
        }

        /** Derives a child-parent frame whose coordinates project as
            content - offset. Returns false instead of overflowing int. */
        bool deriveScrolled(void *parent,
                            int offsetX,
                            int offsetY,
                            const loka::core::Frame &clip,
                            ProjectionParentScope &out) const
        {
          int childTranslationX = 0;
          int childTranslationY = 0;
          if (!trySubtract(this->translationX, offsetX, childTranslationX) ||
              !trySubtract(this->translationY, offsetY, childTranslationY))
          {
            return false;
          }
          out = ProjectionParentScope(parent, childTranslationX, childTranslationY, clip);
          return true;
        }

        /** Applies this frame's translation at the projection leaf. */
        bool project(const LayoutState &content, LayoutState &projected) const
        {
          int projectedX = 0;
          int projectedY = 0;
          if (!tryAdd(content.x, this->translationX, projectedX) ||
              !tryAdd(content.y, this->translationY, projectedY) ||
              projectedX < SHRT_MIN || projectedX > SHRT_MAX ||
              projectedY < SHRT_MIN || projectedY > SHRT_MAX)
          {
            return false;
          }
          projected = content;
          projected.x = static_cast<short>(projectedX);
          projected.y = static_cast<short>(projectedY);
          return true;
        }

        /** Restores a projected leaf result to content coordinates. */
        bool restoreContentY(int projectedY, int &contentY) const
        {
          return trySubtract(projectedY, this->translationY, contentY);
        }

        /** Extends the explicit int measurement channel without ever
            assigning an out-of-range value to LayoutState. */
        bool tryAccumulateContentHeight(int childStartY, int childEndY)
        {
          if (childEndY < childStartY)
          {
            return false;
          }
          const int remaining = SHRT_MAX - this->contentHeight_;
          if (childStartY > INT_MAX - remaining ||
              childEndY > childStartY + remaining)
          {
            return false;
          }
          this->contentHeight_ += childEndY - childStartY;
          return true;
        }

        int contentHeight() const
        {
          return this->contentHeight_;
        }

        void *nativeParent;
        int translationX;
        int translationY;
        loka::core::Frame clipRect;

      private:
        int contentHeight_;

        static bool tryAdd(int lhs, int rhs, int &out)
        {
          if ((rhs > 0 && lhs > INT_MAX - rhs) ||
              (rhs < 0 && lhs < INT_MIN - rhs))
          {
            return false;
          }
          out = lhs + rhs;
          return true;
        }

        static bool trySubtract(int lhs, int rhs, int &out)
        {
          if ((rhs > 0 && lhs < INT_MIN + rhs) ||
              (rhs < 0 && lhs > INT_MAX + rhs))
          {
            return false;
          }
          out = lhs - rhs;
          return true;
        }
      };

      /** Allocation-free LIFO stack. V1 admits the root frame and one active
          ScrollView frame; a second push is an always-on refusal. */
      class ProjectionParentScopeStack
      {
      public:
        enum
        {
          MAX_ACTIVE_SCOPES = 1
        };

        explicit ProjectionParentScopeStack(void *rootParent = 0)
            : activeDepth_(0)
        {
          this->frames_[0] = ProjectionParentScope(
              rootParent, 0, 0, loka::core::Frame(0, 0, 0, 0));
        }

        ~ProjectionParentScopeStack()
        {
          assert(this->activeDepth_ == 0 &&
                 "projection-parent scopes must be balanced before teardown");
        }

        bool resetRoot(void *rootParent, const loka::core::Frame &clip)
        {
          if (this->activeDepth_ != 0)
          {
            assert(false && "the projection root cannot change inside a child scope");
            return false;
          }
          this->frames_[0] = ProjectionParentScope(rootParent, 0, 0, clip);
          return true;
        }

        bool push(const ProjectionParentScope &scope)
        {
          if (this->activeDepth_ >= MAX_ACTIVE_SCOPES)
          {
            return false;
          }
          ++this->activeDepth_;
          this->frames_[this->activeDepth_] = scope;
          return true;
        }

        bool pop()
        {
          if (this->activeDepth_ == 0)
          {
            assert(false && "projection-parent scope pop must be LIFO");
            return false;
          }
          --this->activeDepth_;
          return true;
        }

        ProjectionParentScope &current()
        {
          return this->frames_[this->activeDepth_];
        }

        const ProjectionParentScope &current() const
        {
          return this->frames_[this->activeDepth_];
        }

        unsigned activeDepth() const
        {
          return this->activeDepth_;
        }

      private:
        ProjectionParentScope frames_[MAX_ACTIVE_SCOPES + 1];
        unsigned activeDepth_;

        ProjectionParentScopeStack(const ProjectionParentScopeStack &);
        ProjectionParentScopeStack &operator=(const ProjectionParentScopeStack &);
      };

      /** Pairs one successful push with its pop on every return path. */
      class ProjectionParentScopeGuard
      {
      public:
        ProjectionParentScopeGuard(ProjectionParentScopeStack &stack,
                                   const ProjectionParentScope &scope)
            : stack_(stack.push(scope) ? &stack : 0)
        {
        }

        ~ProjectionParentScopeGuard()
        {
          if (this->stack_)
          {
            const bool popped = this->stack_->pop();
            (void)popped;
            assert(popped && "a projection-parent guard must pop its own frame");
          }
        }

        bool isActive() const
        {
          return this->stack_ != 0;
        }

      private:
        ProjectionParentScopeStack *stack_;

        ProjectionParentScopeGuard(const ProjectionParentScopeGuard &);
        ProjectionParentScopeGuard &operator=(const ProjectionParentScopeGuard &);
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_APP_SCENE_PROJECTION_PROJECTION_PARENT_SCOPE_HPP
