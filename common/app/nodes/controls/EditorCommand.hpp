#ifndef LOKA_APP_EDITOR_COMMAND_HPP
#define LOKA_APP_EDITOR_COMMAND_HPP
namespace loka
{
  namespace app
  {
    /** An accumulating editor verb. None denotes an empty request slot. */
    class EditorCommand
    {
    public:
      enum Kind
      {
        NONE,
        PAGE_UP,
        PAGE_DOWN
      };
      EditorCommand()
          : kind_(NONE)
      {
      }
      explicit EditorCommand(Kind kind)
          : kind_(kind)
      {
      }
      static EditorCommand None()
      {
        return EditorCommand();
      }
      bool isNone() const
      {
        return this->kind_ == NONE;
      }
      Kind kind() const
      {
        return this->kind_;
      }
      bool operator!=(const EditorCommand &other) const
      {
        return this->kind_ != other.kind_;
      }

    private:
      Kind kind_;
    };
  } // namespace app
} // namespace loka
#endif
