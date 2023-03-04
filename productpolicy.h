#pragma once
#include <windows.h>
#include <winnt.h>

const LPCWSTR PPRegKeyPath = L"SYSTEM\\CurrentControlSet\\Control\\ProductOptions";


// https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/ex/slmem/productpolicy.htm

struct ProductPolicyDataHeader {
	DWORD totalsize = 0;
	DWORD numberofvalues = 0;
	DWORD endmarkersize = 0;
	DWORD unknown = 0;
	const DWORD mustbeone = 1;
};

struct ProductPolicyValueHeader {
	WORD totalsize = 0;
	WORD namesize = 0;
	WORD datatype;
	WORD datasize = 0;
	DWORD flags = 0;
	DWORD unknown = 0;
};

enum ProductPolicyValueType : WORD {
	PP_NONE = 0,
	PP_SZ = 1,
	PP_BINARY = 3,
	PP_DWORD = 4,
	PP_MULTI_SZ = 7,
};

struct ProductPolicyValue {
	ProductPolicyValueHeader header;
	WCHAR* policyname;
	BYTE* value;
};

struct ProductPolicyBlob {
	ProductPolicyDataHeader* dataheader;
	ProductPolicyValue* value;
};