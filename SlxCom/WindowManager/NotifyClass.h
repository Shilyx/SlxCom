#pragma once

#include <Windows.h>
#include <map>
#include <string>

enum
{
    WM_CALLBACK = WM_USER + 12,     // 托盘图标的回调消息
    WM_REMOVE_NOTIFY,               // 通知主窗口销毁某托盘，wparam为id，lparam为hwnd
};

enum
{
    CMD_ABOUT = 1,          // 关于
    CMD_DESTROYONSHOW,      // 控制是否在窗口显示时销毁托盘图标
    CMD_SHOWBALLOON,        // 控制是否显示气球
    CMD_HIDEONMINIMAZE,     // 控制是否在最小化时自动隐藏
    CMD_PINWINDOW,          // 指定窗口
    CMD_SWITCHVISIABLE,     // 显示或隐藏窗口
    CMD_DETAIL,             // 查看窗口详细信息
    CMD_DESTROYICON,        // 销毁托盘图标

    CMD_ADD_WINDOW,         // 收纳窗口
    CMD_HIDE_WINDOW,        // 收纳并隐藏窗口
    CMD_COPY_HWNDVALUE,     // 复制到剪贴板，句柄值
    CMD_COPY_CLASSNAME,     // 复制到剪贴板，窗口类名
    CMD_COPY_WINDOWTEXT,    // 复制到剪贴板，窗口标题
    CMD_COPY_CHILDTREE,     // 复制到剪贴板，子窗口树
    CMD_COPY_PROCESSID,     // 复制到剪贴板，进程id
    CMD_COPY_IMAGEPATH,     // 复制到剪贴板，进程路径
    CMD_COPY_IMAGENAME,     // 复制到剪贴板，进程名
    CMD_COPY_THREADID,      // 复制到剪贴板，线程id
    CMD_ALPHA_100,          // 不透明度，100%
    CMD_ALPHA_80,           // 不透明度，80%
    CMD_ALPHA_60,           // 不透明度，60%
    CMD_ALPHA_40,           // 不透明度，40%
    CMD_ALPHA_20,           // 不透明度，20%
    CMD_OPEN_IMAGE_PATH,    // 打开进程所在目录
    CMD_KILL_WINDOW_PROCESS,// 杀死窗口进程
    CMD_UNGROUPING_WINDOW,  // 禁用任务栏分组，针对窗口
};

class CNotifyClass
{
public:
    CNotifyClass(HWND hManagerWindow, HWND hTargetWindow, UINT uId, LPCWSTR lpCreateTime, BOOL bShowBalloonThisTime);
    ~CNotifyClass();

public:
    UINT GetId() const;
    HWND GetTargetWindow() const;
    void SwitchVisiable();
    void DoContextMenu(int x, int y);
    std::wstring GetTargetWindowFullInfo();
    void RefreshInfo();
    std::wstring GetTargetWindowCaption();
    std::wstring GetTargetWindowBaseInfo();
    static void DoPin(HWND hTargetWindow);

    // 得到一个当前可捕获的窗口
    // 满足条件：非桌面，是最前的，不在同一进程内，或在同一进程内但可最小化的
    static HWND GetCaptureableWindow();
    static std::map<HWND, std::wstring> GetAndClearLastNotifys();
    static BOOL IsOptionHideOnMinimaze();

private:
    void RecordToRegistry();
    void RemoveFromRegistry();
    void ReleaseTargetWindow();
    static DWORD CALLBACK PinWindowProc(LPVOID lpParam);

private:
    BOOL m_bUseNotifyIcon;
    HWND m_hManagerWindow;
    HWND m_hTargetWindow;
    NOTIFYICONDATAW m_nid;
    HICON m_hIcon;
    HICON m_hIconSm;
    std::wstring m_strCreateTime;

private:
    static BOOL ms_bDestroyOnShow;
    static BOOL ms_bShowBalloon;
    static BOOL ms_bHideOnMinimaze;
};
