#ifndef LOKA_TOOLBOX_WINDOW_CONTEXT_HPP
#define LOKA_TOOLBOX_WINDOW_CONTEXT_HPP

class ToolboxWindowContext
{
public:
  enum Capability
  {
    CAP_NONE = 0,
    CAP_CONTROL_MANAGER = 1 << 0,
    CAP_TEXT_EDIT = 1 << 1
  };

  explicit ToolboxWindowContext(int capabilities);
  ~ToolboxWindowContext();

  int capabilities() const
  {
    return capabilities_;
  }
  bool hasCapability(Capability cap) const
  {
    return (capabilities_ & cap) != 0;
  }

private:
  int capabilities_;
};

#endif // LOKA_TOOLBOX_WINDOW_CONTEXT_HPP
