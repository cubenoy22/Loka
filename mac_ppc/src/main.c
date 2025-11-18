#include <Types.h>
#include <Windows.h>
#include <QuickDraw.h>
#include <Events.h>
#include <Fonts.h>
#include <TextEdit.h>

void DrawDeveloper(WindowPtr win) {
    SetPort(win);
    MoveTo(20, 40);
    DrawString("Developer");
}

int main(void) {
    WindowPtr win;
    EventRecord event;
    win = NewCWindow(NULL, &(Rect){50, 50, 250, 350}, "Developer", true, documentProc, (WindowPtr)-1, false, 0);
    ShowWindow(win);
    DrawDeveloper(win);
    while (1) {
        WaitNextEvent(everyEvent, &event, 60, NULL);
        if (event.what == updateEvt) {
            BeginUpdate((WindowPtr)event.message);
            DrawDeveloper((WindowPtr)event.message);
            EndUpdate((WindowPtr)event.message);
        } else if (event.what == keyDown && (event.modifiers & cmdKey) && (event.message & charCodeMask) == 'q') {
            break;
        }
    }
    DisposeWindow(win);
    return 0;
}
