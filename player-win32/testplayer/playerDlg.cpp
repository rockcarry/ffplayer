// playerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "player.h"
#include "playerDlg.h"
extern "C" {
#include "../ffplayer/coreplayer.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define TIMER_ID_FIRST_DIALOG  1
#define TIMER_ID_PROGRESS      2

// 全局变量定义
static void *g_hplayer = NULL;

// CplayerDlg dialog


CplayerDlg::CplayerDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CplayerDlg::IDD, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CplayerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

void CplayerDlg::PlayerOpenFile(void)
{
    CFileDialog dlg(TRUE);
    TCHAR       path[MAX_PATH];
    char        str [MAX_PATH];

    // kill player progress timer
    KillTimer(TIMER_ID_PROGRESS);

    // stop player first
    if (g_hplayer)
    {
        player_close(g_hplayer);
        g_hplayer = NULL;
    }

    // open file dialog
    if (dlg.DoModal() == IDOK)
    {
        wcscpy_s(path, dlg.GetPathName());
        WideCharToMultiByte(CP_ACP, 0, path, -1, str, MAX_PATH, NULL, NULL);
    }
    else
    {
        OnOK();
        return;
    }

    // player open file
    g_hplayer = player_open(str, GetSafeHwnd());
    if (g_hplayer)
    {
        m_bPlayPause = FALSE;

        /*
        int speed = 150;
        player_setparam(g_hplayer, PARAM_PLAYER_SPEED, &speed);
        */

        // software volume scale -30dB to 12dB
        // range for volume is [-182, 73]
        // -255 - mute, +255 - max volume, 0 - 0dB
        int volume = -0;
        player_setparam(g_hplayer, PARAM_AUDIO_VOLUME, &volume);

        player_setrect(g_hplayer, 0, 0, 0, m_rtClient.right, m_rtClient.bottom - 2);
        player_setrect(g_hplayer, 1, 0, 0, m_rtClient.right, m_rtClient.bottom - 2);
        player_play(g_hplayer);
        SetTimer(TIMER_ID_PROGRESS, 100, NULL);
    }
}

BEGIN_MESSAGE_MAP(CplayerDlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_LBUTTONDOWN()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
    ON_COMMAND(ID_VIDEO_MODE , &CplayerDlg::OnVideoMode )
    ON_COMMAND(ID_EFFECT_MODE, &CplayerDlg::OnEffectMode)
END_MESSAGE_MAP()


// CplayerDlg message handlers

BOOL CplayerDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    // load accelerators
    m_hAcc = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR1)); 

    // TODO: Add extra initialization here
    MoveWindow(0, 0, 800, 480);

    m_pDrawDC = GetDC();
    SetTimer(TIMER_ID_FIRST_DIALOG, 100, NULL);

    return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CplayerDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width () - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CPaintDC dc(this);

        LONGLONG total = 1, pos = 0;
        player_getparam(g_hplayer, PARAM_MEDIA_DURATION, &total);
        player_getparam(g_hplayer, PARAM_MEDIA_POSITION, &pos  );
        if (pos > 0) {
            RECT fill  = m_rtClient;
            fill.right = (LONG)(fill.right * pos / total);
            fill.top   = fill.bottom - 2;
            dc.FillSolidRect(&fill, RGB(250, 150, 0));
            fill.left  = fill.right;
            fill.right = m_rtClient.right;
            dc.FillSolidRect(&fill, RGB(0, 0, 0));
        }

        CDialog::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CplayerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CplayerDlg::OnDestroy()
{
    CDialog::OnDestroy();
    ReleaseDC(m_pDrawDC);

    // close player
    if (g_hplayer)
    {
        player_close(g_hplayer);
        g_hplayer = NULL;
    }
}

void CplayerDlg::OnTimer(UINT_PTR nIDEvent)
{
    switch (nIDEvent)
    {
    case TIMER_ID_FIRST_DIALOG:
        // kill timer first
        KillTimer(TIMER_ID_FIRST_DIALOG);
        PlayerOpenFile();
        break;

    case TIMER_ID_PROGRESS:
        RECT rect;
        rect.top    = m_rtClient.bottom - 2;
        rect.left   = m_rtClient.left;
        rect.bottom = m_rtClient.bottom;
        rect.right  = m_rtClient.right;
        InvalidateRect(&rect, FALSE);
        break;

    default:
        CDialog::OnTimer(nIDEvent);
        break;
    }
}

void CplayerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (point.y > m_rtClient.bottom - 8)
    {
        LONGLONG total = 1;
        player_getparam(g_hplayer, PARAM_MEDIA_DURATION, &total);
        player_seek(g_hplayer, total * point.x / m_rtClient.right);
    }
    else {
        if (!m_bPlayPause) player_pause(g_hplayer);
        else player_play(g_hplayer);
        m_bPlayPause = !m_bPlayPause;
    }

    CDialog::OnLButtonDown(nFlags, point);
}

HBRUSH CplayerDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    // TODO: Change any attributes of the DC here
    // TODO: Return a different brush if the default is not desired
    if (pWnd == this) return (HBRUSH)GetStockObject(BLACK_BRUSH);
    else return hbr;
}

void CplayerDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    if (nType != SIZE_MINIMIZED) {
        GetClientRect(&m_rtClient);
        player_setrect(g_hplayer, 0, 0, 0, cx, cy - 2);
        player_setrect(g_hplayer, 1, 0, 0, cx, cy - 2);
    }
}

BOOL CplayerDlg::PreTranslateMessage(MSG *pMsg) 
{
    if (TranslateAccelerator(GetSafeHwnd(), m_hAcc, pMsg)) return TRUE;

    if (pMsg->message == MSG_COREPLAYER)
    {
        switch (pMsg->wParam)
        {
        case PLAY_COMPLETED:
            PlayerOpenFile();
            break;
        }
        return TRUE;
    }
    else return CDialog::PreTranslateMessage(pMsg);
}

void CplayerDlg::OnVideoMode()
{
    int mode = 0;
    player_getparam(g_hplayer, PARAM_VIDEO_MODE, &mode);
    mode++; mode %= VIDEO_MODE_MAX_NUM;
    player_setparam(g_hplayer, PARAM_VIDEO_MODE, &mode);
}

void CplayerDlg::OnEffectMode()
{
    int mode = 0;
    player_getparam(g_hplayer, PARAM_VISUAL_EFFECT, &mode);
    mode++; mode %= VISUAL_EFFECT_MAX_NUM;
    player_setparam(g_hplayer, PARAM_VISUAL_EFFECT, &mode);
}
