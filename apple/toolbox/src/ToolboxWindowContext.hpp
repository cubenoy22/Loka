#ifndef LOKA_TOOLBOX_WINDOW_CONTEXT_HPP
#define LOKA_TOOLBOX_WINDOW_CONTEXT_HPP

class ToolboxNodeContextMapper;

class ToolboxWindowContext
{
public:
  explicit ToolboxWindowContext(int capabilities);
  ~ToolboxWindowContext();

  ToolboxNodeContextMapper *contextMapper() const
  {
    return contextMapper_;
  }

private:
  ToolboxNodeContextMapper *contextMapper_;
};

#endif // LOKA_TOOLBOX_WINDOW_CONTEXT_HPP
