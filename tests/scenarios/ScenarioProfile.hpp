#ifndef LOKA_TESTS_SCENARIOS_SCENARIO_PROFILE_HPP
#define LOKA_TESTS_SCENARIOS_SCENARIO_PROFILE_HPP

#include <string>

namespace loka
{
  namespace scenario_tests
  {
    /** A profile fact whose availability cannot drift from its value. */
    template <typename T> class ProfileFact
    {
    public:
      static ProfileFact unavailable()
      {
        return ProfileFact(false, T());
      }

      static ProfileFact available(const T &value)
      {
        return ProfileFact(true, value);
      }

      bool query(T &out) const
      {
        if (!this->available_)
        {
          return false;
        }
        out = this->value_;
        return true;
      }

    private:
      ProfileFact(bool available, const T &value)
          : available_(available),
            value_(value)
      {
      }

      const bool available_;
      const T value_;
    };

    /** Immutable environment facts recorded beside one settled scenario capture. */
    class ScenarioProfile
    {
    public:
      ScenarioProfile(const std::string &osBuild,
                      const std::string &architecture,
                      const ProfileFact<int> &scalePercent,
                      const ProfileFact<int> &depth,
                      const ProfileFact<std::string> &appearance,
                      const std::string &captureApi,
                      long pixelWidth,
                      long pixelHeight);

      std::string render() const;

    private:
      ScenarioProfile &operator=(const ScenarioProfile &);

      const std::string osBuild_;
      const std::string architecture_;
      const ProfileFact<int> scalePercent_;
      const ProfileFact<int> depth_;
      const ProfileFact<std::string> appearance_;
      const std::string captureApi_;
      const long pixelWidth_;
      const long pixelHeight_;
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_SCENARIO_PROFILE_HPP
