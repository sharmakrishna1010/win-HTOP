#include <app_state.h>
#include <buffer.h>
#include <cpu_usage.h>
#include <display.h>
#include <io.h>
#include <memory_usage.h>
#include <process.h>
#include <process_info.h>
#include <render.h>
#include <sort.h>
#include <user_info.h>
#include <iostream>
#include <cstdlib>
#include <cstring>

AppState::AppState()
    : width(100), height(25), visibleRows(24), scrollOffset(0), buffer1(nullptr), buffer2(nullptr),
      activeBuffer(nullptr), backBuffer(nullptr), frameBuffer(nullptr), processCount(0),
      cpuCount(0), lastCpuUpdateTime(0), updateCpu(false), sortType(NAME), sortOrder(ASCENDING),
      filter(true), lastKeyTime(0), nowKey(0)
{
    prevSysTime = {};
    currentSysTime = {};
}

void getConsoleSize(HANDLE hActive, short *width, short *height)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hActive, &csbi))
    {
        *width = static_cast<short>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        *height = static_cast<short>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
    }
    else
    {
        *width = 100;
        *height = 25;
    }
}

void initializeState(AppState &state)
{
    state.buffer1 = createConsoleBuffer();
    state.buffer2 = createConsoleBuffer();
    state.activeBuffer = state.buffer1;
    state.backBuffer = state.buffer2;

    getConsoleSize(state.activeBuffer, &state.width, &state.height);
    state.visibleRows = state.height - 1;
    state.frameBuffer = createFrameBuffer(state.width, state.height);

    COORD size = {state.width, state.height};
    SMALL_RECT window = {0, 0, static_cast<SHORT>(state.width - 1), static_cast<SHORT>(state.height - 1)};

    SetConsoleScreenBufferSize(state.activeBuffer, size);
    SetConsoleWindowInfo(state.activeBuffer, TRUE, &window);
    setConsoleBufferActive(state.activeBuffer);

    state.cpuCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    getSystemTime(&state.currentSysTime);
    state.prevSysTime = state.currentSysTime;
    processList.clear();
    state.lastCpuUpdateTime = 0;
    state.updateCpu = false;
    state.sortType = NAME;
    state.sortOrder = ASCENDING;
    state.filter = true;
    state.lastKeyTime = 0;
    state.nowKey = GetTickCount();
}

void resizeConsoleState(AppState &state)
{
    getConsoleSize(state.activeBuffer, &state.width, &state.height);
    state.visibleRows = state.height - 1;

    if (state.frameBuffer)
    {
        delete[] state.frameBuffer;
    }
    state.frameBuffer = createFrameBuffer(state.width, state.height);

    COORD newSize = {state.width, state.height};
    SMALL_RECT newWindow = {0, 0, static_cast<SHORT>(state.width - 1), static_cast<SHORT>(state.height - 1)};

    auto safeResize = [&](HANDLE hBuf)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hBuf, &csbi);

        bool shrinking = (state.width < csbi.dwSize.X) || (state.height < csbi.dwSize.Y);

        if (shrinking)
        {
            if (!SetConsoleWindowInfo(hBuf, TRUE, &newWindow))
            {
                SMALL_RECT minimal = {0, 0, 1, 1};
                SetConsoleWindowInfo(hBuf, TRUE, &minimal);
                SetConsoleWindowInfo(hBuf, TRUE, &newWindow);
            }
            SetConsoleScreenBufferSize(hBuf, newSize);
        }
        else
        {
            SetConsoleScreenBufferSize(hBuf, newSize);
            SetConsoleWindowInfo(hBuf, TRUE, &newWindow);
        }
    };

    safeResize(state.buffer1);
    safeResize(state.buffer2);
}

void updateInputState(AppState &state, bool &shouldExit)
{
    if (GetAsyncKeyState(VK_UP) & 0x8000)
        state.scrollOffset--;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        state.scrollOffset++;
    if (GetAsyncKeyState('Q') & 0x8000)
    {
        shouldExit = true;
        return;
    }

    HANDLE handleInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD events;
    GetNumberOfConsoleInputEvents(handleInput, &events);

    while (events > 0)
    {
        INPUT_RECORD record;
        DWORD read;
        ReadConsoleInput(handleInput, &record, 1, &read);
        events--;

        if (record.EventType == KEY_EVENT)
        {
            state.nowKey = GetTickCount();

            if (record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode == 'F' && state.nowKey - state.lastKeyTime > 200)
            {
                state.filter = !state.filter;
                state.lastKeyTime = state.nowKey;
            }
            if (record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode == 'P' && state.nowKey - state.lastKeyTime > 200)
                state.sortType = PID;
            if (record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode == 'M' && state.nowKey - state.lastKeyTime > 200)
                state.sortType = MEMORY;
            if (record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode == 'C' && state.nowKey - state.lastKeyTime > 200)
                state.sortType = CPU;
            if (record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode == 'N' && state.nowKey - state.lastKeyTime > 200)
                state.sortType = NAME;
            if (record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode == 'A' && state.nowKey - state.lastKeyTime > 200)
                state.sortOrder = ASCENDING;
            if (record.Event.KeyEvent.bKeyDown && record.Event.KeyEvent.wVirtualKeyCode == 'D' && state.nowKey - state.lastKeyTime > 200)
                state.sortOrder = DESCENDING;
        }

        if (record.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            resizeConsoleState(state);
        }

        if (record.EventType == MOUSE_EVENT)
        {
            MOUSE_EVENT_RECORD &mouse = record.Event.MouseEvent;
            if (mouse.dwEventFlags == MOUSE_WHEELED)
            {
                SHORT delta = HIWORD(mouse.dwButtonState);
                if (delta > 0)
                    state.scrollOffset--;
                else
                    state.scrollOffset++;

                int maxScroll = static_cast<int>(processList.size()) - state.visibleRows;
                if (maxScroll < 0)
                    maxScroll = 0;
                if (state.scrollOffset > maxScroll)
                    state.scrollOffset = maxScroll;
            }
        }
    }
}

