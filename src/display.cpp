#include <windows.h>
#include <cstring>
#include <app_state.h>
#include <buffer.h>
#include <display.h>
#include <io.h>

void priorityToString(char *priorStr, unsigned long index)
{
    if (processList[index].priority == ProcessInfo::IDLE)
        strcpy(priorStr, "Idle");
    else if (processList[index].priority == ProcessInfo::BELOW_NORMAL)
        strcpy(priorStr, "Below Normal");
    else if (processList[index].priority == ProcessInfo::NORMAL)
        strcpy(priorStr, "Normal");
    else if (processList[index].priority == ProcessInfo::ABOVE_NORMAL)
        strcpy(priorStr, "Above Normal");
    else if (processList[index].priority == ProcessInfo::HIGH)
        strcpy(priorStr, "High");
    else if (processList[index].priority == ProcessInfo::REALTIME)
        strcpy(priorStr, "Realtime");
    else if (processList[index].priority == ProcessInfo::UNKNOWN)
        strcpy(priorStr, "??");
}

bool mainLoop()
{
    AppState state;
    initializeState(state);

    while (true)
    {
        bool shouldExit = false;
        updateInputState(state, shouldExit);
        if (shouldExit)
            break;

        refreshProcessState(state);
        renderProcessState(state);

        setConsoleBufferActive(state.backBuffer);
        hideConsoleCursor(state.activeBuffer);
        enableMouseInput();

        HANDLE tmp = state.activeBuffer;
        state.activeBuffer = state.backBuffer;
        state.backBuffer = tmp;

        Sleep(50);
    }

    destroyState(state);
    return true;
}

bool allocateConsole()
{
    if (GetConsoleCP())
    {
        return true;
    }
    else
    {
        BOOL newConsole = AllocConsole();
        if (newConsole)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}
