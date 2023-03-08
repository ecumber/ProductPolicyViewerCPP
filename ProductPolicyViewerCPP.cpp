// ProductPolicyViewerCPP.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "ProductPolicyViewerCPP.h"
#include "productpolicy.h"
#include <stdio.h>
#include <commctrl.h>
#include <strsafe.h>

#pragma warning( disable : 4995 )

#define MAX_LOADSTRING 100
static ProductPolicyBlob* PPDataBlob;
static int numberofitems = 0;
static bool commandlinemode = false;
HWND listviewwnd;

LSTATUS OpenProductPolicyKey(LPBYTE output) {
    HKEY PPKey = NULL;
    DWORD bytesread = 65536;
    LSTATUS result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, PPRegKeyPath, 0, KEY_READ, &PPKey);
    if (result != ERROR_SUCCESS) goto error;
    result = RegQueryValueExW(PPKey, L"ProductPolicy", 0, NULL, output, &bytesread);
    if (result != ERROR_SUCCESS) goto error;
    return result;
    
error:
    LPWSTR errormsg = new WCHAR[50];
    wsprintfW(errormsg, L"RegOpenKeyExW failed with %d", result);
    MessageBoxW(NULL, errormsg, L"Error", MB_ICONERROR);
    delete[] errormsg;
    ExitProcess(1);
}

#define GetDWORDAtCurrentPos(pos) *reinterpret_cast<DWORD*>(pos)
#define MovePointerForwardByDWORD(pos) pos = (pos + sizeof(DWORD))
#define GetWORDAtCurrentPos(pos) *reinterpret_cast<WORD*>(pos)
#define MovePointerForwardByWORD(pos) pos = (pos + sizeof(WORD))

int ParseProductPolicyData(LPBYTE buffer) {
    if (!buffer)
        return 0;
    BYTE* currentposition = buffer;
    PPDataBlob = new ProductPolicyBlob;
    PPDataBlob->dataheader = new ProductPolicyDataHeader;
    // get header size
    PPDataBlob->dataheader->totalsize = GetDWORDAtCurrentPos(currentposition);
    MovePointerForwardByDWORD(currentposition);
    
    // number of policies
    PPDataBlob->dataheader->valuesize = GetDWORDAtCurrentPos(currentposition);
    int valuesize = PPDataBlob->dataheader->valuesize;

    PPDataBlob->value = new ProductPolicyValue[0x0923]; // upper limit of values
    MovePointerForwardByDWORD(currentposition);
    PPDataBlob->dataheader->endmarkersize = GetDWORDAtCurrentPos(currentposition);

    BYTE* valuepointer = buffer + 0x14;

    int i = 0;
    while (true) {
        if (!valuepointer)
            return 0;
        BYTE* oldvaluepointer = valuepointer;
        PPDataBlob->value[i].header.totalsize = GetWORDAtCurrentPos(valuepointer);
        DWORD* end = reinterpret_cast<DWORD*>((valuepointer) + PPDataBlob->value[i].header.totalsize);
        if (*end == 0x00000045) {
            break;
        }

        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.namesize = GetWORDAtCurrentPos(valuepointer);
        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.datatype = GetWORDAtCurrentPos(valuepointer);
        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.datasize = GetWORDAtCurrentPos(valuepointer);
        MovePointerForwardByWORD(valuepointer);
        PPDataBlob->value[i].header.flags = GetDWORDAtCurrentPos(valuepointer);
        valuepointer = (valuepointer + (2 * sizeof(DWORD)));
        PPDataBlob->value[i].policyname = new WCHAR[PPDataBlob->value[i].header.namesize + 2];
        memset(PPDataBlob->value[i].policyname, 0, PPDataBlob->value[i].header.namesize + 2);

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
            case ProductPolicyValueType::PP_SZ:
                WCHAR* temp = new WCHAR[PPDataBlob->value[i].header.datasize];
                StringCchCopyW(temp, PPDataBlob->value[i].header.datasize, reinterpret_cast<WCHAR*>(valuepointer));
                PPDataBlob->value[i].datavalue = new char[PPDataBlob->value[i].header.datasize / 2];
                wcstombs(PPDataBlob->value[i].datavalue, temp, PPDataBlob->value[i].header.datasize / 2);
                PPDataBlob->value[i].datavalue[PPDataBlob->value[i].header.datasize] = 0; // null terminator
                break;
        }
        valuepointer = oldvaluepointer + PPDataBlob->value[i].header.totalsize;
        i++;
    }
    return i;
}



