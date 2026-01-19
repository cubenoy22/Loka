#include "Win32OpenFileDialogContext.hpp"
#include <commdlg.h>

Win32OpenFileDialogContext::Win32OpenFileDialogContext(HWND parent, declara::app::OpenFileDialogNode *node)
    : parent_(parent), node_(node), visibleState_(0), lastVisible_(false), presenting_(false)
{
  visibleState_ = node_ ? node_->props.isVisible_ : 0;
  if (visibleState_)
  {
    lastVisible_ = visibleState_->get();
    bindVisible();
    applyVisible();
  }
}

Win32OpenFileDialogContext::~Win32OpenFileDialogContext()
{
  unbindVisible();
}

void Win32OpenFileDialogContext::bindVisible()
{
  if (!visibleState_)
  {
    return;
  }
  visibleState_->deferBind(&Win32OpenFileDialogContext::VisibleChangedThunk, this);
}

void Win32OpenFileDialogContext::unbindVisible()
{
  if (!visibleState_)
  {
    return;
  }
  visibleState_->deferUnbind(&Win32OpenFileDialogContext::VisibleChangedThunk, this);
}

void Win32OpenFileDialogContext::applyVisible()
{
  if (!visibleState_)
  {
    return;
  }
  const bool visible = visibleState_->get();
  if (visible && !lastVisible_)
  {
    presentDialog();
  }
  lastVisible_ = visible;
}

void Win32OpenFileDialogContext::presentDialog()
{
  if (presenting_)
  {
    return;
  }
  presenting_ = true;

  char buffer[MAX_PATH];
  buffer[0] = '\0';

  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = parent_;
  ofn.lpstrFile = buffer;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  GetOpenFileNameA(&ofn);

  presenting_ = false;
}

void Win32OpenFileDialogContext::VisibleChangedThunk(void *userData)
{
  Win32OpenFileDialogContext *self = static_cast<Win32OpenFileDialogContext *>(userData);
  if (self)
  {
    self->applyVisible();
  }
}
