#pragma once

#include <Windows.h>
#include <map>
#include <string>

enum
{
    WM_CALLBACK = WM_USER + 12,     // ����ͼ��Ļص���Ϣ
    WM_REMOVE_NOTIFY,               // ֪ͨ����������ĳ���̣�wparamΪid��lparamΪhwnd
};

enum
{
    CMD_ABOUT = 1,          // ����
    CMD_DESTROYONSHOW,      // �����Ƿ��ڴ�����ʾʱ��������ͼ��
    CMD_SHOWBALLOON,        // �����Ƿ���ʾ����
    CMD_HIDEONMINIMAZE,     // �����Ƿ�����С��ʱ�Զ�����
    CMD_PINWINDOW,          // ָ������
    CMD_SWITCHVISIABLE,     // ��ʾ�����ش���
    CMD_DETAIL,             // �鿴������ϸ��Ϣ
    CMD_DESTROYICON,        // ��������ͼ��

    CMD_ADD_WINDOW,         // ���ɴ���
    CMD_HIDE_WINDOW,        // ���ɲ����ش���
    CMD_COPY_HWNDVALUE,     // ���Ƶ������壬���ֵ
    CMD_COPY_CLASSNAME,     // ���Ƶ������壬��������
    CMD_COPY_WINDOWTEXT,    // ���Ƶ������壬���ڱ���
    CMD_COPY_CHILDTREE,     // ���Ƶ������壬�Ӵ�����
    CMD_COPY_PROCESSID,     // ���Ƶ������壬����id
    CMD_COPY_IMAGEPATH,     // ���Ƶ������壬����·��
    CMD_COPY_IMAGENAME,     // ���Ƶ������壬������
    CMD_COPY_THREADID,      // ���Ƶ������壬�߳�id
    CMD_ALPHA_100,          // ��͸���ȣ�100%
    CMD_ALPHA_80,           // ��͸���ȣ�80%
    CMD_ALPHA_60,           // ��͸���ȣ�60%
    CMD_ALPHA_40,           // ��͸���ȣ�40%
    CMD_ALPHA_20,           // ��͸���ȣ�20%
    CMD_OPEN_IMAGE_PATH,    // �򿪽�������Ŀ¼
    CMD_KILL_WINDOW_PROCESS,// ɱ�����ڽ���
    CMD_UNGROUPING_WINDOW,  // �������������飬��Դ���
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

    // �õ�һ����ǰ�ɲ���Ĵ���
    // ���������������棬����ǰ�ģ�����ͬһ�����ڣ�����ͬһ�����ڵ�����С����
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
