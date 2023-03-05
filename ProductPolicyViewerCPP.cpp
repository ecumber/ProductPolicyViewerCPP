// ProductPolicyViewerCPP.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "ProductPolicyViewerCPP.h"
#include "productpolicy.h"
#include <stdio.h>

#define MAX_LOADSTRING 100

static ProductPolicyBlob* PPDataBlob;

LSTATUS OpenProductPolicyKey(LPBYTE output) {
    HKEY PPKey = NULL;
    LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, PPRegKeyPath, 0, KEY_READ, &PPKey);
    DWORD bytesread;
    result = RegQueryValueExW(PPKey, L"ProductPolicy", 0, NULL, output, &bytesread);
    return result;
    
}

#define GetDWORDAtCurrentPos(pos) *reinterpret_cast<DWORD*>(pos)
#define MovePointerForwardByDWORD(pos) pos = (pos + sizeof(DWORD))
#define GetWORDAtCurrentPos(pos) *reinterpret_cast<WORD*>(pos)
#define MovePointerForwardByWORD(pos) pos = (pos + sizeof(WORD))

void ParseProductPolicyData(LPBYTE buffer) {
    if (!buffer)
        return;
    BYTE* currentposition = buffer;
    PPDataBlob = new ProductPolicyBlob;
    PPDataBlob->dataheader = new ProductPolicyDataHeader;
    // get header size
    PPDataBlob->dataheader->totalsize = GetDWORDAtCurrentPos(currentposition);
    MovePointerForwardByDWORD(currentposition);
    
    // number of policies
    PPDataBlob->dataheader->numberofvalues = GetDWORDAtCurrentPos(currentposition);
    int numvalues = PPDataBlob->dataheader->numberofvalues;

    PPDataBlob->value = new ProductPolicyValue[PPDataBlob->dataheader->numberofvalues];
    MovePointerForwardByDWORD(currentposition);
    PPDataBlob->dataheader->endmarkersize = GetDWORDAtCurrentPos(currentposition);

    BYTE* valuepointer = buffer + 0x14;

    for (int i = 0; i < numvalues; i++) {
        BYTE* oldvaluepointer = valuepointer;
        PPDataBlob->value[i].header.totalsize = GetWORDAtCurrentPos(valuepointer);
        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.namesize = GetWORDAtCurrentPos(valuepointer);
        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.datatype = GetWORDAtCurrentPos(valuepointer);
        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.datasize = GetWORDAtCurrentPos(valuepointer);
        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.flags = GetDWORDAtCurrentPos(valuepointer);
        valuepointer = (valuepointer + (2 * sizeof(DWORD)));
        PPDataBlob->value[i].policyname = (WCHAR*)calloc(PPDataBlob->value[i].header.namesize + 2, 1);

        wcsncpy(PPDataBlob->value[i].policyname, reinterpret_cast<WCHAR*>(valuepointer), (size_t)(PPDataBlob->value[i].header.namesize / 2));
        valuepointer = (valuepointer + PPDataBlob->value[i].header.namesize);

        switch (PPDataBlob->value[i].header.datatype) {
            case ProductPolicyValueType::PP_DWORD:
                PPDataBlob->value[i].datavalue = new char[sizeof(DWORD)];
                for (int j = 0; j <= sizeof(DWORD); j++) {
                    PPDataBlob->value[i].datavalue[j] = *(valuepointer + j);
                }
                break;
            case ProductPolicyValueType::PP_BINARY:
                PPDataBlob->value[i].datavalue = new char[PPDataBlob->value[i].header.datasize];
                for (int j = 0; j <= (PPDataBlob->value[i].header.datasize - 1); j++) {
                    PPDataBlob->value[i].datavalue[j] = *(valuepointer + j);
                }
                break;
        }
        valuepointer = oldvaluepointer + PPDataBlob->value[i].header.totalsize;
    }

}

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    LPBYTE buffer = (LPBYTE)malloc(65536);
    OpenProductPolicyKey(buffer);
    ParseProductPolicyData(buffer);


    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PRODUCTPOLICYVIEWERCPP, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PRODUCTPOLICYVIEWERCPP));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PRODUCTPOLICYVIEWERCPP));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_PRODUCTPOLICYVIEWERCPP);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}