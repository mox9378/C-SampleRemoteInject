// InjectExample.cpp : �������̨Ӧ�ó������ڵ㡣


#include "stdafx.h"

int EnableDebugPriv(const wchar_t *name)
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tp;
	LUID luid;

	//�򿪽������ƻ�
	if(NULL == OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES,&hToken))
		return 1;

	//��ý��̱���ΨһID
	if(!LookupPrivilegeValue(NULL,name,&luid))
		return 1;

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	tp.Privileges[0].Luid = luid;
	
	//����Ȩ��
	if(!AdjustTokenPrivileges(hToken,0,&tp,sizeof(TOKEN_PRIVILEGES),NULL,NULL))
		return 1;
	return 0;
}

BOOL InjectDll(const wchar_t* DllFullPath,const DWORD dwRemoteProcessId)
{
	HANDLE hRemoteProcess;
	EnableDebugPriv(SE_DEBUG_NAME);
	//��Զ���߳� 
	hRemoteProcess = OpenProcess(PROCESS_ALL_ACCESS,FALSE,dwRemoteProcessId);
	if(!hRemoteProcess)
	{
		printf("OpenProcess Fail,GetLastError: %d",GetLastError());
		return FALSE;
	}

	void *pszLibFileRemote;
	//ʹ��VirtualAllocEx ������Զ�̽��̵��ڴ��ַ�ռ����DLL�ļ����ռ�
	pszLibFileRemote = VirtualAllocEx(hRemoteProcess,NULL,(wcslen(DllFullPath)+1)*sizeof(wchar_t),MEM_COMMIT,PAGE_READWRITE);
	if(!pszLibFileRemote)
	{
		printf("VirtualAllocEx Fail,GetLastError: %d",GetLastError());
		return FALSE;
	}

	//ʹ��WriteProcessMemory ������DLL��·��д�뵽Զ�̽��̵��ڴ�ռ�
	DWORD dwReceiveSize;
	if(0 == WriteProcessMemory(hRemoteProcess,pszLibFileRemote,(void*)DllFullPath,wcslen(DllFullPath)*sizeof(wchar_t),NULL))
	{
		printf("WriteProcessMemory Fail,GetLastError: %d",GetLastError());
		return FALSE;
	}
	printf("WriteProcessMem Success!\r\n");

	//����LoadLibrary ����ڵ�ַ
	PTHREAD_START_ROUTINE pfnStartAddr = NULL;

//#ifdef _UNICODE
	pfnStartAddr = (PTHREAD_START_ROUTINE)GetProcAddress(::GetModuleHandle(TEXT("Kernel32")),"LoadLibraryW");
//#else
	//pfnStartAddr = (PTHREAD_START_ROUTINE)GetProcAddress(::GetModuleHandle(TEXT("Kernel32")),"LoadLibraryA");
//#endif


	if(NULL == pfnStartAddr)
	{
		printf("GetProcAddress Fail,GetLastError: %d",GetLastError());
		return FALSE;
	}

	//����Զ���߳� LoadLibraryA,ͨ��Զ���̵߳��ô����µ��߳�
	DWORD dwThreadId=0;
	HANDLE hRemoteThread = CreateRemoteThread(hRemoteProcess,NULL,0,pfnStartAddr,pszLibFileRemote,0,NULL);
	if(hRemoteThread == NULL)
	{
		printf("ע���߳�ʧ��,ErrorCode: %d\r\n",GetLastError());
		return FALSE;
	}
	printf("Inject Success ,ProcessId : %d\r\n",dwRemoteProcessId);
	
	WaitForSingleObject(hRemoteThread,INFINITE);
	GetExitCodeThread(hRemoteThread,&dwThreadId);
	//ж�� ע��dll
	pfnStartAddr = (PTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(TEXT("Kernel32")),"FreeLibrary");
	hRemoteThread = CreateRemoteThread(hRemoteProcess,NULL,0,pfnStartAddr,(LPVOID)dwThreadId,0,NULL);
	CloseHandle(hRemoteThread);
	//�ͷ�Զ�̽��̿ؼ�
	VirtualFreeEx(hRemoteProcess,pszLibFileRemote,wcslen(DllFullPath)*sizeof(wchar_t)+1,MEM_DECOMMIT);
	//�ͷž��
	CloseHandle(hRemoteProcess);
	return TRUE;
}

DWORD GetProcessId()
{
	DWORD Pid = -1;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); // ����ϵͳ����

	//����ϵͳ����
	PROCESSENTRY32 lPrs; //���������Ϣ�Ľṹ
	ZeroMemory(&lPrs,sizeof(PROCESSENTRY32));

	lPrs.dwSize = sizeof(lPrs);
	wchar_t *targetFile = L"calc.exe";
	Process32First(hSnap,&lPrs); //ȡ��ϵͳ�����е�һ��������Ϣ
	if(wcsstr(targetFile,lPrs.szExeFile)) // �жϽ�����Ϣ�Ƿ�Ϊexplore.exe
	{
		Pid = lPrs.th32ProcessID;
		return Pid;
	}
	while(1)
	{
		ZeroMemory(&lPrs,sizeof(lPrs));
		lPrs.dwSize = sizeof(lPrs);
		if(!Process32Next(hSnap,&lPrs))
		{
			Pid=-1;
			break;
		}
		if(wcsstr(targetFile,lPrs.szExeFile))
		{
			Pid = lPrs.th32ProcessID;
			break;
		}
	}
	CloseHandle(hSnap);
	return Pid;

}

int _tmain(int argc, _TCHAR* argv[])
{
	wchar_t myFILE[MAX_PATH];
	GetCurrentDirectory(MAX_PATH,myFILE); //��ȡ��ǰ·��
	wcscat_s(myFILE,L"\\InjectDllExample.dll");
	InjectDll(myFILE,GetProcessId());

	return 0;
}

