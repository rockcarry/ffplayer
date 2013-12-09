// playerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "player.h"
#include "playerDlg.h"
extern "C" {
#include "../coreplayer/coreplayer.h"
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCREEN_WIDTH   800
#define SCREEN_HEIGHT  480

#define TIMER_ID_FIRST_DIALOG  1
#define TIMER_ID_PROGRESS      2

// 全局变量定义
static HANDLE g_hplayer = NULL;

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
        playerclose(g_hplayer);
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

    // invalidate rect
    InvalidateRect(NULL, TRUE);
    m_nPosXCur = 0;

    // player open file
    g_hplayer = playeropen(str, GetSafeHwnd());
    if (g_hplayer)
    {
        playergetparam(g_hplayer, PARAM_GET_DURATION, &m_nTimeTotal);
        playersetrect(g_hplayer, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 2);
        playerplay(g_hplayer);
        SetTimer(TIMER_ID_PROGRESS, 500, NULL);
    }
}

BEGIN_MESSAGE_MAP(CplayerDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_LBUTTONDOWN()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


// CplayerDlg message handlers

BOOL CplayerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
    RECT rect;
    int  w, h;
    MoveWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    GetClientRect(&rect);
    w = SCREEN_WIDTH  + (SCREEN_WIDTH  - rect.right);
    h = SCREEN_HEIGHT + (SCREEN_HEIGHT - rect.bottom);
    MoveWindow(0, 0, w, h);

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
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
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
}

void CplayerDlg::OnTimer(UINT_PTR nIDEvent)
{
    RECT fill;
    int  pos;

    switch (nIDEvent)
    {
    case TIMER_ID_FIRST_DIALOG:
        // kill timer first
        KillTimer(TIMER_ID_FIRST_DIALOG);
        PlayerOpenFile();
        break;

    case TIMER_ID_PROGRESS:
        playergetparam(g_hplayer, PARAM_GET_POSITION, &pos);
        fill.left   = 0;
        fill.right  = (LONG)(SCREEN_WIDTH * pos / m_nTimeTotal);
        fill.top    = SCREEN_HEIGHT - 2;
        fill.bottom = SCREEN_HEIGHT;
        m_nPosXCur  = fill.right;
        m_pDrawDC->FillSolidRect(&fill, RGB(250, 200, 0));
        break;

    default:
        CDialog::OnTimer(nIDEvent);
        break;
    }
}

void CplayerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    RECT fill;
    if (point.y > SCREEN_HEIGHT - 5)
    {
        if (point.x < m_nPosXCur) {
            fill.left   = point.x;
            fill.right  = SCREEN_WIDTH;
            fill.top    = SCREEN_HEIGHT - 2;
            fill.bottom = SCREEN_HEIGHT;
            m_nPosXCur  = point.x;
            m_pDrawDC->FillSolidRect(&fill, 0);
        }
        else
        {
            fill.left   = m_nPosXCur;
            fill.right  = point.x;
            fill.top    = SCREEN_HEIGHT - 2;
            fill.bottom = SCREEN_HEIGHT;
            m_nPosXCur  = point.x;
            m_pDrawDC->FillSolidRect(&fill, RGB(250, 200, 0));
        }
        playerseek(g_hplayer, m_nTimeTotal * point.x / SCREEN_WIDTH);
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

BOOL CplayerDlg::PreTranslateMessage(MSG *pMsg) 
{
    if (pMsg->message == MSG_COREPLAYER)
    {
        switch (pMsg->wParam)
        {
        case PLAYER_STOP:
            PlayerOpenFile();
            break;
        }
        return TRUE;
    }
    else return CDialog::PreTranslateMessage(pMsg);
}

