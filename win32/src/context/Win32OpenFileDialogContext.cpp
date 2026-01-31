#include "Win32OpenFileDialogContext.hpp"
#include <commdlg.h>

Win32OpenFileDialogContext::Win32OpenFileDialogContext(HWND parent, loka::app::OpenFileDialogNode *node)
    : parent_(parent),
      node_(node),
      visibleState_(0),
      resultState_(0),
      onResult_(0),
      lastVisible_(false),
      presenting_(false)
{
  visibleState_ = node_ ? node_->props.isVisible_ : 0;
  resultState_ = node_ ? node_->props.result_ : 0;
  onResult_ = node_ ? node_->props.onResult_ : 0;
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

  if (GetOpenFileNameA(&ofn))
  {
    loka::file::File file = loka::file::File::FromPath(loka::core::String(buffer));
    file.setKind(loka::file::File::KIND_FILE);
    setResult(loka::app::FileChooserResult::File(file));
  }
  else
  {
    DWORD error = CommDlgExtendedError();
    if (error != 0)
    {
      setResult(loka::app::FileChooserResult::Error(static_cast<int>(error)));
    }
    else
    {
      setResult(loka::app::FileChooserResult::Canceled());
    }
  }

  presenting_ = false;
}

void Win32OpenFileDialogContext::setResult(const loka::app::FileChooserResult &result)
{
  if (resultState_)
  {
    resultState_->set(result, true);
  }
  if (onResult_)
  {
    onResult_->emit();
  }
}

void Win32OpenFileDialogContext::VisibleChangedThunk(void *userData)
{
  Win32OpenFileDialogContext *self = static_cast<Win32OpenFileDialogContext *>(userData);
  if (self)
  {
    self->applyVisible();
  }
}
