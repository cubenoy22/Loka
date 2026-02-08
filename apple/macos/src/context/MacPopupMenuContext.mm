#include "MacPopupMenuContext.hpp"
#include "Utf8String.hpp"
#include <AppKit/AppKit.h>
#include "loka/platform/StringUTF8.hpp"

@interface LokaPopupMenuTarget : NSObject
@property(nonatomic, assign) MacPopupMenuContext *owner;
- (IBAction)popupChanged:(id)sender;
@end

@implementation LokaPopupMenuTarget
- (IBAction)popupChanged:(id)sender
{
  (void)sender;
  if (self.owner)
  {
    self.owner->handleSelectionChange();
  }
}
@end

MacPopupMenuContext::MacPopupMenuContext(void *parentView, int x, int y, int width, int height, loka::app::PopupMenuNode *node)
    : node_(node), popup_(0), target_(0), selectionState_(0), enabledState_(0),
      applyingFromState_(false), updatingFromControl_(false)
{
  NSView *parent = (NSView *)parentView;
  NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y, width, height) pullsDown:NO];

  LokaPopupMenuTarget *target = [[LokaPopupMenuTarget alloc] init];
  target.owner = this;
  [popup setTarget:target];
  [popup setAction:@selector(popupChanged:)];

  if (parent)
  {
    [parent addSubview:popup];
  }

  popup_ = (void *)popup;
  target_ = (void *)target;

  applyItems();
  bindSelection();
  bindEnabled();
}

MacPopupMenuContext::~MacPopupMenuContext()
{
  unbindSelection();
  unbindEnabled();
  NSPopUpButton *popup = (NSPopUpButton *)popup_;
  if (popup)
  {
    [popup setTarget:nil];
    [popup setAction:nil];
    [popup removeFromSuperview];
  }
  if (target_)
  {
    [(id)target_ release];
    target_ = 0;
  }
  if (popup_)
  {
    [(id)popup_ release];
  }
  popup_ = 0;
}

void MacPopupMenuContext::handleSelectionChange()
{
  if (!applyingFromState_)
  {
    syncStateFromControl();
  }
}

void MacPopupMenuContext::bindSelection()
{
  if (!node_)
  {
    return;
  }
  selectionState_ = static_cast<loka::core::State<int> *>(node_->props.selectedIndex_);
  if (selectionState_)
  {
    selectionState_->deferBind(&MacPopupMenuContext::SelectionChangedThunk, this);
    applySelection();
  }
}

void MacPopupMenuContext::unbindSelection()
{
  if (selectionState_)
  {
    selectionState_->deferUnbind(&MacPopupMenuContext::SelectionChangedThunk, this);
    selectionState_ = 0;
  }
}

void MacPopupMenuContext::bindEnabled()
{
  if (!node_)
  {
    return;
  }
  enabledState_ = static_cast<loka::core::State<bool> *>(node_->props.enabled_);
  if (enabledState_)
  {
    enabledState_->deferBind(&MacPopupMenuContext::EnabledChangedThunk, this);
    applyEnabled();
  }
}

void MacPopupMenuContext::unbindEnabled()
{
  if (enabledState_)
  {
    enabledState_->deferUnbind(&MacPopupMenuContext::EnabledChangedThunk, this);
    enabledState_ = 0;
  }
}

void MacPopupMenuContext::applyItems()
{
  NSPopUpButton *popup = (NSPopUpButton *)popup_;
  if (!popup || !node_)
  {
    return;
  }
  [popup removeAllItems];
  const loka::Vector<loka::core::String> *items = node_->props.items_;
  if (!items)
  {
    return;
  }
  for (std::size_t i = 0; i < items->size(); ++i)
  {
    std::string utf8;
    if (loka::platform::CollectUtf8((*items)[i], utf8))
    {
      [popup addItemWithTitle:loka::macos::CreateNSStringFromUtf8(utf8)];
    }
  }
}

void MacPopupMenuContext::applySelection()
{
  NSPopUpButton *popup = (NSPopUpButton *)popup_;
  if (!popup || !selectionState_)
  {
    return;
  }
  NSInteger itemCount = [popup numberOfItems];
  if (itemCount <= 0)
  {
    return;
  }
  int index = selectionState_->get();
  if (index < 0)
  {
    index = 0;
  }
  if (index >= itemCount)
  {
    index = static_cast<int>(itemCount) - 1;
  }
  applyingFromState_ = true;
  [popup selectItemAtIndex:index];
  applyingFromState_ = false;
}

void MacPopupMenuContext::applyEnabled()
{
  NSPopUpButton *popup = (NSPopUpButton *)popup_;
  if (!popup || !enabledState_)
  {
    return;
  }
  [popup setEnabled:enabledState_->get() ? YES : NO];
}

void MacPopupMenuContext::syncStateFromControl()
{
  NSPopUpButton *popup = (NSPopUpButton *)popup_;
  if (!popup || !selectionState_)
  {
    return;
  }
  loka::core::MutableState<int> *mutableState = dynamic_cast<loka::core::MutableState<int> *>(selectionState_);
  if (!mutableState)
  {
    return;
  }
  updatingFromControl_ = true;
  NSInteger index = [popup indexOfSelectedItem];
  mutableState->set(static_cast<int>(index), true);
  updatingFromControl_ = false;
  if (node_ && node_->props.onChange_)
  {
    node_->props.onChange_->emit();
  }
}

void MacPopupMenuContext::SelectionChangedThunk(void *userData)
{
  MacPopupMenuContext *self = static_cast<MacPopupMenuContext *>(userData);
  if (self)
  {
    self->applySelection();
  }
}

void MacPopupMenuContext::EnabledChangedThunk(void *userData)
{
  MacPopupMenuContext *self = static_cast<MacPopupMenuContext *>(userData);
  if (self)
  {
    self->applyEnabled();
  }
}