void refreshProcessState(AppState &state)
{
    getProcessIDList(state.processIDArray, sizeof(state.processIDArray), &state.processCount);

    if (state.filter)
        filterProcessArray(state.processIDArray, state.processCount);

    ULONGLONG now = GetTickCount64();
    state.updateCpu = (now - state.lastCpuUpdateTime > 1000);
    if (state.updateCpu)
        getSystemTime(&state.currentSysTime);

    oldList = processList;
    processList.clear();

    DWORD selfPid = GetCurrentProcessId();
    bool found = false;

    for (unsigned long i = 0; i < state.processCount; i++)
    {
        if (state.processIDArray[i] == selfPid)
        {
            found = true;
            break;
        }
    }

    if (!found && state.processCount < 4096)
    {
        state.processIDArray[state.processCount++] = selfPid;
    }

    for (unsigned long i = 0; i < state.processCount; i++)
    {
        ProcessInfo p{};
        p.pid = state.processIDArray[i];
        processList.push_back(p);
        ProcessInfo &newProc = processList.back();

        bool foundOld = false;
        for (auto &oldProc : oldList)
        {
            if (oldProc.pid == newProc.pid)
            {
                strcpy(newProc.name, oldProc.name);
                strcpy(newProc.userName, oldProc.userName);
                strcpy(newProc.userGroup, oldProc.userGroup);
                newProc.prevCpuTime = oldProc.prevCpuTime;
                newProc.cpuUsage = oldProc.cpuUsage;
                foundOld = true;
                break;
            }
        }

        if (!foundOld)
        {
            strcpy(newProc.name, "[system]");
            fetchProcessName(processList.size() - 1);
            fetchUserInfo(processList.size() - 1);
            fetchTimeUsage(processList.size() - 1);
            newProc.prevCpuTime = newProc.currCpuTime;
            newProc.cpuUsage = 0.0;
        }

        fetchMemoryUsage(processList.size() - 1);
        fetchPriority(processList.size() - 1);

        if (state.updateCpu)
        {
            fetchTimeUsage(processList.size() - 1);
            fetchCpuUsage(processList.size() - 1, state.cpuCount, &state.prevSysTime, &state.currentSysTime);
            newProc.prevCpuTime = newProc.currCpuTime;
        }
    }

    sortProcessList(processList, state.sortType, state.sortOrder);

    if (state.scrollOffset < 0)
        state.scrollOffset = 0;
    int maxScroll = static_cast<int>(processList.size()) - state.visibleRows;
    if (maxScroll < 0)
        maxScroll = 0;
    if (state.scrollOffset > maxScroll)
        state.scrollOffset = maxScroll;

    if (state.updateCpu)
    {
        state.prevSysTime = state.currentSysTime;
        state.lastCpuUpdateTime = now;
    }
}

void renderProcessState(AppState &state)
{
    clearFrameBuffer(state.frameBuffer, state.width, state.height);

    paintFrame(state.frameBuffer, state.width, 0, 0, (char *)"PID");
    paintFrame(state.frameBuffer, state.width, 0, 10, (char *)"Name");
    paintFrame(state.frameBuffer, state.width, 0, 50, (char *)"User");
    paintFrame(state.frameBuffer, state.width, 0, 70, (char *)"Group");
    paintFrame(state.frameBuffer, state.width, 0, 90, (char *)"Memory");
    paintFrame(state.frameBuffer, state.width, 0, 100, (char *)"CPU");
    paintFrame(state.frameBuffer, state.width, 0, 110, (char *)"Priority");

    for (int row = 0; row < state.visibleRows; row++)
    {
        int idx = row + state.scrollOffset;
        if (idx >= static_cast<int>(processList.size()))
            break;

        char pidStr[16];
        sprintf(pidStr, "%lu", processList[idx].pid);

        char memStrMB[16];
        sprintf(memStrMB, "%llu", static_cast<unsigned long long>(processList[idx].memoryMB));

        char cpuUsageStr[16];
        sprintf(cpuUsageStr, "%.2f", processList[idx].cpuUsage);

        char priorityStr[16];
        priorityToString(priorityStr, idx);

        paintFrame(state.frameBuffer, state.width, row + 1, 0, pidStr);
        paintFrame(state.frameBuffer, state.width, row + 1, 10, processList[idx].name);
        paintFrame(state.frameBuffer, state.width, row + 1, 50, processList[idx].userName);
        paintFrame(state.frameBuffer, state.width, row + 1, 70, processList[idx].userGroup);
        paintFrame(state.frameBuffer, state.width, row + 1, 90, memStrMB);
        paintFrame(state.frameBuffer, state.width, row + 1, 95, (char *)"MB");
        paintFrame(state.frameBuffer, state.width, row + 1, 100, (char *)cpuUsageStr);
        paintFrame(state.frameBuffer, state.width, row + 1, 110, (char *)priorityStr);
    }

    writeFrameToConsoleBuffer(state.backBuffer, state.frameBuffer, state.width, state.height);
}

void destroyState(AppState &state)
{
    if (state.frameBuffer)
        delete[] state.frameBuffer;
    if (state.buffer1)
        CloseHandle(state.buffer1);
    if (state.buffer2)
        CloseHandle(state.buffer2);
}
