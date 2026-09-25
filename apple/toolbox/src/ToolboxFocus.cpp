/** Controller focus reads and key delivery, shared with the host fixture. */
ToolboxEditTextContext *ToolboxScenePlatformController::fallbackFocusContext() const
{
  loka::app::FocusParticipant *row = loka::app::FocusParticipant::from(this->fallbackFocus_.peerRow());
  // Only fallback EditText hits connect this source slot.
  return row ? static_cast<ToolboxEditTextContext *>(row->context()) : 0;
}

bool ToolboxScenePlatformController::readNativeFocus(loka::app::scene::NodeContext *&out)
{
  out = 0;
  if (!this->window_ || !this->window_->window() || FrontWindow() != this->window_->window())
    return false;
  EditTextControlBinding *native = this->editControls_.focused();
  out = native ? native->ownerContext : this->fallbackFocusContext();
  return true;
}

bool ToolboxScenePlatformController::handleKeyDown(char key)
{
  EditTextControlBinding *focusedEdit = this->editControls_.focused();
  if (focusedEdit && focusedEdit->te)
  {
    if (focusedEdit->editor)
    {
      focusedEdit->editor->key(key);
      return true;
    }
    this->beginBatchUpdate();
    TEKey(key, focusedEdit->te);
    this->updateStateFromEdit(*focusedEdit);
    this->endBatchUpdate();
    return true;
  }
  this->beginBatchUpdate();
  if (!this->handleTextKey(key))
  {
    this->endBatchUpdate();
    return false;
  }
  this->endBatchUpdate();
  return true;
}

bool ToolboxScenePlatformController::handleTextKey(char key)
{
  ToolboxEditTextContext *context = this->fallbackFocusContext();
  loka::core::State<loka::core::String> *text = context ? context->projectedTextState() : 0;
  if (!text)
  {
    return false;
  }
  std::string utf8;
  loka::platform::CollectUtf8(text->get(), utf8);
  if (key == 8 || key == 0x7F)
  {
    if (!utf8.empty())
    {
      utf8.erase(utf8.size() - 1);
    }
  }
  else if (key == 13)
  {
    return true;
  }
  else if (key >= 32)
  {
    utf8.push_back(key);
  }
  else
  {
    return false;
  }
  const loka::app::scene::WriteSeat<loka::core::String> seat = context->projectedWriteSeat();
  if (!seat.isValid())
    return false;
  seat.set(loka::core::String(utf8));
  return true;
}

bool ToolboxScenePlatformController::handleMouseDown(const Point &point)
{
  if (this->handleControlClick(point))
  {
    return false;
  }
  if (this->handleEditClick(point))
    return true;
  for (size_t i = 0; i < this->hitLedger_.editHits_.size(); ++i)
  {
    EditHit &hit = this->hitLedger_.editHits_[i];
    if (hit.text && PtInRect(point, &hit.rect))
    {
      loka::app::FocusParticipant *row =
          hit.context && hit.context->owner()
              ? loka::app::FocusParticipant::from(hit.context->owner()->asFocusParticipant())
              : 0;
      if (!row)
        continue;
      row->connectSource(this->fallbackFocus_);
      return true;
    }
  }
  this->fallbackFocus_.cut();
  for (size_t i = 0; i < this->hitLedger_.popupHits_.size(); ++i)
  {
    PopupHit &hit = this->hitLedger_.popupHits_[i];
    if (hit.context && PtInRect(point, &hit.rect) && hit.context->handleMouseDown(point, this))
    {
      return false;
    }
  }
  for (size_t i = 0; i < this->hitLedger_.cellHits_.size(); ++i)
  {
    CellHit &hit = this->hitLedger_.cellHits_[i];
    if (hit.context && PtInRect(point, &hit.rect) && hit.context->handleMouseDown(point, this))
    {
      return false;
    }
  }
  for (size_t i = 0; i < this->hitLedger_.buttonHits_.size(); ++i)
  {
    ButtonHit &hit = this->hitLedger_.buttonHits_[i];
    if (hit.context && PtInRect(point, &hit.rect) && hit.context->handleMouseDown(point, this))
    {
      return false;
    }
  }
  return false;
}

void ToolboxScenePlatformController::recordEditHit(const Rect &rect,
                                                   loka::core::State<loka::core::String> *text,
                                                   loka::app::scene::BoundaryNode *boundary,
                                                   ToolboxEditTextContext *context)
{
  Rect clipped;
  if (!this->intersectWithProjectionClip(rect, clipped))
  {
    if (this->fallbackFocusContext() == context)
      this->fallbackFocus_.cut();
    return;
  }
  EditHit hit;
  hit.context = context;
  hit.rect = clipped;
  hit.text = text;
  hit.boundary = boundary;
  this->hitLedger_.editHits_.push_back(hit);
  this->bindTextState(text);
}
