// ProductPolicyViewerConsole.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include "productpolicy.h"
#include <strsafe.h>
#include <ProductPolicyViewerCPP/productpolicy.h>

#define GetDWORDAtCurrentPos(pos) *reinterpret_cast<DWORD*>(pos)
#define MovePointerForwardByDWORD(pos) pos = (pos + sizeof(DWORD))
#define GetWORDAtCurrentPos(pos) *reinterpret_cast<WORD*>(pos)
#define MovePointerForwardByWORD(pos) pos = (pos + sizeof(WORD))

static int numberofitems = 0;

static ProductPolicyBlob* PPDataBlob;

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
        DWORD* end = reinterpret_cast<DWORD*>((valuepointer)+PPDataBlob->value[i].header.totalsize);
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
            if (PPDataBlob->value[i].datavalue[(PPDataBlob->value[i].header.datasize / 2) - 1] != 0) {
                PPDataBlob->value[i].datavalue[PPDataBlob->value[i].header.datasize / 2] = 0;
            }
            break;
        }
        valuepointer = oldvaluepointer + PPDataBlob->value[i].header.totalsize;
        i++;
    }
    return i;
}


int main()
{
    char* temp = new char[320];
    LPBYTE ppbuffer = new BYTE[65536];
    OpenProductPolicyKey(ppbuffer);
    numberofitems = ParseProductPolicyData(ppbuffer);
    delete[] ppbuffer;
    
    WCHAR datatype[16] = L"Unknown";
    WCHAR* datavalue = new WCHAR[2048];
    
    HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    
    for (int i = 0; i < numberofitems; i++) {
        switch (PPDataBlob->value[i].header.datatype) {
        case ProductPolicyValueType::PP_SZ:
            StringCchCopyW(datatype, sizeof(L"String"), L"String");
            StringCchPrintfA(temp, 256, "%s", PPDataBlob->value[i].datavalue);
            break;
        case ProductPolicyValueType::PP_BINARY:
            StringCchCopyW(datatype, sizeof(L"Binary"), L"Binary");
            StringCchPrintfA(temp, 256, "0x%x", *PPDataBlob->value[i].datavalue);
            break;
        case ProductPolicyValueType::PP_DWORD:
            StringCchCopyW(datatype, sizeof(L"DWORD"), L"DWORD");
            StringCchPrintfA(temp, 256, "%u", (DWORD)*PPDataBlob->value[i].datavalue);
            break;
    
        }
        mbstowcs(datavalue, temp, 256);
        wprintf(L"%d: %s\n\tType: %s\n\tValue: %s\n", i, PPDataBlob->value[i].policyname, datatype, datavalue);
    }
    delete[] datavalue, temp;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
