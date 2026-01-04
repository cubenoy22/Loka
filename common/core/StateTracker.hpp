#ifndef LOKA_STATETRACKER_HPP
#define LOKA_STATETRACKER_HPP

#include <vector>
#include <functional>
#include <map>
#include <string>
#include <set>

namespace declara
{
  namespace core
  {

    class StateBase;

    // StateTrackerのPhase定義
    enum TrackerPhase
    {
      TRACKER_IDLE = 0,
      TRACKER_PRECOMMIT = 1,
      TRACKER_COMMIT = 2
    };

    class StateTracker
    {
    public:
      virtual void begin() = 0;
      virtual void defer(void (*fn)(void *), void *userData) = 0;
      virtual void markDirty(StateBase *state) = 0;
      virtual bool end() = 0;
      virtual void registerDependency(StateBase *dependent, StateBase *dependency) = 0;
      virtual TrackerPhase phase() const = 0;
      virtual ~StateTracker() {}
    };

    class PushStateTracker : public StateTracker
    {
    public:
      PushStateTracker(const std::vector<StateBase *> &states);
      PushStateTracker();
      void begin();
      void defer(void (*fn)(void *), void *userData);
      void markDirty(StateBase *state);
      void addState(StateBase *state);
      bool end();
      /**
       * @brief 依存グラフ（依存元→依存先）を構築する。通常はDerivedStateの依存関係から自動生成される。
       */
      void registerDependency(StateBase *dependent, StateBase *dependency);
      TrackerPhase phase() const;
      ~PushStateTracker();

    private:
      // Typedefs to avoid nested template closers in C++98
      typedef std::vector<StateBase *> StateList;
      typedef std::pair<void (*)(void *), void *> DeferredEntry;
      typedef std::map<StateBase *, StateList> DependencyMap;
      /// dirtyStates: トランザクション中にdirtyなStateを管理（伝播バッファ）
      StateList dirtyStates;
      /// deferred: commit時に一括発火する副作用コールバック
      std::vector<DeferredEntry> deferred;
      /// dependents: 依存グラフ（依存元→依存先リスト）。registerDependencyで構築され、依存伝播の起点となる。
      DependencyMap dependents;
      /// phase_: Trackerの現在のトランザクションフェーズ
      TrackerPhase phase_;
      /// visiting_: 再帰伝播時の循環依存検出用一時セット
      std::set<StateBase *> visiting_;
      /// states_: Trackerが管理する全State群（begin()/end()でcurrentTrackerをセット）
      StateList states_;
    };

  } // namespace core
} // namespace declara

using declara::core::StateBase;

#endif // LOKA_STATETRACKER_HPP
