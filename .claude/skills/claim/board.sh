#!/usr/bin/env bash
# Mirror a claimed issue onto the Loka Board (GitHub Project "Loka Board",
# Lihi-AI #1) so people and other sessions can see who works on what.
# The claim directory stays the lock; the board is only the visible view.
# Closing the issue or merging its PR moves the card to Done (board workflows).
#
# Usage: board.sh start <issue-number> <owner-slug>
set -u

PROJECT_OWNER=Lihi-AI
PROJECT_NUMBER=1
PROJECT_ID=PVT_kwHOEZDf884BlJH6
STATUS_FIELD=PVTSSF_lAHOEZDf884BlJH6zhj3HmQ
STATUS_IN_PROGRESS=47fc9ee4
SESSION_FIELD=PVTF_lAHOEZDf884BlJH6zhj3Hps
REPO_URL=https://github.com/cubenoy22/Loka

if [ $# -ne 3 ] || [ "$1" != start ]; then
  echo "usage: $0 start <issue-number> <owner-slug>" >&2
  exit 2
fi
issue=$2
owner=$3

# A board failure never undoes or blocks the claim: warn and exit 0.
item=$(gh project item-add "$PROJECT_NUMBER" --owner "$PROJECT_OWNER" \
  --url "$REPO_URL/issues/$issue" --format json --jq .id 2>/dev/null) || {
  echo "board: could not add #$issue (needs gh 'project' scope); claim still holds" >&2
  exit 0
}
gh project item-edit --id "$item" --project-id "$PROJECT_ID" --field-id "$STATUS_FIELD" \
  --single-select-option-id "$STATUS_IN_PROGRESS" >/dev/null 2>&1 &&
gh project item-edit --id "$item" --project-id "$PROJECT_ID" --field-id "$SESSION_FIELD" \
  --text "$owner" >/dev/null 2>&1 ||
  echo "board: added #$issue but could not set its fields" >&2
echo "board: #$issue In Progress ($owner)"
