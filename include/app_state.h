#pragma once
#include <windows.h>
#include <process_info.h>
#include <sort.h>

void priorityToString(char *priorStr, unsigned long index);

struct AppState
{
    short width;
    short height;
    int visibleRows;
    int scrollOffset;
    HANDLE buffer1;
    HANDLE buffer2;
    HANDLE activeBuffer;
    HANDLE backBuffer;
    CHAR_INFO *frameBuffer;
    unsigned long processIDArray[4096];
    unsigned long processCount;
    FILETIME prevSysTime;
    FILETIME currentSysTime;
    int cpuCount;
    ULONGLONG lastCpuUpdateTime;
    bool updateCpu;
    SortType sortType;
    SortOrder sortOrder;
    bool filter;
    DWORD lastKeyTime;
    DWORD nowKey;

    AppState();
};

void getConsoleSize(HANDLE hActive, short *width, short *height);
void initializeState(AppState &state);
void resizeConsoleState(AppState &state);
void updateInputState(AppState &state, bool &shouldExit);
void refreshProcessState(AppState &state);
void renderProcessState(AppState &state);
void destroyState(AppState &state);