extern "C" int InitProductPolicyColumns(HWND parentwnd, HINSTANCE hinstance, int numberofitems) {
    INITCOMMONCONTROLSEX* CommonControlSex = new INITCOMMONCONTROLSEX;
    CommonControlSex->dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(CommonControlSex);

    RECT clientrect;
    GetClientRect(parentwnd, &clientrect);


    listviewwnd = CreateWindowEx(WS_EX_CLIENTEDGE, 
        WC_LISTVIEW, 
        L"Test", 
        WS_CHILD | LVS_REPORT | LVS_EDITLABELS, 
        0, 0, 
        clientrect.right - clientrect.left, 
        clientrect.bottom - clientrect.top,
        parentwnd, 
        (HMENU)1811, 
        hinstance, NULL);
    SetWindowText(listviewwnd, L"Leo");

    ListView_SetExtendedListViewStyleEx(listviewwnd, LVS_REPORT | LVS_EX_GRIDLINES, LVS_REPORT | LVS_EX_GRIDLINES);

    LVITEM item = { 0 };
    item.pszText = LPSTR_TEXTCALLBACK; // Sends an LVN_GETDISPINFO message.
    item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    item.cchTextMax = 256;
    LPWSTR text = new WCHAR[16];
    StringCchCopy(text, sizeof("Policy"), L"Policy");

    LVCOLUMN column = { 0 };
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = text;
    column.cx = 360;

    SendMessage(listviewwnd, LVM_INSERTCOLUMN, 0, (LPARAM)&column);

    //type
    StringCchCopy(text, sizeof("Type"), L"Type");
    column.pszText = text;
    column.cx = 128;
    SendMessage(listviewwnd, LVM_INSERTCOLUMN, 1, (LPARAM)&column);
    
    //value
    StringCchCopy(text, sizeof("Value"), L"Value");
    column.pszText = text;
    column.cx = 256;
    SendMessage(listviewwnd, LVM_INSERTCOLUMN, 2, (LPARAM)&column);

    delete[] text;

    for (int i = 0; i < numberofitems; i++) {

        item.iItem = i;
        WCHAR* charbuffer = new WCHAR[PPDataBlob->value[i].header.namesize];
        StringCchCopyW(charbuffer, PPDataBlob->value[i].header.namesize, PPDataBlob->value[i].policyname);
        item.pszText = charbuffer;
        SendMessage(listviewwnd, LVM_INSERTITEM, 0, (LPARAM)&item);

    }    

    for (int i = 0; i < numberofitems; i++) {
        item.iItem = i;
        item.iSubItem = 1;
        WCHAR charbuffer[16] = L"Unknown"; // fallback if it messes up
        switch (PPDataBlob->value[i].header.datatype) {
            case ProductPolicyValueType::PP_BINARY:
                StringCchCopyW(charbuffer, 7, L"Binary");
                break;
            case ProductPolicyValueType::PP_DWORD:
                StringCchCopyW(charbuffer, 6, L"DWORD");
                break;
            case ProductPolicyValueType::PP_SZ:
                StringCchCopyW(charbuffer, 8, L"String");
                break;
        }
        
        item.pszText = charbuffer;
        SendMessage(listviewwnd, LVM_SETITEM, 0, (LPARAM)&item);
    }

    unsigned int datavalue = 0;
    char* bytes = nullptr;
    WCHAR* string = nullptr;

    for (int i = 0; i < numberofitems; i++) {
        item.iItem = i;
        item.iSubItem = 2;
        WCHAR charbuffer[256] = L"Unknown"; // fallback if it messes up
        UINT8 type = static_cast<UINT8>(PPDataBlob->value[i].header.datatype);
        switch (type) {
        case ProductPolicyValueType::PP_BINARY:
            bytes = new char[PPDataBlob->value[i].header.datasize + 1];
            for (int j = 0; j < (PPDataBlob->value[i].header.datasize); j++)
                bytes[j] = *(PPDataBlob->value[i].datavalue + j);
            bytes[PPDataBlob->value[i].header.datasize] = 0;
            wsprintf(charbuffer, L"0x%x", *bytes);
            break;
        case ProductPolicyValueType::PP_DWORD:
            datavalue = *reinterpret_cast<int*>(PPDataBlob->value[i].datavalue);
            wsprintf(charbuffer, L"%u", datavalue);
            break;
        case ProductPolicyValueType::PP_SZ:
            string = new WCHAR[PPDataBlob->value[i].header.datasize];
            mbstowcs(string, PPDataBlob->value[i].datavalue, PPDataBlob->value[i].header.datasize);
            StringCchCopyW(charbuffer, 256, string);
            break;
        }

        item.pszText = charbuffer;
        SendMessage(listviewwnd, LVM_SETITEM, 0, (LPARAM)&item);
    }
    delete[] bytes;
    delete CommonControlSex;

    ShowWindow(listviewwnd, SW_NORMAL);
    return 0;
}

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LPBYTE buffer = new BYTE[65536];
    OpenProductPolicyKey(buffer);
    numberofitems = ParseProductPolicyData(buffer);
    delete[] buffer;

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
    WNDCLASSEXW wcex = { 0 };

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
   HINSTANCE* listhInstance = new HINSTANCE;

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);

   InitProductPolicyColumns(hWnd, hInstance, numberofitems);

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
    case WM_NOTIFY:
        switch (lParam) {
            case LVN_BEGINLABELEDIT:
                MessageBox(NULL, L"Hi", L"Hi", 0);
        }

    case WM_COMMAND:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
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
    case WM_SIZE:
        RECT rect;
        GetClientRect(hWnd, &rect);
        SetWindowPos(listviewwnd, 0, 0, 0, rect.right - rect.left, rect.bottom - rect.top, 0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CommandLineFunc() {
    char temp[320] = { 0 };
    LPBYTE ppbuffer = new BYTE[65536];
    OpenProductPolicyKey(ppbuffer);
    numberofitems = ParseProductPolicyData(ppbuffer);
    delete[] ppbuffer;

    WCHAR datatype[16] = L"Unknown";
    WCHAR* datavalue = new WCHAR[128];
    WCHAR* buffer = new WCHAR[512];

    HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    memset(datavalue, 0, 128);
    memset(buffer, 0, 256);

    for (int i = 0; i < numberofitems; i++) {
        memset(buffer, 0, 256);
        switch (PPDataBlob->value[i].header.datatype) {
        case ProductPolicyValueType::PP_SZ:
            StringCchCopyW(datatype, sizeof(L"String"), L"String");
            StringCchPrintfA(temp, 320, "%s", PPDataBlob->value[i].datavalue);
            break;
        case ProductPolicyValueType::PP_BINARY:
            StringCchCopyW(datatype, sizeof(L"Binary"), L"Binary");
            StringCchPrintfA(temp, 320, "0x%x", *PPDataBlob->value[i].datavalue);
            break;
        case ProductPolicyValueType::PP_DWORD:
            StringCchCopyW(datatype, sizeof(L"DWORD"), L"DWORD");
            StringCchPrintfA(temp, 320, "%d", (DWORD)*PPDataBlob->value[i].datavalue);
            break;

        }
        mbstowcs(datavalue, temp, 256);

        StringCchPrintfW(buffer, 512, L"%s\n\tType: %s\n\tValue: %s\n", PPDataBlob->value[i].policyname, datatype, datavalue);
        wprintf(buffer);

    }
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        FreeConsole();
        wWinMain(GetModuleHandle(NULL), NULL, NULL, 1);
    }
    else
        CommandLineFunc();
}
